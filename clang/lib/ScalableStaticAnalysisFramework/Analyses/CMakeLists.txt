set(LLVM_LINK_COMPONENTS
  Support
  )

add_clang_library(clangScalableStaticAnalysisFrameworkAnalyses 
  CallGraph/CallGraphExtractor.cpp
  CallGraph/CallGraphJSONFormat.cpp
  EntityPointerLevel/EntityPointerLevel.cpp
  UnsafeBufferUsage/UnsafeBufferUsage.cpp
  UnsafeBufferUsage/UnsafeBufferUsageExtractor.cpp
  PointerAssignments/PointerAssignments.cpp
  PointerAssignments/PointerAssignmentsExtractor.cpp

  LINK_LIBS
  clangAST
  clangAnalysis
  clangBasic
  clangScalableStaticAnalysisFrameworkCore

  DEPENDS
  )
