// RUN: %clang -target powerpc64le-unknown-linux-gnu -fsyntax-only \
// RUN:   -mcpu=pwr9 -mfloat16 %s 2>&1 | FileCheck %s --check-prefix=HASFLOAT16 --allow-empty
// RUN: %clang -target powerpc64le-unknown-linux-gnu -fsyntax-only \
// RUN:   -mcpu=pwr8 -mfloat16 %s 2>&1 | FileCheck %s --check-prefix=HASFLOAT16 --allow-empty

// RUN: not %clang -target powerpc64le-unknown-linux-gnu -fsyntax-only \
// RUN:   -mcpu=pwr7 -mfloat16 %s 2>&1 | FileCheck %s --check-prefix=BADCPU
// RUN: not %clang -target powerpc64le-unknown-linux-gnu -fsyntax-only \
// RUN:   -mcpu=pwr6 -mfloat16 %s 2>&1 | FileCheck %s --check-prefix=BADCPU

// RUN: not %clang -target powerpc64le-unknown-linux-gnu -fsyntax-only \
// RUN:   -mcpu=pwr9 -mfloat16 -msoft-float %s 2>&1 | FileCheck %s --check-prefix=SOFTFLOAT

// RUN: not %clang -target powerpc64le-unknown-linux-gnu -fsyntax-only \
// RUN:   -mcpu=pwr9 %s 2>&1 | FileCheck %s --check-prefix=NOFLAG

_Float16 f;

// HASFLOAT16-NOT: option '-mfloat16' cannot be specified with
// HASFLOAT16-NOT: _Float16 is not supported on this target

// BADCPU: option '-mfloat16' cannot be specified with

// SOFTFLOAT: option '-mfloat16' cannot be specified with '-msoft-float'

// NOFLAG: _Float16 is not supported on this target
