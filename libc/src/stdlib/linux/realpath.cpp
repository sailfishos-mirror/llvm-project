//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Implementation of POSIX realpath.
///
//===----------------------------------------------------------------------===//

#include "src/stdlib/realpath.h"
#include "hdr/errno_macros.h"
#include "hdr/limits_macros.h"
#include "hdr/types/size_t.h"
#include "src/__support/CPP/optional.h"
#include "src/__support/CPP/string.h"
#include "src/__support/CPP/string_view.h"
#include "src/__support/OSUtil/linux/syscall_wrappers/getcwd.h"
#include "src/__support/OSUtil/linux/syscall_wrappers/readlink.h"
#include "src/__support/OSUtil/path.h"
#include "src/__support/common.h"
#include "src/__support/error_or.h"
#include "src/__support/libc_errno.h"
#include "src/__support/macros/config.h"
#include "src/string/memory_utils/inline_memcpy.h"

namespace LIBC_NAMESPACE_DECL {
namespace {

#ifdef SYMLOOP_MAX
constexpr size_t MAX_SYMLINK_TRAVERSALS = SYMLOOP_MAX;
#else
constexpr size_t MAX_SYMLINK_TRAVERSALS = 40;
#endif

// Container for a fully resolved, canonical path.
//
// The contained path is always in its canonical form. It is:
// - Absolute
// - Symlink-free
// - Without a trailing separator
// - Devoid of path traversals like "." or ".."
class ResolvedPath {
public:
  ResolvedPath() { set_to_root(); }

  void set_to_root() { path_ = path::SEPARATOR; }

  cpp::optional<Error> set_to_cwd() {
    char buf[PATH_MAX];
    ErrorOr<ssize_t> ret = linux_syscalls::getcwd(buf, PATH_MAX);
    if (!ret) {
      if (ret.error() == ERANGE)
        return Error(ENAMETOOLONG);
      return Error(ret.error());
    }

    if (*ret <= 0)
      return Error(EIO);

    path_ = cpp::string_view(buf, *ret - 1);
    return cpp::nullopt;
  }

  // Removes the trailing path component.
  void set_to_parent() {
    size_t sep_index = cpp::string_view(path_).find_last_of(path::SEPARATOR);

    // Never move past the root separator. For example,
    // ensures that set_to_parent on "/hello" only resizes to "/".
    path_.resize(sep_index >= 1 ? sep_index : 1);
  }

  // Adds a single component to the end of this path.
  cpp::optional<Error> push_component(cpp::string_view component) {
    if (!path::is_root(path_)) {
      if (cpp::optional<Error> err = push_raw(path::SEPARATOR); err)
        return err;
    }

    return push_raw(component);
  }

  // Releases ownership of the underlying C-string and resets this path.
  //
  // Must be free'd by the caller.
  char *release() { return path_.release_c_str(); }

  const char *c_str() const { return path_.c_str(); }

  // Copies the content of this path to `dst`.
  void copy_to(char *dst) {
    inline_memcpy(dst, path_.c_str(), path_.size() + 1);
  }

private:
  cpp::optional<Error> push_raw(cpp::string_view value) {
    // -1 because PATH_MAX includes a null-terminator.
    size_t remaining_bytes = (PATH_MAX - 1) - path_.size();
    if (value.size() > remaining_bytes)
      return Error(ENAMETOOLONG);

    path_ += value;
    return cpp::nullopt;
  }

  cpp::optional<Error> push_raw(char c) {
    return push_raw(cpp::string_view(&c, 1));
  }

  cpp::string path_;
};

// Container for a string that can be quickly prepended.
template <size_t N> class PrependableString {
public:
  LIBC_INLINE size_t size() const { return N - start; }

  LIBC_INLINE bool empty() const { return size() == 0; }

  // Takes a view of the first prefix_length characters from this string.
  // The view is only valid until the next mutating call to PrependableString.
  LIBC_INLINE cpp::string_view take_prefix(size_t prefix_length) {
    if (prefix_length >= size())
      prefix_length = size();

    cpp::string_view view(buf + start, prefix_length);
    start += prefix_length;
    return view;
  }

  // Discards the first prefix_length characters from this string.
  LIBC_INLINE void discard_prefix(size_t prefix_length) {
    take_prefix(prefix_length);
  }

  // Prepends c to this string. Returns false if there was insufficient space.
  LIBC_INLINE bool prepend(char c) { return prepend(cpp::string_view(&c, 1)); }

  // Prepends src to this string. Returns false if there was insufficient space.
  LIBC_INLINE bool prepend(cpp::string_view src) {
    if (src.size() > start)
      return false;

    start -= src.size();
    inline_memcpy(buf + start, src.data(), src.size());
    return true;
  }

  // A view over this string.
  LIBC_INLINE cpp::string_view view() const {
    return cpp::string_view(buf + start, N - start);
  }

private:
  size_t start = N;
  char buf[N];
};

// A view over path components yet to be processed by realpath.
//
// When `realpath("./a/../b")` is called, the input path can be viewed as
// a stack of components, where components closest to the root are at the top.
// For example:
//
//   ```
//   PendingPath p("./a/..");
//   assert(p.advance_component() == ".");
//   assert(p.advance_component() == "a");
//
//   p.prepend_components("b/c");
//   assert(p.advance_component() == "b");
//   assert(p.advance_component() == "c");
//   assert(p.advance_component() == "..");
//   assert(p.empty());
//   ```
class PendingPath {
public:
  // Whether all path components have been consumed.
  LIBC_INLINE bool empty() const { return path.empty(); }

