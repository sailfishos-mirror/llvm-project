set(LLVM_LINK_COMPONENTS
  Support
  )

add_clang_library(clangScalableStaticAnalysisFrameworkAnalyses
  UnsafeBufferUsage/UnsafeBufferUsage.cpp
  UnsafeBufferUsage/UnsafeBufferUsageExtractor.cpp  

  LINK_LIBS
  clangAST
  clangAnalysis
  clangBasic
  clangScalableStaticAnalysisFrameworkCore

  DEPENDS
  )
