// RUN: %clang_cc1 -triple dxil-pc-shadermodel6.0-library -x hlsl -emit-llvm -disable-llvm-passes -finclude-default-header -verify %s

Texture2D<float4> t;

void main() {
  // expected-error@+3 {{'mips_type' is a private member of 'hlsl::Texture2D<>'}}
  // expected-note@*:* {{implicitly declared private here}}
  // expected-error@+1 {{intermediate helper type 'Texture2D<float4>::mips_type' cannot be used as a variable}}
  Texture2D<float4>::mips_type a; 

  // expected-error@+3 {{'mips_slice_type' is a private member of 'hlsl::Texture2D<>'}}
  // expected-note@*:* {{implicitly declared private here}}
  // expected-error@+1 {{intermediate helper type 'Texture2D<float4>::mips_slice_type' cannot be used as a variable}}
  Texture2D<float4>::mips_slice_type b;

  // expected-warning@+2 {{'auto' type specifier is a HLSL 202y extension}}
  // expected-error@+1 {{intermediate helper type 'mips_type' cannot be used as a variable}}
  auto c = t.mips;
}
