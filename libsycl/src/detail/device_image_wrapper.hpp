//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the declaration of the helpers for device images and
/// programs.
///
//===----------------------------------------------------------------------===//

#ifndef _LIBSYCL_DEVICE_IMAGE_WRAPPER
#define _LIBSYCL_DEVICE_IMAGE_WRAPPER

#include <sycl/__impl/detail/config.hpp>

#include <detail/device_binary_structures.hpp>

#include <OffloadAPI.h>

#include <map>

_LIBSYCL_BEGIN_NAMESPACE_SYCL
namespace detail {

class DeviceImageManager;

/// A wrapper of liboffload program handle to manage its lifetime.
class ProgramWrapper {
public:
  /// Constructs ProgramWrapper by creating liboffload program with the provided
  /// arguments.
  ///
  /// \param Device is a device to use for program creation.
  /// \param DevImage is a device image (wrapped __sycl_tgt_device_image) to use
  /// for program creation.
  /// \throw sycl::exception with sycl::errc::runtime when failed to create
  /// program.
  ProgramWrapper(ol_device_handle_t Device, DeviceImageManager &DevImage);

  /// Releases the corresponding liboffload program handle by calling
  /// olDestroyProgram.
  ~ProgramWrapper();

  ProgramWrapper(const ProgramWrapper &) = delete;
  ProgramWrapper &operator=(const ProgramWrapper &) = delete;
  ProgramWrapper(ProgramWrapper &&) = delete;
  ProgramWrapper &operator=(ProgramWrapper &&) = delete;

  /// \return the corresponding liboffload program handle.
  ol_program_handle_t getOLHandle() { return MProgram; }

private:
  ol_program_handle_t MProgram{};
};

/// This class manages all work with device images: from data parsing to program
/// creation.
class DeviceImageManager {
public:
  DeviceImageManager(const __sycl_tgt_device_image &Bin) : MBin(&Bin) {}
  // Explicitly delete copy constructor/operator= to avoid unintentional copies.
  DeviceImageManager(const DeviceImageManager &) = delete;
  DeviceImageManager &operator=(const DeviceImageManager &) = delete;

  DeviceImageManager(DeviceImageManager &&) = default;
  DeviceImageManager &operator=(DeviceImageManager &&) = default;

  ~DeviceImageManager() = default;

  /// \return a reference to the corresponding raw __sycl_tgt_device_image
  /// object.
  const __sycl_tgt_device_image &getRawData() const { return *get(); }

  /// \return the size of the corresponding device image data in bytes.
  size_t getSize() const {
    return static_cast<size_t>(MBin->ImageEnd - MBin->ImageStart);
  }

  ///  Returns liboffload program handle by lookup of existing programs or by
  ///  creation of a new one from this image.
  /// \param DeviceHandle liboffload handle of device the program must be
  /// compatible with.
  /// \return liboffload handle of the program compatible with specified device.
  ol_program_handle_t getOrCreateProgram(ol_device_handle_t DeviceHandle) {
    auto ProgramIt = MPrograms.find(DeviceHandle);
    if (ProgramIt == MPrograms.end()) {
      ProgramIt =
          MPrograms.emplace_hint(ProgramIt, std::piecewise_construct,
                                 std::forward_as_tuple(DeviceHandle),
                                 std::forward_as_tuple(DeviceHandle, *this));
    }

    return ProgramIt->second.getOLHandle();
  }

protected:
  std::unordered_map<ol_device_handle_t, ProgramWrapper> MPrograms;

  const __sycl_tgt_device_image *get() const { return MBin; }

  __sycl_tgt_device_image const *MBin{};
};

} // namespace detail

_LIBSYCL_END_NAMESPACE_SYCL

#endif // _LIBSYCL_DEVICE_IMAGE_WRAPPER
