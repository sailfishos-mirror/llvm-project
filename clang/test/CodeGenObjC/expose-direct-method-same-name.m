// This tests that when using visibility("default") to expose direct methods across
// linkage units, category names are included in the mangled symbol names to avoid
// duplicate symbol issues. Previously, direct methods with the same name in different
// categories would create duplicate symbols when linked together. Now each category
// method gets a unique symbol name that includes the category name.
//
// REQUIRES: system-darwin
//
// RUN: rm -rf %t && mkdir -p %t
// RUN: split-file %s %t
//
// Compile Bar base class
// RUN: %clang -fobjc-direct-precondition-thunk -target arm64-apple-darwin \
// RUN:   -fobjc-arc -c %t/bar.m -I %t -o %t/bar.o
//
// Compile CategoryA and CategoryB separately
// RUN: %clang -fobjc-direct-precondition-thunk -target arm64-apple-darwin \
// RUN:   -fobjc-arc -c %t/bar_catA.m -I %t -o %t/bar_catA.o
//
// RUN: %clang -fobjc-direct-precondition-thunk -target arm64-apple-darwin \
// RUN:   -fobjc-arc -c %t/bar_catB.m -I %t -o %t/bar_catB.o
//
// Link to dylib - should succeed now that category names are included in symbol names
// RUN: %clang -target arm64-apple-darwin -fobjc-arc -dynamiclib \
// RUN:   %t/bar.o %t/bar_catA.o %t/bar_catB.o \
// RUN:   -framework Foundation \
// RUN:   -o %t/libBar.dylib
//
// RUN: llvm-nm %t/libBar.dylib | FileCheck %s --check-prefix=CHECK_DYLIB
// CHECK_DYLIB: _-[Bar(CategoryA) sameName]D
// CHECK_DYLIB: _-[Bar(CategoryB) sameName]D
//
// Compile main.m (uses CategoryB)
// RUN: %clang -fobjc-direct-precondition-thunk -target arm64-apple-darwin \
// RUN:   -fobjc-arc -c %t/main.m -I %t -o %t/main.o
//
// Link main against libBar.dylib and run it
// RUN: %clang -target arm64-apple-darwin -fobjc-arc \
// RUN:   %t/main.o -L%t -lBar \
// RUN:   -framework Foundation \
// RUN:   -Wl,-rpath,%t \
// RUN:   -o %t/main
// RUN: %t/main
//
// RUN: llvm-nm %t/main | FileCheck %s --check-prefix=CHECK_EXE
// CHECK_EXE: U _-[Bar(CategoryB) sameName]D
// CHECK_EXE: t _-[Bar(CategoryB) sameName]D_thunk
// CHECK_EXE-NOT: CategoryA

//--- bar.h
#import <Foundation/Foundation.h>

@interface Bar : NSObject
@end

//--- bar.m
#import "bar.h"

@implementation Bar
@end

//--- bar_catA.h
#import "bar.h"

@interface Bar (CategoryA)
- (int)sameName __attribute__((objc_direct, visibility("default")));
@end

//--- bar_catB.h
#import "bar.h"

@interface Bar (CategoryB)
- (int)sameName __attribute__((objc_direct, visibility("default")));
@end

//--- bar_catA.m
#import "bar_catA.h"

@implementation Bar (CategoryA)
- (int)sameName {
  return 1;
}
@end

//--- bar_catB.m
#import "bar_catB.h"

@implementation Bar (CategoryB)
- (int)sameName {
  return 2;
}
@end

//--- main.m
#import "bar_catB.h"

int main(int argc, char *argv[]) {
  @autoreleasepool {
    Bar *bar = [[Bar alloc] init];
    int result = [bar sameName];
    return result != 2;
  }
}
