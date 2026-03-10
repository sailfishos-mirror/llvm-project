; ModuleID = '/usr/local/google/home/stevenperron/spirv/llvm/texture-2d-implementation/clang/test/SemaHLSL/Texture2D-mips-errors.hlsl'
source_filename = "/usr/local/google/home/stevenperron/spirv/llvm/texture-2d-implementation/clang/test/SemaHLSL/Texture2D-mips-errors.hlsl"
target datalayout = "e-m:e-ve-p:32:32-i1:32-i8:8-i16:16-i32:32-i64:64-f16:16-f32:32-f64:64-n8:16:32:64"
target triple = "dxilv1.0-pc-shadermodel6.0-library"

%"class.hlsl::Texture2D" = type { target("dx.Texture", <4 x float>, 0, 0, 0, 2), %"struct.hlsl::Texture2D<>::mips_type" }
%"struct.hlsl::Texture2D<>::mips_type" = type { target("dx.Texture", <4 x float>, 0, 0, 0, 2) }
%"struct.hlsl::Texture2D<>::mips_slice_type" = type { target("dx.Texture", <4 x float>, 0, 0, 0, 2), i32 }

@_ZL1t = internal global %"class.hlsl::Texture2D" poison, align 4
@.str = private unnamed_addr constant [2 x i8] c"t\00", align 1
@llvm.global_ctors = appending global [1 x { i32, ptr, ptr }] [{ i32, ptr, ptr } { i32 65535, ptr @_GLOBAL__sub_I_Texture2D_mips_errors.hlsl, ptr null }]

; Function Attrs: alwaysinline convergent nounwind
define internal void @__cxx_global_var_init() #0 {
entry:
  call void @_ZN4hlsl9Texture2DIDv4_fE27__createFromImplicitBindingEjjijPKc(ptr dead_on_unwind writable sret(%"class.hlsl::Texture2D") align 4 @_ZL1t, i32 noundef 0, i32 noundef 0, i32 noundef 1, i32 noundef 0, ptr noundef @.str) #3
  ret void
}

; Function Attrs: alwaysinline convergent mustprogress norecurse nounwind
define linkonce_odr hidden void @_ZN4hlsl9Texture2DIDv4_fE27__createFromImplicitBindingEjjijPKc(ptr dead_on_unwind noalias writable sret(%"class.hlsl::Texture2D") align 4 %agg.result, i32 noundef %orderId, i32 noundef %spaceNo, i32 noundef %range, i32 noundef %index, ptr noundef %name) #1 align 2 {
entry:
  %result.ptr = alloca ptr, align 4
  %orderId.addr = alloca i32, align 4
  %spaceNo.addr = alloca i32, align 4
  %range.addr = alloca i32, align 4
  %index.addr = alloca i32, align 4
  %name.addr = alloca ptr, align 4
  %tmp = alloca %"class.hlsl::Texture2D", align 4
  store ptr %agg.result, ptr %result.ptr, align 4
  store i32 %orderId, ptr %orderId.addr, align 4
  store i32 %spaceNo, ptr %spaceNo.addr, align 4
  store i32 %range, ptr %range.addr, align 4
  store i32 %index, ptr %index.addr, align 4
  store ptr %name, ptr %name.addr, align 4
  call void @_ZN4hlsl9Texture2DIDv4_fEC1Ev(ptr noundef nonnull align 4 dereferenceable(8) %tmp) #3
  %0 = load i32, ptr %orderId.addr, align 4
  %1 = load i32, ptr %spaceNo.addr, align 4
  %2 = load i32, ptr %range.addr, align 4
  %3 = load i32, ptr %index.addr, align 4
  %4 = load ptr, ptr %name.addr, align 4
  %5 = call target("dx.Texture", <4 x float>, 0, 0, 0, 2) @llvm.dx.resource.handlefromimplicitbinding.tdx.Texture_v4f32_0_0_0_2t(i32 %0, i32 %1, i32 %2, i32 %3, ptr %4)
  %__handle = getelementptr inbounds nuw %"class.hlsl::Texture2D", ptr %tmp, i32 0, i32 0
  store target("dx.Texture", <4 x float>, 0, 0, 0, 2) %5, ptr %__handle, align 4
  %__handle1 = getelementptr inbounds nuw %"class.hlsl::Texture2D", ptr %tmp, i32 0, i32 0
  %6 = load target("dx.Texture", <4 x float>, 0, 0, 0, 2), ptr %__handle1, align 4
  %mips = getelementptr inbounds nuw %"class.hlsl::Texture2D", ptr %tmp, i32 0, i32 1
  %__handle2 = getelementptr inbounds nuw %"struct.hlsl::Texture2D<>::mips_type", ptr %mips, i32 0, i32 0
  store target("dx.Texture", <4 x float>, 0, 0, 0, 2) %6, ptr %__handle2, align 4
  call void @_ZN4hlsl9Texture2DIDv4_fEC1ERKS2_(ptr noundef nonnull align 4 dereferenceable(8) %agg.result, ptr noundef nonnull align 4 dereferenceable(8) %tmp) #3
  ret void
}

