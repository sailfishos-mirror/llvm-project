// REQUIRES: system-darwin

// RUN: rm -rf %t
// RUN: split-file %s %t

// Test 1: Check IR from library implementation (visibility attributes)
// RUN: %clang -target arm64-apple-darwin -fobjc-arc \
// RUN:   -fobjc-direct-precondition-thunk -S -emit-llvm -o - %t/foo.m \
// RUN:   -I %t | FileCheck %s --check-prefix=FOO_M

// Test 2: Check IR from main (consumer)
// RUN: %clang -target arm64-apple-darwin -fobjc-arc \
// RUN:   -fobjc-direct-precondition-thunk -S -emit-llvm -o - %t/main.m \
// RUN:   -I %t | FileCheck %s --check-prefix=MAIN_M

// Test 3: Build libFoo.dylib from foo.m
// RUN: %clang -fobjc-direct-precondition-thunk -target arm64-apple-darwin \
// RUN:   -fobjc-arc -dynamiclib %t/foo.m -I %t \
// RUN:   -framework Foundation \
// RUN:   -install_name @rpath/libFoo.dylib \
// RUN:   -o %t/libFoo.dylib

// Test 4: Verify visibility works correctly in dylib
// RUN: llvm-nm -g %t/libFoo.dylib | FileCheck %s --check-prefix=DYLIB

// Hidden visibility methods should NOT be exported
// DYLIB-NOT: -[Foo instanceMethod:]D
// DYLIB-NOT: +[Foo classMethod:]D
// DYLIB-NOT: -[Foo privateValue]D
// DYLIB-NOT: -[Foo setPrivateValue:]D

// Default visibility methods SHOULD be exported
// DYLIB: {{[0-9a-f]+}} T {{.*}}_+[Foo exportedClassMethod:]D
// DYLIB: {{[0-9a-f]+}} T {{.*}}_-[Foo exportedInstanceMethod:]D
// DYLIB: {{[0-9a-f]+}} T {{.*}}_-[Foo exportedValue]D
// DYLIB: {{[0-9a-f]+}} T {{.*}}_-[Foo setExportedValue:]D

// Test 5: Compile main.m
// RUN: %clang -fobjc-direct-precondition-thunk -target arm64-apple-darwin \
// RUN:   -fobjc-arc -c %t/main.m -I %t -o %t/main.o

// Test 6: Link main with libFoo.dylib
// RUN: %clang -target arm64-apple-darwin -fobjc-arc \
// RUN:   %t/main.o -L%t -lFoo \
// RUN:   -Wl,-rpath,%t \
// RUN:   -framework Foundation \
// RUN:   -o %t/main

// Test 7: Verify symbols in main executable
// RUN: llvm-nm %t/main | FileCheck %s --check-prefix=EXE

// Thunks should be defined locally
// EXE-DAG: {{[0-9a-f]+}} t {{.*}}_+[Foo exportedClassMethod:]D_thunk
// EXE-DAG: {{[0-9a-f]+}} t {{.*}}_-[Foo exportedInstanceMethod:]D_thunk
// EXE-DAG: {{[0-9a-f]+}} t {{.*}}_-[Foo exportedValue]D_thunk
// EXE-DAG: {{[0-9a-f]+}} t {{.*}}_-[Foo setExportedValue:]D_thunk

// Actual methods should be undefined
// EXE-DAG: U {{.*}}_+[Foo exportedClassMethod:]D
// EXE-DAG: U {{.*}}_-[Foo exportedInstanceMethod:]D
// EXE-DAG: U {{.*}}_-[Foo exportedValue]D
// EXE-DAG: U {{.*}}_-[Foo setExportedValue:]D

//--- foo.h
// Header for libFoo
#import <Foundation/Foundation.h>

@interface Foo : NSObject
// Direct properties with default hidden visibility
@property (nonatomic, direct) int privateValue;

// Direct properties with explicit default visibility
@property (nonatomic, direct) int exportedValue __attribute__((visibility("default")));

