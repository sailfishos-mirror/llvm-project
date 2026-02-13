if (LLDB_ENABLE_TREESITTER)
  set(RUST_HIGHLIGHTER_PLUGIN lldbPluginHighlighterTreeSitterRust)
endif()

add_lldb_unittest(HighlighterTests
  HighlighterTest.cpp

  LINK_LIBS
    lldbPluginHighlighterDefault
    lldbPluginHighlighterClang
    lldbPluginCPlusPlusLanguage
    lldbPluginObjCLanguage
    lldbPluginObjCPlusPlusLanguage
    ${RUST_HIGHLIGHTER_PLUGIN}
)
