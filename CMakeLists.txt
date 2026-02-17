cmake_minimum_required(VERSION 4.1)
project(CLI)

set(CMAKE_CXX_STANDARD 20)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    #-- Set the basic build options
    add_compile_options(-Wall -Wextra -Werror -save-temps=obj -fPIC)
    add_link_options(-Wl,--no-undefined)

    #-- Garbage collect sections
    add_compile_options(-ffunction-sections -fdata-sections -fno-unique-section-names)
    add_link_options(-Wl,--gc-sections)
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    #-- Set the basic build options
    add_compile_options(-Wall -Wextra -Werror -fPIC)
    add_link_options(-Wl,--no-undefined)

    #-- Garbage collect sections
    add_compile_options(-ffunction-sections -fdata-sections)
    add_link_options(-Wl,--gc-sections)
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Intel")
    # using Intel C++
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    # using Visual Studio C++
    include_directories(".")
endif()

add_executable(${PROJECT_NAME} main.cpp CommandLine.cpp)

#-- Add getopt.c for MSVC, as it does not have a built-in getopt implementation
if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
target_sources(${PROJECT_NAME} PRIVATE getopt.c)
endif()
