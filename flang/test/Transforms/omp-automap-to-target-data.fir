// RUN: fir-opt --omp-automap-to-target-data %s | FileCheck %s
// Test OMP AutomapToTargetData pass.

module {
  fir.global
      @_QMtestEarr{omp.declare_target = #omp.declaretarget<device_type = (any),
                       capture_clause = (enter), automap = true>} target
                       : !fir.box<!fir.heap<!fir.array<?xi32>>>

      func.func @automap() {
    %c0 = arith.constant 0 : index
    %c10 = arith.constant 10 : i32
    %addr = fir.address_of(@_QMtestEarr) : !fir.ref<!fir.box<!fir.heap<!fir.array<?xi32>>>>
    %decl:2 = hlfir.declare %addr {fortran_attrs = #fir.var_attrs<allocatable, target>, uniq_name = "_QMtestEarr"} : (!fir.ref<!fir.box<!fir.heap<!fir.array<?xi32>>>>) -> (!fir.ref<!fir.box<!fir.heap<!fir.array<?xi32>>>>, !fir.ref<!fir.box<!fir.heap<!fir.array<?xi32>>>>)
    %idx = fir.convert %c10 : (i32) -> index
    %cond = arith.cmpi sgt, %idx, %c0 : index
    %n = arith.select %cond, %idx, %c0 : index
    %mem = fir.allocmem !fir.array<?xi32>, %n {fir.must_be_heap = true}
    %shape = fir.shape %n : (index) -> !fir.shape<1>
    %box = fir.embox %mem(%shape) : (!fir.heap<!fir.array<?xi32>>, !fir.shape<1>) -> !fir.box<!fir.heap<!fir.array<?xi32>>>
    fir.store %box to %decl#0 : !fir.ref<!fir.box<!fir.heap<!fir.array<?xi32>>>>
    %ld = fir.load %decl#0 : !fir.ref<!fir.box<!fir.heap<!fir.array<?xi32>>>>
    %base = fir.box_addr %ld : (!fir.box<!fir.heap<!fir.array<?xi32>>>) -> !fir.heap<!fir.array<?xi32>>
    fir.freemem %base : !fir.heap<!fir.array<?xi32>>
    %undef = fir.zero_bits !fir.heap<!fir.array<?xi32>>
    %sh0 = fir.shape %c0 : (index) -> !fir.shape<1>
    %empty = fir.embox %undef(%sh0) : (!fir.heap<!fir.array<?xi32>>, !fir.shape<1>) -> !fir.box<!fir.heap<!fir.array<?xi32>>>
    fir.store %empty to %decl#0 : !fir.ref<!fir.box<!fir.heap<!fir.array<?xi32>>>>
    return
  }
}

// CHECK-LABEL: func.func @automap()
// CHECK: fir.allocmem
// CHECK: fir.store
// CHECK: omp.map.info {{.*}}map_clauses(to)
// CHECK: omp.target_enter_data
// CHECK: omp.map.info {{.*}}map_clauses(delete)
// CHECK: omp.target_exit_data
// CHECK: fir.freemem
