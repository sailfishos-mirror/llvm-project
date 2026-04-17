set(LLVM_LINK_COMPONENTS
  Support
  )

add_clang_library(clangScalableStaticAnalysisFrameworkAnalyses 
  CallGraph/CallGraphExtractor.cpp
  CallGraph/CallGraphJSONFormat.cpp
  EntityPointerLevel/EntityPointerLevel.cpp
  PointerFlow/PointerFlow.cpp
  PointerFlow/PointerFlowAnalysis.cpp
  PointerFlow/PointerFlowExtractor.cpp
  SSAFAnalysesCommon.cpp
  UnsafeBufferUsage/UnsafeBufferUsage.cpp
  UnsafeBufferUsage/UnsafeBufferUsageAnalysis.cpp
  UnsafeBufferUsage/UnsafeBufferUsageExtractor.cpp

  LINK_LIBS
  clangAST
  clangAnalysis
  clangBasic
  clangScalableStaticAnalysisFrameworkCore

  DEPENDS
  )
