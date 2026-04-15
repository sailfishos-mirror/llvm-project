//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the declaration of the class that aggregates information
/// specific to device kernels (i.e. information that is uniform between
/// different submissions of the same kernel).
///
//===----------------------------------------------------------------------===//

#ifndef _LIBSYCL_DEVICE_KERNEL_INFO
#define _LIBSYCL_DEVICE_KERNEL_INFO

#include <sycl/__impl/detail/config.hpp>

#include <OffloadAPI.h>

_LIBSYCL_BEGIN_NAMESPACE_SYCL
namespace detail {

class ProgramAndKernelManager;

// TODO: Pointers to instances of this class are supported to be stored in
// header function templates as a static variable to avoid repeated runtime
// lookup overhead.
class DeviceKernelInfo {
public:
  /// Constructs a device kernel info instance.
  ///
  /// \param KernelName the name of the kernel.
  /// \param DeviceImage the device image containing device code of this kernel.
  DeviceKernelInfo(std::string_view KernelName, DeviceImageManager &DeviceImage)
      : MName(KernelName), MDeviceImage(DeviceImage) {}

  /// \return the name of this kernel.
  std::string_view getName() { return MName; }

private:
  std::unordered_map<ol_device_handle_t, ol_symbol_handle_t> MBuiltKernels;

  std::string_view MName;
  DeviceImageManager &MDeviceImage;

  /// Searches for the existing kernel handle compatible with the specified
  /// device.
  /// \param Device the device the kernel must be compatible with.
  /// \return a liboffload kernel handle if a built kernel was found; otherwise
  /// returns nullptr.
  ol_symbol_handle_t getKernel(ol_device_handle_t Device) const {
    if (auto KernelIt = MBuiltKernels.find(Device);
        KernelIt != MBuiltKernels.end())
      return KernelIt->second;
    return nullptr;
  }

  /// \return the device image containing the device code of this kernel.
  DeviceImageManager &getDeviceImage() const { return MDeviceImage; }

  /// Attaches a liboffload kernel handle to this device kernel info object.
  /// \param Device the device the kernel symbol was created for.
  /// \param Kernel the liboffload kernel symbol to attach.
  void addKernel(ol_device_handle_t Device, ol_symbol_handle_t Kernel) {
    assert(Kernel && Device &&
           MBuiltKernels.find(Device) == MBuiltKernels.end());
    MBuiltKernels.insert({Device, Kernel});
  }

  /// Kernel info update is intended to be done only by ProgramAndKernelManager.
  friend class ProgramAndKernelManager;
};

} // namespace detail

_LIBSYCL_END_NAMESPACE_SYCL

#endif // _LIBSYCL_DEVICE_KERNEL_INFO
