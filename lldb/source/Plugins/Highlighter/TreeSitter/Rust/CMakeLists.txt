add_lldb_library(lldbPluginHighlighterTreeSitterRust PLUGIN
  RustTreeSitterHighlighter.cpp

  LINK_COMPONENTS
    Support
  LINK_LIBS
    lldbCore
    lldbUtility
    lldbTreeSitter
    tree-sitter-rust
)
