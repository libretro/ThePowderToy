# LibRetro
add_library(libretro INTERFACE)
target_include_directories(libretro INTERFACE ./libretro)

# BSON
add_subdirectory(bson)
target_include_directories(bson INTERFACE ./bson)