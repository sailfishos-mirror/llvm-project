//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include <sycl/__impl/exception.hpp>

#include <detail/device_impl.hpp>
#include <detail/offload/offload_utils.hpp>
#include <detail/program_manager.hpp>

#include <cstring>

_LIBSYCL_BEGIN_NAMESPACE_SYCL
namespace detail {

static inline bool checkFatBinVersion(const __sycl_tgt_bin_desc &FatbinDesc) {
  return FatbinDesc.Version == _LIBSYCL_SUPPORTED_OFFLOAD_BINARY_VERSION;
}

static inline bool
checkDeviceImageValidness(const __sycl_tgt_device_image &DeviceImage) {
  return (DeviceImage.Version == _LIBSYCL_SUPPORTED_DEVICE_BINARY_VERSION) &&
         (DeviceImage.OffloadKind == OFK_SYCL) &&
         (DeviceImage.ImageFormat == IMG_SPIRV);
}

void ProgramManager::addImages(__sycl_tgt_bin_desc *FatbinDesc) {
  assert(FatbinDesc && "Device images descriptor can't be nullptr");

  if (!checkFatBinVersion(*FatbinDesc))
    throw sycl::exception(sycl::make_error_code(sycl::errc::runtime),
                          "Incompatible version of device images descriptor.");
  if (!FatbinDesc->NumDeviceBinaries)
    return;

  std::lock_guard<std::mutex> Guard(MImageCollectionMutex);
  for (int I = 0; I < FatbinDesc->NumDeviceBinaries; I++) {
    const auto &RawDeviceImage = FatbinDesc->DeviceImages[I];
    if (!checkDeviceImageValidness(RawDeviceImage))
      throw sycl::exception(sycl::make_error_code(sycl::errc::runtime),
                            "Incompatible device image.");

    const EntryTy *EntriesB = RawDeviceImage.EntriesBegin;
    const EntryTy *EntriesE = RawDeviceImage.EntriesEnd;
    // Ignore "empty" device image.
    if (EntriesB == EntriesE)
      continue;

    std::unique_ptr<DeviceImageWrapper> NewImageWrapper =
        std::make_unique<DeviceImageWrapper>(RawDeviceImage);

    for (auto EntriesIt = EntriesB; EntriesIt != EntriesE; EntriesIt++) {
      auto Name = EntriesIt->SymbolName;
      auto KernelIDIt = MKernelNameToID.find(Name);
      if (KernelIDIt == MKernelNameToID.end()) {
        sycl::kernel_id KernelID =
            detail::createSyclObjFromImpl<sycl::kernel_id>(
                std::make_shared<detail::KernelIdImpl>(Name));
        KernelIDIt = MKernelNameToID.insert(
            MKernelNameToID.end(),
            std::make_pair(std::string_view(Name), KernelID));
      }

      MKernelIDToDevImageJIT.insert(
          std::make_pair(KernelIDIt->second, NewImageWrapper.get()));
    }

    MDeviceImageWrappers.insert(
        std::make_pair(&RawDeviceImage, std::move(NewImageWrapper)));
  }
}

void ProgramManager::removeImages(__sycl_tgt_bin_desc *FatbinDesc) {
  assert(FatbinDesc && "Device images descriptor can't be nullptr");

  if (!checkFatBinVersion(*FatbinDesc))
    throw sycl::exception(sycl::make_error_code(sycl::errc::runtime),
                          "Incompatible version of device images descriptor.");
  if (FatbinDesc->NumDeviceBinaries == 0)
    return;

  std::lock_guard<std::mutex> Guard(MImageCollectionMutex);
  for (int I = 0; I < FatbinDesc->NumDeviceBinaries; I++) {
    const auto &RawDeviceImage = FatbinDesc->DeviceImages[I];

    auto DevImageIt = MDeviceImageWrappers.find(&RawDeviceImage);
    if (DevImageIt == MDeviceImageWrappers.end())
      continue;

    const EntryTy *EntriesB = RawDeviceImage.EntriesBegin;
    const EntryTy *EntriesE = RawDeviceImage.EntriesEnd;
    // Ignore "empty" device image
    if (EntriesB == EntriesE)
      continue;

    for (auto EntriesIt = EntriesB; EntriesIt != EntriesE; EntriesIt++) {
      if (auto KernelIDIt = MKernelNameToID.find(EntriesIt->SymbolName);
          KernelIDIt != MKernelNameToID.end()) {
        MKernelIDToDevImageJIT.erase(KernelIDIt->second);
        MKernelNameToID.erase(KernelIDIt);
      }
    }

    MDeviceImageWrappers.erase(DevImageIt);
  }
}

static bool isImageTargetCompatible(const DeviceImageWrapper &Image,
                                    const DeviceImpl &Device) {
  sycl::backend BE = Device.getBackend();
  const char *Target = Image.getRawData().TripleString;

  return (strcmp(Target, _LIBSYCL_DEVICE_BINARY_TARGET_SPIRV64) == 0) &&
         (BE == sycl::backend::level_zero);
}

DeviceImageWrapper *ProgramManager::getDeviceImage(const char *KernelName,
                                                   kernel_id KernelID,
                                                   DeviceImpl &Device) {
  auto [Begin, End] = MKernelIDToDevImageJIT.equal_range(KernelID);
  if (Begin != End) {
    ol_result_t Result{};
    bool IsValid{};
    // TODO: with AOT (not implemented yet), we need to analize and check
    // olIsValidBinary for AOT binaries first.
    for (auto It = Begin; It != End; ++It) {
      if (isImageTargetCompatible(*It->second, Device)) {
        callAndThrow(olIsValidBinary, Device.getHandle(),
                     It->second->getRawData().ImageStart, It->second->getSize(),
                     &IsValid);
        if (IsValid)
          return It->second;
      }
    }
  }

  throw exception(make_error_code(errc::runtime),
                  "No kernel named " + std::string(KernelName) + " was found");
}

} // namespace detail
_LIBSYCL_END_NAMESPACE_SYCL

extern "C" void __sycl_register_lib(__sycl_tgt_bin_desc *FatbinDesc) {
  sycl::detail::ProgramManager::getInstance().addImages(FatbinDesc);
}

extern "C" void __sycl_unregister_lib(__sycl_tgt_bin_desc *FatbinDesc) {
  sycl::detail::ProgramManager::getInstance().removeImages(FatbinDesc);
}