; Function Attrs: alwaysinline convergent mustprogress norecurse nounwind
define hidden void @_Z4mainv() #1 {
entry:
  %a = alloca %"struct.hlsl::Texture2D<>::mips_type", align 4
  %b = alloca %"struct.hlsl::Texture2D<>::mips_slice_type", align 4
  %c = alloca %"struct.hlsl::Texture2D<>::mips_type", align 4
  call void @_ZN4hlsl9Texture2DIDv4_fE9mips_typeC1Ev(ptr noundef nonnull align 4 dereferenceable(4) %a) #3
  call void @_ZN4hlsl9Texture2DIDv4_fE15mips_slice_typeC1Ev(ptr noundef nonnull align 4 dereferenceable(8) %b) #3
  call void @_ZN4hlsl9Texture2DIDv4_fE9mips_typeC1ERKS3_(ptr noundef nonnull align 4 dereferenceable(4) %c, ptr noundef nonnull align 4 dereferenceable(4) getelementptr inbounds nuw (i8, ptr @_ZL1t, i32 4)) #3
  ret void
}

; Function Attrs: alwaysinline convergent mustprogress norecurse nounwind
define linkonce_odr hidden void @_ZN4hlsl9Texture2DIDv4_fE9mips_typeC1Ev(ptr noundef nonnull align 4 dereferenceable(4) %this) unnamed_addr #1 align 2 {
entry:
  %this.addr = alloca ptr, align 4
  store ptr %this, ptr %this.addr, align 4
  %this1 = load ptr, ptr %this.addr, align 4
  call void @_ZN4hlsl9Texture2DIDv4_fE9mips_typeC2Ev(ptr noundef nonnull align 4 dereferenceable(4) %this1) #3
  ret void
}

; Function Attrs: alwaysinline convergent mustprogress norecurse nounwind
define linkonce_odr hidden void @_ZN4hlsl9Texture2DIDv4_fE15mips_slice_typeC1Ev(ptr noundef nonnull align 4 dereferenceable(8) %this) unnamed_addr #1 align 2 {
entry:
  %this.addr = alloca ptr, align 4
  store ptr %this, ptr %this.addr, align 4
  %this1 = load ptr, ptr %this.addr, align 4
  call void @_ZN4hlsl9Texture2DIDv4_fE15mips_slice_typeC2Ev(ptr noundef nonnull align 4 dereferenceable(8) %this1) #3
  ret void
}

; Function Attrs: alwaysinline convergent mustprogress norecurse nounwind
define linkonce_odr hidden void @_ZN4hlsl9Texture2DIDv4_fE9mips_typeC1ERKS3_(ptr noundef nonnull align 4 dereferenceable(4) %this, ptr noundef nonnull align 4 dereferenceable(4) %other) unnamed_addr #1 align 2 {
entry:
  %this.addr = alloca ptr, align 4
  %other.addr = alloca ptr, align 4
  store ptr %this, ptr %this.addr, align 4
  store ptr %other, ptr %other.addr, align 4
  %this1 = load ptr, ptr %this.addr, align 4
  %0 = load ptr, ptr %other.addr, align 4
  call void @_ZN4hlsl9Texture2DIDv4_fE9mips_typeC2ERKS3_(ptr noundef nonnull align 4 dereferenceable(4) %this1, ptr noundef nonnull align 4 dereferenceable(4) %0) #3
  ret void
}

; Function Attrs: alwaysinline convergent mustprogress norecurse nounwind
define linkonce_odr hidden void @_ZN4hlsl9Texture2DIDv4_fEC1Ev(ptr noundef nonnull align 4 dereferenceable(8) %this) unnamed_addr #1 align 2 {
entry:
  %this.addr = alloca ptr, align 4
  store ptr %this, ptr %this.addr, align 4
  %this1 = load ptr, ptr %this.addr, align 4
  call void @_ZN4hlsl9Texture2DIDv4_fEC2Ev(ptr noundef nonnull align 4 dereferenceable(8) %this1) #3
  ret void
}

; Function Attrs: nocallback nofree nosync nounwind willreturn memory(none)
declare target("dx.Texture", <4 x float>, 0, 0, 0, 2) @llvm.dx.resource.handlefromimplicitbinding.tdx.Texture_v4f32_0_0_0_2t(i32, i32, i32, i32, ptr) #2

