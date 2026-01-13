// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -emit-llvm -o - %s -O1 -fexceptions -fcxx-exceptions | FileCheck %s --check-prefixes=COMMON,TIGHT
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -emit-llvm -o - %s -O1 -fexceptions -fcxx-exceptions -sloppy-temporary-lifetimes | FileCheck %s --check-prefixes=SLOPPY,COMMON

// COM: Note that this test case would break if we allowed tighter lifetimes to
// run when exceptions were enabled. If we make them work together this test
// will need to be updated.

extern "C" {

struct Trivial {
  int x[100];
};

void func_that_throws(Trivial t);

// COMMON-LABEL: define{{.*}} void @test()
void test() {
  // COMMON: %[[AGG1:.*]] = alloca %struct.Trivial
  // COMMON: %[[AGG2:.*]] = alloca %struct.Trivial

  // TIGHT: call void @llvm.lifetime.start.p0(ptr{{.*}} %[[AGG1]])
  // COMMON: invoke void @func_that_throws(ptr{{.*}} %[[AGG1]])
  // TIGHT-NEXT: to label %[[CONT:.*]] unwind label %[[LPAD1:.*]]

  // TIGHT: [[CONT]]:
  // TIGHT: call void @llvm.lifetime.end.p0(ptr{{.*}} %[[AGG1]])
  // TIGHT: call void @llvm.lifetime.start.p0(ptr{{.*}} %[[AGG2]])
  // COMMON: invoke void @func_that_throws(ptr{{.*}} %[[AGG2]])
  // TIGHT-NEXT: to label %[[CONT2:.*]] unwind label %[[LPAD2:.*]]

  // TIGHT: [[CONT2]]:
  // TIGHT-NEXT: call void @llvm.lifetime.end.p0(ptr{{.*}} %[[AGG2]])

  // TIGHT: [[LPAD1:lpad.*]]:
  // TIGHT: landingpad
  // TIGHT: br label %[[EHCLEANUP:.*]]

  // TIGHT: [[LPAD2:lpad.*]]:
  // TIGHT: landingpad
  // TIGHT: call void @llvm.lifetime.end.p0(ptr{{.*}} %[[AGG2]])
  // TIGHT: br label %[[EHCLEANUP]]

  // TIGHT: [[EHCLEANUP]]:
  // TIGHT: call void @llvm.lifetime.end.p0(ptr{{.*}} %[[AGG1]])


  // SLOPPY-NOT: llvm.lifetime.start
  // SLOPPY-NOT: llvm.lifetime.end
  try {
    func_that_throws(Trivial{0});
    func_that_throws(Trivial{0});
  } catch (...) {
  }
}
} // end extern "C"
