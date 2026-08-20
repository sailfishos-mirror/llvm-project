// Minimal reproducer: UnsafeBufferReachableAnalysis hangs (never reaches a
// fixed point) when the pointer-flow graph contains a cycle whose edges each
// raise the pointer level.
//
// The BoundsPropagationGraph in UnsafeBufferUsageAnalysis.cpp adds an edge
// (src, i + d) -> (dest, j + d) for every pointer-flow edge (src, i) ->
// (dest, j). Its doc comment says the delta `d` is bounded by an upper bound
// `UB` derived from the pointer type's maximum level, but getDestNodes() never
// applies that bound: it emits dest nodes at getPointerLevel() + Delta with no
// cap. A cycle in which each traversal increases the level therefore produces
// ever-deeper reachable nodes, and step()'s worklist never drains.
//
// Here the single assignment `a = (char **)*a` yields the pointer-flow edge
// (a, 1) -> (a, 2): dereferencing `a` (char**) once gives a char*, and the
// reinterpret cast lets it be stored back into `a` (char**). Starting from the
// unsafe node (a, 1):
//   (a, 1) -> (a, 2) -> (a, 3) -> (a, 4) -> ...   (unbounded)
// The reinterpret cast is essential: without it the level-mismatched
// assignment would not type-check, so no such level-raising self-edge exists.
//
// This test currently HANGS the analyzer. It is committed as a documented
// reproducer for the fixed-point bug; once getDestNodes() caps the level it
// should terminate and can be turned into a regression test.
//
// To reproduce manually, run the analyzer step below and then:
//   killall clang-ssaf-analyzer

// RUN: rm -rf %t && mkdir -p %t

// RUN: %clang_cc1 -fsyntax-only %s \
// RUN:   --ssaf-extract-summaries=PointerFlow,UnsafeBufferUsage \
// RUN:   --ssaf-tu-summary-file=%t/tu.summary.json \
// RUN:   --ssaf-compilation-unit-id="tu-1"

// RUN: clang-ssaf-linker %t/tu.summary.json -o %t/lu.json

// RUN: clang-ssaf-analyzer %t/lu.json -o %t/wpa.json \
// RUN:   -a UnsafeBufferReachableAnalysisResult

void f(char **a, int i, int j) {
  a[i][j] = 0;      // 'a' is used as an unsafe buffer at levels 1 and 2
  a = (char **)*a;  // (a, 1) -> (a, 2): a self-edge that raises the level
}
