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

// The CHECK lines below use readable tokens instead of inline FileCheck regex.
// Expand the tokens into regex, then run FileCheck on the expanded copy:
//   $NS - skip the "namespace" array, up to its closing ']'.
//   $WS - whitespace, possibly spanning newlines.
//   $PTR_L1 - a reachable-set entry closing as "}, 1 ]", i.e. pointer level 1.
// RUN: sed -e 's|$NS|{{([^]]\|[[:space:]])+\\],}}|g' \
// RUN:     -e 's|$WS|{{[[:space:]]+}}|g' \
// RUN:     -e 's|$PTR_L1|{{[[:space:]]+\\},[[:space:]]+1[[:space:]]+\\]}}|g' \
// RUN:     %s > %t/checks.txt
// RUN: FileCheck %t/checks.txt --input-file=%t/wpa.json

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

  // 'q' is unsafe and the original propagation path is
  // 'q -> r -> return_new -> new_local -> place'.  However, since 'return_new'
  // (also 'place') is type-constrained, it is removed from the graph, resulting
  // in no path from 'q' to 'new_local'.
  q[5] = 0;
  ::operator delete(q);
}

void delete_caller() {
  char * foo;
  delete(foo);
}


// CHECK: "id_table"
// CHECK-DAG: "id": [[NEW_RET:[0-9]+]],$NS$WS"suffix": "0",$WS"usr": "c:@F@operator new#{{[^#]+}}#*v#"
// CHECK-DAG: "id": [[NEW_PLACE:[0-9]+]],$NS$WS"suffix": "2",$WS"usr": "c:@F@operator new#{{[^#]+}}#*v#"
// CHECK-DAG: "id": [[DEL_PTR:[0-9]+]],$NS$WS"suffix": "1",$WS"usr": "c:@F@operator delete#*v#"

// CHECK-DAG: "id": [[FOO_Q:[0-9]+]],$NS$WS"suffix": "",$WS"usr": "{{[^"]+}}foo#*I#@q"
// CHECK-DAG: "id": [[FOO_R:[0-9]+]],$NS$WS"suffix": "",$WS"usr": "{{[^"]+}}foo#*I#@r"
// CHECK-DAG: "id": [[NEW_LOCAL:[0-9]+]],$NS$WS"suffix": "",$WS"usr": "{{[^"]+}}operator new#{{[^#]+}}#*v#@new_local"
// CHECK-DAG: "id": [[DELETE_LOCAL:[0-9]+]],$NS$WS"suffix": "",$WS"usr": "{{[^"]+}}operator delete#*v#@delete_local"

// Contributor (owner) functions, i.e. the keys of the reachable-result map:
// CHECK-DAG: "id": [[CONTRIBUTOR_FOO:[0-9]+]],$NS$WS"suffix": "",$WS"usr": "c:@F@foo#*I#"
// CHECK-DAG: "id": [[CONTRIBUTOR_DELETE_FN:[0-9]+]],$NS$WS"suffix": "",$WS"usr": "c:@F@operator delete#*v#"

// The return and every new/delete parameter are reported as type-constrained.
// CHECK: "analysis_name": "TypeConstrainedPointersAnalysisResult"
// CHECK-DAG: "@": [[NEW_RET]]
// CHECK-DAG: "@": [[NEW_PLACE]]
// CHECK-DAG: "@": [[DEL_PTR]]

// CHECK: "analysis_name": "UnsafeBufferReachableAnalysisResult"

// 'foo' reaches its own unsafe pointers 'q' and 'r'.
// CHECK: "@": [[CONTRIBUTOR_FOO]]$WS},$WS[
// CHECK-DAG: "@": [[FOO_Q]]$PTR_L1
// CHECK-DAG: "@": [[FOO_R]]$PTR_L1

// 'operator delete' reaches its own unsafe pointer 'delete_local'.
// CHECK: "@": [[CONTRIBUTOR_DELETE_FN]]$WS},$WS[
// CHECK-DAG: "@": [[DELETE_LOCAL]]$PTR_L1

// CHECK-NOT: "@": [[NEW_RET]]$WS
// CHECK-NOT: "@": [[NEW_PLACE]]$WS
// CHECK-NOT: "@": [[DEL_PTR]]$WS
// CHECK-NOT: "@": [[NEW_LOCAL]]$WS

// CHECK: "analysis_name"
