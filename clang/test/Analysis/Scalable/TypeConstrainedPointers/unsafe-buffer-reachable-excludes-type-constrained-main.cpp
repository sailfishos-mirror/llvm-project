// Test that UnsafeBufferReachableAnalysis excludes type-constrained pointers

// RUN: rm -rf %t && mkdir -p %t

// RUN: %clang_cc1 -fsyntax-only %s \
// RUN:   --ssaf-extract-summaries=PointerFlow,UnsafeBufferUsage,TypeConstrainedPointers \
// RUN:   --ssaf-tu-summary-file=%t/tu.summary.json \
// RUN:   --ssaf-compilation-unit-id="tu-1"

// RUN: clang-ssaf-linker %t/tu.summary.json -o %t/lu.json

// RUN: clang-ssaf-analyzer %t/lu.json -o %t/wpa.json \
// RUN:   -a UnsafeBufferReachableAnalysisResult

// RUN: FileCheck %s --input-file=%t/wpa.json


int main(int argc, char **argv) {
  argv[5] = 0; // unsafe use of a type-constrained pointer
  return 0;
}

// 'q' is an ordinary unsafe pointer parameter and must remain in the result.
void foo(int *q) {
  q[5] = 0;
}

// CHECK-DAG: "id": [[Q_ID:[0-9]+]],{{([^]]|[[:space:]])+\],[[:space:]]+"suffix": "1",[[:space:]]+"usr": }}"c:@F@foo#*I#"
// CHECK-DAG: "id": [[ARGV_ID:[0-9]+]],{{([^]]|[[:space:]])+\],[[:space:]]+"suffix": "2",[[:space:]]+"usr": }}"c:@F@main{{.*}}"

// 'argv' is reported as type-constrained.
// CHECK: "analysis_name": "TypeConstrainedPointersAnalysisResult"
// CHECK: "@": [[ARGV_ID]]

// In the reachable result 'q' is present but 'argv' is not.
// CHECK: "analysis_name": "UnsafeBufferReachableAnalysisResult"
// CHECK-DAG: {{\{[[:space:]]+}}"@": [[Q_ID]]{{[[:space:]]+\},[[:space:]]+1[[:space:]]+\]}}
// CHECK-NOT: "@": [[ARGV_ID]]

// CHECK: "analysis_name"
