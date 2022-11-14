cmake_minimum_required(VERSION 3.9)

project (file_hash)

set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)

if(WIN32)
    set(CMAKE_CXX_FLAGS "/WX- /MP")
    set(CMAKE_CXX_FLAGS_DEBUG "/W3 /Zi /Od /WX- ${CMAKE_CXX_FLAGS}")
    set(CMAKE_CXX_FLAGS_RELEASE "/Ob1 /Ot /Oi /Oy /GL /arch:AVX ${CMAKE_CXX_FLAGS}")
    add_compile_options(/Ob2 /DNDEBUG /O2 /Ot /Oi /Oy /GL /arch:AVX)
elseif()
    set(CMAKE_CXX_FLAGS_DEBUG "-g -fno-strict-aliasing ${CMAKE_CXX_FLAGS}")
    set(CMAKE_CXX_FLAGS_RELEASE "-DNDEBUG -O2 -s -mtune=native ${CMAKE_CXX_FLAGS}")
endif()

add_executable(file_hash src/main.cpp src/mm_file.cpp)
target_include_directories(file_hash PUBLIC "src")
