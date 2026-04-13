//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// to add
///
//===----------------------------------------------------------------------===//

#ifndef _LIBSYCL_DEVICE_KERNEL_INFO
#define _LIBSYCL_DEVICE_KERNEL_INFO

#include <sycl/__impl/detail/config.hpp>

#include <OffloadAPI.h>

_LIBSYCL_BEGIN_NAMESPACE_SYCL
namespace detail {

// This class aggregates information specific to device kernels (i.e.
// information that is uniform between different submissions of the same
// kernel). Pointers to instances of this class are stored in header function
// templates as a static variable to avoid repeated runtime lookup overhead.
class DeviceKernelInfo {
public:
  DeviceKernelInfo(std::string_view KernelName, DeviceImageManager &DeviceImage)
      : MName(KernelName), MDeviceImage(DeviceImage) {}

  ol_symbol_handle_t getKernel(ol_device_handle_t Device) {
    if (auto KernelIt = MBuiltKernels.find(Device);
        KernelIt != MBuiltKernels.end())
      return KernelIt->second;
    return nullptr;
  }

  DeviceImageManager &getDeviceImage() { return MDeviceImage; }
  std::string_view getName() { return MName; }

  void addKernel(ol_device_handle_t Device, ol_symbol_handle_t Kernel) {
    assert(MBuiltKernels.find(Device) != MBuiltKernels.end());
    MBuiltKernels.insert({Device, Kernel});
  }

private:
  std::unordered_map<ol_device_handle_t, ol_symbol_handle_t> MBuiltKernels;

  std::string_view MName;
  DeviceImageManager &MDeviceImage;
};

} // namespace detail

_LIBSYCL_END_NAMESPACE_SYCL

#endif // _LIBSYCL_DEVICE_KERNEL_INFO
