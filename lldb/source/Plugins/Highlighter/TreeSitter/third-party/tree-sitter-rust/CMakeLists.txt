add_lldb_library(tree-sitter-rust
  parser.c
  scanner.c

  LINK_LIBS
    tree-sitter
)

set_property(TARGET tree-sitter-rust PROPERTY C_STANDARD 11)

target_compile_options(tree-sitter-rust PRIVATE
    "-Wno-everything"
)

target_include_directories(tree-sitter-rust PRIVATE
  ${TreeSitter_INCLUDE_DIR})
