cmake_minimum_required(VERSION 3.8)
project(moon-engine)
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
		-std=c++2a
        )

if (POLICY CMP0072)
  SET(OpenGL_GL_PREFERENCE LEGACY)
endif()


set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
include_directories(.)
aux_source_directory(. src_files)

add_executable(pangu ${src_files})

add_subdirectory(virtual_machine)
add_subdirectory(pipeline)
target_link_libraries(pangu virtual_machine pipeline)

