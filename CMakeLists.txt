cmake_minimum_required(VERSION 3.8)
project(pangu)
set(CXX_FLAGS
        -g
        # -DVALGRIND
        -DCHECK_PTHREAD_RETURN_VALUE
        -D_FILE_OFFSET_BITS=64
        -Wall
        -Wextra
        -Werror
        -Wconversion
        -Wno-unused-parameter
        -Wold-style-cast
        -Woverloaded-virtual
        -Wpointer-arith
        -Wshadow
        -Wwrite-strings
        -march=native
        # -MMD
        -rdynamic
        # -std=c++2a
        )
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DDEBUG_MODE")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -DNDEBUG")
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  #set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -fno-omit-frame-pointer")
  #set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address -fno-omit-frame-pointer -lasan")
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
include_directories(.)
aux_source_directory(. src_files)

add_executable(pangu Application.cpp)
add_executable(pangu-lexer pangu_lexer.cpp)
add_executable(pangu-grammer pangu_grammer.cpp)

add_subdirectory(virtual_machine)
add_subdirectory(pipeline)
add_subdirectory(lexer)
add_subdirectory(grammer)
add_subdirectory(pgcodes)
target_link_libraries(pangu pgcodes grammer lexer pipeline)
target_link_libraries(pangu-grammer pgcodes grammer lexer pipeline)
target_link_libraries(pangu-lexer virtual_machine lexer pipeline)

