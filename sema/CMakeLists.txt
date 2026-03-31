aux_source_directory(. DIR_LIB_SRCS)
add_library(sema ${DIR_LIB_SRCS})
target_include_directories(sema PRIVATE ${CMAKE_SOURCE_DIR})

