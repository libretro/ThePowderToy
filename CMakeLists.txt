cmake_minimum_required(VERSION 3.9)

project(ThePowderToy)

set(CMAKE_CXX_STANDARD 11)

# Borrowed from Citra - https://github.com/citra-emu/citra
# This function should be passed a list of all files in a target. It will automatically generate
# file groups following the directory hierarchy, so that the layout of the files in IDEs matches the
# one in the filesystem.
function(create_target_directory_groups target_name)
    # Place any files that aren't in the source list in a separate group so that they don't get in
    # the way.
    source_group("Other Files" REGULAR_EXPRESSION ".")

    get_target_property(target_sources "${target_name}" SOURCES)

    foreach(file_name IN LISTS target_sources)
        get_filename_component(dir_name "${file_name}" PATH)
        # Group names use '\' as a separator even though the entire rest of CMake uses '/'...
        string(REPLACE "/" "\\" group_name "${dir_name}")
        source_group("${group_name}" FILES "${file_name}")
    endforeach()
endfunction()

# Borrowed from Citra - https://github.com/citra-emu/citra
# Include optimisations for MSVC code
if (MSVC)
    # /W3 - Level 3 warnings
    # /MP - Multi-threaded compilation
    # /Zi - Output debugging information
    # /Zo - enhanced debug info for optimized builds
    # /permissive- - enables stricter C++ standards conformance checks
    set(CMAKE_C_FLAGS   "/W3 /MP /Zi /Zo /permissive-" CACHE STRING "" FORCE)
    # /EHsc - C++-only exception handling semantics
    # /Zc:throwingNew - let codegen assume `operator new` will never return null
    # /Zc:inline - let codegen omit inline functions in object files
    set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} /EHsc /Zc:throwingNew,inline" CACHE STRING "" FORCE)

    # /MDd - Multi-threaded Debug Runtime DLL
    set(CMAKE_C_FLAGS_DEBUG   "/Od /MDd" CACHE STRING "" FORCE)
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}" CACHE STRING "" FORCE)

    # /O2 - Optimization level 2
    # /GS- - No stack buffer overflow checks
    # /MD - Multi-threaded runtime DLL
    set(CMAKE_C_FLAGS_RELEASE   "/O2 /GS- /MD" CACHE STRING "" FORCE)
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}" CACHE STRING "" FORCE)

    set(CMAKE_EXE_LINKER_FLAGS_DEBUG   "/DEBUG /MANIFEST:NO" CACHE STRING "" FORCE)
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "/DEBUG /MANIFEST:NO /INCREMENTAL:NO /OPT:REF,ICF" CACHE STRING "" FORCE)
endif()

add_subdirectory(externals)
include_directories(src)
add_subdirectory(src)
