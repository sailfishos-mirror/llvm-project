// RUN: mlir-tblgen -gen-op-defs -I %S/../../include %s | FileCheck %s

include "mlir/IR/OpBase.td"

def TestDialect : Dialect {
  let name = "test";
  let cppNamespace = "::test";
}

// A custom properties printer shadows the generated per-field helper.
// CHECK-NOT: CustomPropertiesPrinterOp::_odsPrintPropertiesAsKeyValueList
// CHECK: void CustomPropertiesPrinterOp::print
// CHECK-NOT: CustomPropertiesPrinterOp::_odsPrintPropertiesAsKeyValueList
def CustomPropertiesPrinterOp : Op<TestDialect, "custom_properties_printer"> {
  let arguments = (ins I64Attr:$attr);
  let assemblyFormat = "prop-dict attr-dict";
  let hasCustomPropertiesPrinter = 1;
  let extraClassDeclaration = [{
    void printProperties(::mlir::MLIRContext *, ::mlir::OpAsmPrinter &,
                         const Properties &,
                         ::mlir::ArrayRef<::llvm::StringRef>);
  }];
}
