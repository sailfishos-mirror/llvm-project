// RUN: %clang_cc1 -triple powerpc-ibm-aix -mloadtime-comment-vars=sccsid,version,build_number -emit-llvm -o - %s | FileCheck %s
// RUN: %clang_cc1 -triple powerpc64-ibm-aix -mloadtime-comment-vars=sccsid,version,build_number -emit-llvm -o - %s | FileCheck %s

// String pointer (Should be emitted and tagged)
static char *sccsid = "@(#) Object sid Version 1.0";

// String array (Should be emitted and tagged)
static char version[] = "Object scc Version 2.0";

// Const string (Not in CLI list, should NOT be emitted)
static const char *copyright = "Copyright 2026";

// Integer (In CLI list but invalid type, should NOT be emitted)
static int build_number = 12345;

void foo() {}

// CHECK: @sccsid = internal global ptr @.str, align {{[0-9]+}}, !copyright.variable ![[MD_SCC:[0-9]+]]
// CHECK-NEXT: @.str = private unnamed_addr constant [28 x i8] c"@(#) Object sid Version 1.0\00", align 1
// CHECK: @version = internal global [23 x i8] c"Object scc Version 2.0\00", align {{[0-9]+}}, !copyright.variable ![[MD_VER:[0-9]+]]

// Ensure the unrequested/invalid variables are optimized away
// CHECK-NOT: @copyright
// CHECK-NOT: @build_number

// Ensure the metadata tags contain the correct strings
// CHECK: ![[MD_SCC]] = !{!"sccsid"}
// CHECK: ![[MD_VER]] = !{!"version"}