; Function Attrs: alwaysinline convergent mustprogress norecurse nounwind
define linkonce_odr hidden void @_ZN4hlsl9Texture2DIDv4_fEC1ERKS2_(ptr noundef nonnull align 4 dereferenceable(8) %this, ptr noundef nonnull align 4 dereferenceable(8) %other) unnamed_addr #1 align 2 {
entry:
  %this.addr = alloca ptr, align 4
  %other.addr = alloca ptr, align 4
  store ptr %this, ptr %this.addr, align 4
  store ptr %other, ptr %other.addr, align 4
  %this1 = load ptr, ptr %this.addr, align 4
  %0 = load ptr, ptr %other.addr, align 4
  call void @_ZN4hlsl9Texture2DIDv4_fEC2ERKS2_(ptr noundef nonnull align 4 dereferenceable(8) %this1, ptr noundef nonnull align 4 dereferenceable(8) %0) #3
  ret void
}

; Function Attrs: alwaysinline convergent mustprogress norecurse nounwind
define linkonce_odr hidden void @_ZN4hlsl9Texture2DIDv4_fEC2Ev(ptr noundef nonnull align 4 dereferenceable(8) %this) unnamed_addr #1 align 2 {
entry:
  %this.addr = alloca ptr, align 4
  store ptr %this, ptr %this.addr, align 4
  %this1 = load ptr, ptr %this.addr, align 4
  %mips = getelementptr inbounds nuw %"class.hlsl::Texture2D", ptr %this1, i32 0, i32 1
  call void @_ZN4hlsl9Texture2DIDv4_fE9mips_typeC1Ev(ptr noundef nonnull align 4 dereferenceable(4) %mips) #3
  %__handle = getelementptr inbounds nuw %"class.hlsl::Texture2D", ptr %this1, i32 0, i32 0
  store target("dx.Texture", <4 x float>, 0, 0, 0, 2) poison, ptr %__handle, align 4
  ret void
}

; Function Attrs: alwaysinline convergent mustprogress norecurse nounwind
define linkonce_odr hidden void @_ZN4hlsl9Texture2DIDv4_fEC2ERKS2_(ptr noundef nonnull align 4 dereferenceable(8) %this, ptr noundef nonnull align 4 dereferenceable(8) %other) unnamed_addr #1 align 2 {
entry:
  %this.addr = alloca ptr, align 4
  %other.addr = alloca ptr, align 4
  store ptr %this, ptr %this.addr, align 4
  store ptr %other, ptr %other.addr, align 4
  %this1 = load ptr, ptr %this.addr, align 4
  %mips = getelementptr inbounds nuw %"class.hlsl::Texture2D", ptr %this1, i32 0, i32 1
  call void @_ZN4hlsl9Texture2DIDv4_fE9mips_typeC1Ev(ptr noundef nonnull align 4 dereferenceable(4) %mips) #3
  %0 = load ptr, ptr %other.addr, align 4, !nonnull !3, !align !4
  %__handle = getelementptr inbounds nuw %"class.hlsl::Texture2D", ptr %0, i32 0, i32 0
  %1 = load target("dx.Texture", <4 x float>, 0, 0, 0, 2), ptr %__handle, align 4
  %__handle2 = getelementptr inbounds nuw %"class.hlsl::Texture2D", ptr %this1, i32 0, i32 0
  store target("dx.Texture", <4 x float>, 0, 0, 0, 2) %1, ptr %__handle2, align 4
  %2 = load ptr, ptr %other.addr, align 4, !nonnull !3, !align !4
  %mips3 = getelementptr inbounds nuw %"class.hlsl::Texture2D", ptr %2, i32 0, i32 1
  %mips4 = getelementptr inbounds nuw %"class.hlsl::Texture2D", ptr %this1, i32 0, i32 1
  %call = call noundef nonnull align 4 dereferenceable(4) ptr @_ZN4hlsl9Texture2DIDv4_fE9mips_typeaSERKS3_(ptr noundef nonnull align 4 dereferenceable(4) %mips4, ptr noundef nonnull align 4 dereferenceable(4) %mips3) #3
  ret void
}

