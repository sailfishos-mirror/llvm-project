add_library(tree-sitter INTERFACE)
target_include_directories(tree-sitter
    INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include
)
