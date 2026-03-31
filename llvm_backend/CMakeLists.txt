aux_source_directory(. DIR_LIB_SRCS)
add_library(llvm_backend ${DIR_LIB_SRCS})

target_include_directories(llvm_backend SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS})
target_compile_definitions(llvm_backend PRIVATE ${LLVM_DEFINITIONS})

llvm_map_components_to_libnames(PANGU_LLVM_LIBS Core Support)
target_link_libraries(llvm_backend ${PANGU_LLVM_LIBS})