- (instancetype)initWithprivateValue:(int)x exportedValue:(int)y;
// Default hidden visibility
- (int)instanceMethod:(int)x __attribute__((objc_direct));
+ (int)classMethod:(int)x __attribute__((objc_direct));

// Explicit default visibility (should be exported)
- (int)exportedInstanceMethod:(int)x __attribute__((objc_direct, visibility("default")));
+ (int)exportedClassMethod:(int)x __attribute__((objc_direct, visibility("default")));
@end

//--- foo.m

// libFoo does not have thunks because the true implementation is not used internally.
// FOO_M-NOT: @{{.*}}_thunk
#import "foo.h"

@implementation Foo

// FOO_M-LABEL: define internal ptr @"\01-[Foo initWithprivateValue:exportedValue:]"
- (instancetype)initWithprivateValue:(int)x exportedValue:(int)y {
  self = [super init];
  if (self) {
    _privateValue = x;
    _exportedValue = y;
  }
  return self;
}

// FOO_M-LABEL: define hidden i32 @"-[Foo instanceMethod:]D"
- (int)instanceMethod:(int)x {
  // Compiler is smart enough to know that self is non-nil, so we dispatch to
  // true implementation.
  // FOO_M: call i32 @"-[Foo privateValue]D"
  // FOO_M: call i32 @"-[Foo exportedValue]D"
  return x + [self privateValue] + [self exportedValue];
}

// Hidden property getter and setter (default visibility)
// FOO_M-LABEL: define hidden i32 @"-[Foo privateValue]D"

// Exported property getter and setter (explicit default visibility)
// FOO_M-LABEL: define dso_local i32 @"-[Foo exportedValue]D"

// FOO_M-LABEL: define hidden i32 @"+[Foo classMethod:]D"
+ (int)classMethod:(int)x {
  return x * 3;
}

// FOO_M-LABEL: define dso_local i32 @"-[Foo exportedInstanceMethod:]D"
- (int)exportedInstanceMethod:(int)x {
  // FOO_M: call i32 @"-[Foo privateValue]D"
  // FOO_M: call i32 @"-[Foo exportedValue]D"
  return x + [self privateValue] + [self exportedValue];
}

// FOO_M-LABEL: define dso_local i32 @"+[Foo exportedClassMethod:]D"
+ (int)exportedClassMethod:(int)x {
  return x * 5;
}

// Hidden property getter and setter (default visibility)
// FOO_M-LABEL: define hidden void @"-[Foo setPrivateValue:]D"

// Exported property getter and setter (explicit default visibility)
// FOO_M-LABEL: define dso_local void @"-[Foo setExportedValue:]D"

@end

//--- main.m
// Consumer of libFoo (separate linkage unit)
#import "foo.h"
#include <stdio.h>

int main() {
@autoreleasepool {
    Foo *obj = [[Foo alloc] initWithprivateValue:10 exportedValue:20];
    printf("Allocated Foo\n");

    // MAIN_M: call void @"-[Foo setExportedValue:]D_thunk"
    [obj setExportedValue:30];

    // MAIN_M: call i32 @"-[Foo exportedValue]D_thunk"
    printf("Reset exportedValue: %d\n", [obj exportedValue]);

    // MAIN_M: call i32 @"-[Foo exportedInstanceMethod:]D_thunk"
    printf("Exported instance: %d\n", [obj exportedInstanceMethod:10]);

    // MAIN_M: call i32 @"+[Foo exportedClassMethod:]D_thunk"
    printf("Exported class: %d\n", [Foo exportedClassMethod:10]);
}
    return 0;
}

// Thunks are generated during compilation
// MAIN_M-LABEL: define linkonce_odr hidden void @"-[Foo setExportedValue:]D_thunk"
// MAIN_M-LABEL: define linkonce_odr hidden i32 @"-[Foo exportedValue]D_thunk"
// MAIN_M-LABEL: define linkonce_odr hidden i32 @"-[Foo exportedInstanceMethod:]D_thunk"
// MAIN_M-LABEL: define linkonce_odr hidden i32 @"+[Foo exportedClassMethod:]D_thunk"
