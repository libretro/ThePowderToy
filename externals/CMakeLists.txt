# LibRetro
add_library(libretro INTERFACE)
target_include_directories(libretro INTERFACE ./libretro)

# dirent
add_library(generic INTERFACE)
target_include_directories(generic INTERFACE ./generic)

# BSON
add_subdirectory(bson)
target_include_directories(bson INTERFACE ./bson)

# Lua
add_subdirectory(lua)
target_include_directories(lua INTERFACE ./lua)

# zlib
add_subdirectory(zlib)

# bzip2
add_subdirectory(bzip2)

# winpthreads
if (MSVC)
    set(PTHREADS_BUILD_STATIC OFF CACHE BOOL "" FORCE)
    add_definitions(-D_TIMESPEC_DEFINED)
    add_subdirectory(winpthreads)
    #target_include_directories(${PTHREADS_LIBRARY} INTERFACE ./winpthreads/pthreads-win32)
endif()