  // Takes the next path component,
  // starting with the component closest to the root.
  LIBC_INLINE cpp::string_view advance_component() {
    const size_t offset = path.view().find_first_not_of(path::SEPARATOR);
    path.discard_prefix(offset);

    const size_t length = path.view().find_first_of(path::SEPARATOR);
    return path.take_prefix(length);
  }

  // Prepends components to this PendingPath, adding a separator if needed.
  LIBC_INLINE cpp::optional<Error> prepend_path(cpp::string_view target) {
    bool needs_separator =
        !target.ends_with(path::SEPARATOR) && !target.empty() &&
        !path.view().starts_with(path::SEPARATOR) && !path.empty();

    if (needs_separator) {
      if (!path.prepend(path::SEPARATOR))
        return Error(ENAMETOOLONG);
    }

    if (!path.prepend(target))
      return Error(ENAMETOOLONG);

    return cpp::nullopt;
  }

private:
  PrependableString<PATH_MAX> path;
};

// A buffer for calls to `readlink`.
class ReadlinkBuffer {
public:
  // Calls readlink and returns a view into this buffer.
  // The view is only valid until the next mutating call to ReadlinkBuffer.
  ErrorOr<cpp::string_view> readlink(const char *path) {
    ErrorOr<ssize_t> bytes_written =
        linux_syscalls::readlink(path, buffer, sizeof(buffer));
    if (!bytes_written)
      return Error(bytes_written.error());

    if (*bytes_written <= 0)
      return Error(EIO); // Should not be possible, check to guard underflow.

    cpp::string_view target(buffer, static_cast<size_t>(*bytes_written));
    if (target.size() >= sizeof(buffer))
      return Error(ENAMETOOLONG);

    return target;
  }

private:
  char buffer[PATH_MAX];
};

cpp::optional<Error> resolve_path(cpp::string_view path,
                                  ResolvedPath &resolved_path) {
  PendingPath pending_path;
  if (cpp::optional<Error> err = pending_path.prepend_path(path); err)
    return err;

  ReadlinkBuffer target_buf;
  size_t symlinks_followed = 0;

  while (!pending_path.empty()) {
    cpp::string_view component = pending_path.advance_component();
    if (component.empty() || component == path::CURRENT_DIR_COMPONENT)
      continue;

    if (component == path::PARENT_DIR_COMPONENT) {
      resolved_path.set_to_parent();
      continue;
    }

    if (cpp::optional<Error> err = resolved_path.push_component(component); err)
      return err;

    // Try to read the path as a symlink.
    ErrorOr<cpp::string_view> target =
        target_buf.readlink(resolved_path.c_str());
    if (!target) {
      // Failure with EINVAL is okay, it indicates the path was not a symlink.
      if (target.error() == EINVAL)
        continue;
      return Error(target.error());
    }

    symlinks_followed += 1;
    if (symlinks_followed > MAX_SYMLINK_TRAVERSALS)
      return Error(ELOOP);

    // If we had a symlink, we need to process all its components,
    // so prepend it to pending_path for subsequent iterations.
    if (cpp::optional<Error> err = pending_path.prepend_path(*target); err)
      return err;

    // Strip the symlink component from the resolved path.
    resolved_path.set_to_parent();

    // If the symlink pointed to an absolute path, reset to root.
    if (path::is_absolute(*target))
      resolved_path.set_to_root();
  }

  return cpp::nullopt;
}

ErrorOr<char *> realpath_impl(const char *__restrict path_cstr,
                              char *__restrict resolved_path_buf) {
  if (path_cstr == nullptr)
    return Error(EINVAL);

  cpp::string_view path(path_cstr);
  if (path.size() == 0)
    return Error(ENOENT);

  if (path.size() >= PATH_MAX)
    return Error(ENAMETOOLONG);

  ResolvedPath resolved_path;
  if (!path::is_absolute(path)) {
    if (cpp::optional<Error> err = resolved_path.set_to_cwd(); err)
      return *err;
  }

  if (cpp::optional<Error> err = resolve_path(path, resolved_path); err)
    return *err;

  if (resolved_path_buf != nullptr) {
    resolved_path.copy_to(resolved_path_buf);
    return resolved_path_buf;
  }
  return resolved_path.release();
}

} // namespace

LLVM_LIBC_FUNCTION(char *, realpath,
                   (const char *__restrict path,
                    char *__restrict resolved_path)) {
  ErrorOr<char *> res = realpath_impl(path, resolved_path);
  if (!res) {
    libc_errno = res.error();
    return nullptr;
  }
  return *res;
}

} // namespace LIBC_NAMESPACE_DECL
