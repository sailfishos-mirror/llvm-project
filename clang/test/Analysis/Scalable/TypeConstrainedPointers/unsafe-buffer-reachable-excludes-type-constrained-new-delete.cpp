// Test that UnsafeBufferReachableAnalysis excludes the
// type-constrained pointers of 'operator new' / 'operator delete'
// overloads.
//
// RUN: rm -rf %t && mkdir -p %t

// RUN: %clang_cc1 -fsyntax-only %s \
// RUN:   --ssaf-extract-summaries=PointerFlow,UnsafeBufferUsage,TypeConstrainedPointers \
// RUN:   --ssaf-tu-summary-file=%t/tu.summary.json \
// RUN:   --ssaf-compilation-unit-id="tu-1"

// RUN: clang-ssaf-linker %t/tu.summary.json -o %t/lu.json

// RUN: clang-ssaf-analyzer %t/lu.json -o %t/wpa.json \
// RUN:   -a UnsafeBufferReachableAnalysisResult

// RUN: FileCheck %s --input-file=%t/wpa.json

typedef __SIZE_TYPE__ size_t;

// Return value and the 2nd parameter are type-constrained:
void *operator new(size_t size, void *place) noexcept {
  int *new_local = (int *)place;

  return new_local;
}

// The parameter is type-constrained:
void operator delete(void *ptr) noexcept {
  int *delete_local = (int *)ptr;

  delete_local[5] = 0;
}

void foo(int *p) {  
  void *r = ::operator new(10, p);
  int *q = (int *)r;

  // 'q' is unsafe, it propagates along the path
  // 'q -> r -> return_new -> new_local -> place'  
  // All pointers along the path are rechable but return_new and place
  // are type-constrained.
  q[5] = 0; 
  ::operator delete(q);
}


// CHECK: "id_table"
// CHECK-DAG: "id": [[NEW_RET:[0-9]+]],{{([^]]|[[:space:]])+\],[[:space:]]+"suffix": "0",[[:space:]]+"usr": }}"c:@F@operator new#l#*v#"
// CHECK-DAG: "id": [[NEW_PLACE:[0-9]+]],{{([^]]|[[:space:]])+\],[[:space:]]+"suffix": "2",[[:space:]]+"usr": }}"c:@F@operator new#l#*v#"
// CHECK-DAG: "id": [[DEL_PTR:[0-9]+]],{{([^]]|[[:space:]])+\],[[:space:]]+"suffix": "1",[[:space:]]+"usr": }}"c:@F@operator delete#*v#"

// CHECK-DAG: "id": [[FOO_Q:[0-9]+]],{{([^]]|[[:space:]])+\],[[:space:]]+"suffix": "",[[:space:]]+"usr": "[^"]+}}foo#*I#@q"
// CHECK-DAG: "id": [[FOO_R:[0-9]+]],{{([^]]|[[:space:]])+\],[[:space:]]+"suffix": "",[[:space:]]+"usr": "[^"]+}}foo#*I#@r"
// CHECK-DAG: "id": [[NEW_LOCAL:[0-9]+]],{{([^]]|[[:space:]])+\],[[:space:]]+"suffix": "",[[:space:]]+"usr": "[^"]+}}operator new#l#*v#@new_local"
// CHECK-DAG: "id": [[DELETE_LOCAL:[0-9]+]],{{([^]]|[[:space:]])+\],[[:space:]]+"suffix": "",[[:space:]]+"usr": "[^"]+}}operator delete#*v#@delete_local"

// The return and every new/delete parameter are reported as type-constrained.
// CHECK: "analysis_name": "TypeConstrainedPointersAnalysisResult"
// CHECK-DAG: "@": [[NEW_RET]]
// CHECK-DAG: "@": [[NEW_PLACE]]
// CHECK-DAG: "@": [[DEL_PTR]]

// CHECK: "analysis_name": "UnsafeBufferReachableAnalysisResult"
// CHECK-DAG: {{\{[[:space:]]+}}"@": [[FOO_Q]]{{[[:space:]]+\},[[:space:]]+1[[:space:]]+\]}}
// CHECK-DAG: {{\{[[:space:]]+}}"@": [[FOO_R]]{{[[:space:]]+\},[[:space:]]+1[[:space:]]+\]}}
// CHECK-DAG: {{\{[[:space:]]+}}"@": [[NEW_LOCAL]]{{[[:space:]]+\},[[:space:]]+1[[:space:]]+\]}}
// CHECK-DAG: {{\{[[:space:]]+}}"@": [[DELETE_LOCAL]]{{[[:space:]]+\},[[:space:]]+1[[:space:]]+\]}}

// CHECK-NOT: {{"@": }}[[NEW_RET]]{{[[:space:]]}}
// CHECK-NOT: {{"@": }}[[NEW_PLACE]]{{[[:space:]]}}
// CHECK-NOT: {{"@": }}[[DEL_PTR]]{{[[:space:]]}}

// CHECK: "analysis_name"