; Function Attrs: alwaysinline convergent mustprogress norecurse nounwind
define linkonce_odr hidden noundef nonnull align 4 dereferenceable(4) ptr @_ZN4hlsl9Texture2DIDv4_fE9mips_typeaSERKS3_(ptr noundef nonnull align 4 dereferenceable(4) %this, ptr noundef nonnull align 4 dereferenceable(4) %other) #1 align 2 {
entry:
  %this.addr = alloca ptr, align 4
  %other.addr = alloca ptr, align 4
  store ptr %this, ptr %this.addr, align 4
  store ptr %other, ptr %other.addr, align 4
  %this1 = load ptr, ptr %this.addr, align 4
  %0 = load ptr, ptr %other.addr, align 4, !nonnull !3, !align !4
  %__handle = getelementptr inbounds nuw %"struct.hlsl::Texture2D<>::mips_type", ptr %0, i32 0, i32 0
  %1 = load target("dx.Texture", <4 x float>, 0, 0, 0, 2), ptr %__handle, align 4
  %__handle2 = getelementptr inbounds nuw %"struct.hlsl::Texture2D<>::mips_type", ptr %this1, i32 0, i32 0
  store target("dx.Texture", <4 x float>, 0, 0, 0, 2) %1, ptr %__handle2, align 4
  ret ptr %this1
}

; Function Attrs: alwaysinline convergent mustprogress norecurse nounwind
define linkonce_odr hidden void @_ZN4hlsl9Texture2DIDv4_fE9mips_typeC2Ev(ptr noundef nonnull align 4 dereferenceable(4) %this) unnamed_addr #1 align 2 {
entry:
  %this.addr = alloca ptr, align 4
  store ptr %this, ptr %this.addr, align 4
  %this1 = load ptr, ptr %this.addr, align 4
  %__handle = getelementptr inbounds nuw %"struct.hlsl::Texture2D<>::mips_type", ptr %this1, i32 0, i32 0
  store target("dx.Texture", <4 x float>, 0, 0, 0, 2) poison, ptr %__handle, align 4
  ret void
}

; Function Attrs: alwaysinline convergent mustprogress norecurse nounwind
define linkonce_odr hidden void @_ZN4hlsl9Texture2DIDv4_fE15mips_slice_typeC2Ev(ptr noundef nonnull align 4 dereferenceable(8) %this) unnamed_addr #1 align 2 {
entry:
  %this.addr = alloca ptr, align 4
  store ptr %this, ptr %this.addr, align 4
  %this1 = load ptr, ptr %this.addr, align 4
  %__handle = getelementptr inbounds nuw %"struct.hlsl::Texture2D<>::mips_slice_type", ptr %this1, i32 0, i32 0
  store target("dx.Texture", <4 x float>, 0, 0, 0, 2) poison, ptr %__handle, align 4
  ret void
}

; Function Attrs: alwaysinline convergent mustprogress norecurse nounwind
define linkonce_odr hidden void @_ZN4hlsl9Texture2DIDv4_fE9mips_typeC2ERKS3_(ptr noundef nonnull align 4 dereferenceable(4) %this, ptr noundef nonnull align 4 dereferenceable(4) %other) unnamed_addr #1 align 2 {
entry:
  %this.addr = alloca ptr, align 4
  %other.addr = alloca ptr, align 4
  store ptr %this, ptr %this.addr, align 4
  store ptr %other, ptr %other.addr, align 4
  %this1 = load ptr, ptr %this.addr, align 4
  %0 = load ptr, ptr %other.addr, align 4, !nonnull !3, !align !4
  %__handle = getelementptr inbounds nuw %"struct.hlsl::Texture2D<>::mips_type", ptr %0, i32 0, i32 0
  %1 = load target("dx.Texture", <4 x float>, 0, 0, 0, 2), ptr %__handle, align 4
  %__handle2 = getelementptr inbounds nuw %"struct.hlsl::Texture2D<>::mips_type", ptr %this1, i32 0, i32 0
  store target("dx.Texture", <4 x float>, 0, 0, 0, 2) %1, ptr %__handle2, align 4
  ret void
}

; Function Attrs: alwaysinline convergent nounwind
define internal void @_GLOBAL__sub_I_Texture2D_mips_errors.hlsl() #0 {
entry:
  call void @__cxx_global_var_init()
  ret void
}

attributes #0 = { alwaysinline convergent nounwind "no-nans-fp-math"="true" "no-signed-zeros-fp-math"="true" "no-trapping-math"="true" "stack-protector-buffer-size"="8" }
attributes #1 = { alwaysinline convergent mustprogress norecurse nounwind "no-nans-fp-math"="true" "no-signed-zeros-fp-math"="true" "no-trapping-math"="true" "stack-protector-buffer-size"="8" }
attributes #2 = { nocallback nofree nosync nounwind willreturn memory(none) }
attributes #3 = { convergent }

!dx.valver = !{!0}
!llvm.module.flags = !{!1}
!llvm.ident = !{!2}

!0 = !{i32 1, i32 8}
!1 = !{i32 4, !"dx.disable_optimizations", i32 1}
!2 = !{!"clang version 23.0.0git (git@github.com:llvm/llvm-project.git 61ec7e51067976def87b0fd8e88f084182349d51)"}
!3 = !{}
!4 = !{i64 4}
