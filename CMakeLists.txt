cmake_minimum_required(VERSION 3.14)
project(libretro_core_glad_lua C)

include(FetchContent)

# Fetch libretro-common
FetchContent_Declare(
    libretro-common
    GIT_REPOSITORY https://github.com/libretro/libretro-common.git
    GIT_TAG master
)
FetchContent_MakeAvailable(libretro-common)

# Fetch glad
FetchContent_Declare(
    glad
    GIT_REPOSITORY https://github.com/Dav1dde/glad.git
    GIT_TAG v0.1.36
)
FetchContent_MakeAvailable(glad)
set(GLAD_PROFILE "core" CACHE STRING "OpenGL profile")
set(GLAD_API "gl=3.3" CACHE STRING "API type/version")
set(GLAD_GENERATOR "c" CACHE STRING "Language to generate")

# Fetch Lua
FetchContent_Declare(
    lua
    GIT_REPOSITORY https://github.com/lua/lua.git
    GIT_TAG v5.4.7
)
FetchContent_Populate(lua)

# Fetch miniz
FetchContent_Declare(
    miniz
    GIT_REPOSITORY https://github.com/richgel999/miniz.git
    GIT_TAG 3.0.2
)
FetchContent_MakeAvailable(miniz)

# Lua source files
set(LUA_SRC
    ${lua_SOURCE_DIR}/lapi.c
    ${lua_SOURCE_DIR}/lauxlib.c
    ${lua_SOURCE_DIR}/lbaselib.c
    ${lua_SOURCE_DIR}/lcode.c
    ${lua_SOURCE_DIR}/lcorolib.c
    ${lua_SOURCE_DIR}/lctype.c
    ${lua_SOURCE_DIR}/ldblib.c
    ${lua_SOURCE_DIR}/ldebug.c
    ${lua_SOURCE_DIR}/ldo.c
    ${lua_SOURCE_DIR}/ldump.c
    ${lua_SOURCE_DIR}/lfunc.c
    ${lua_SOURCE_DIR}/lgc.c
    ${lua_SOURCE_DIR}/linit.c
    ${lua_SOURCE_DIR}/liolib.c
    ${lua_SOURCE_DIR}/llex.c
    ${lua_SOURCE_DIR}/lmathlib.c
    ${lua_SOURCE_DIR}/lmem.c
    ${lua_SOURCE_DIR}/loadlib.c
    ${lua_SOURCE_DIR}/lobject.c
    ${lua_SOURCE_DIR}/lopcodes.c
    ${lua_SOURCE_DIR}/loslib.c
    ${lua_SOURCE_DIR}/lparser.c
    ${lua_SOURCE_DIR}/lstate.c
    ${lua_SOURCE_DIR}/lstring.c
    ${lua_SOURCE_DIR}/lstrlib.c
    ${lua_SOURCE_DIR}/ltable.c
    ${lua_SOURCE_DIR}/ltablib.c
    ${lua_SOURCE_DIR}/ltm.c
    ${lua_SOURCE_DIR}/lundump.c
    ${lua_SOURCE_DIR}/lvm.c
    ${lua_SOURCE_DIR}/lzio.c
    ${lua_SOURCE_DIR}/lutf8lib.c
)

# Create a static Lua library
add_library(lua STATIC ${LUA_SRC})
target_include_directories(lua PUBLIC 
    ${lua_SOURCE_DIR}
)

# Testing for OpenGL
set(USE_OPENGL ON)

# lrcgl library
add_library(lrcgl SHARED 
    src/lib.c
    src/module_lua.c
    src/module_opengl.c

    ${miniz_SOURCE_DIR}/miniz.c
    ${miniz_SOURCE_DIR}/miniz_tdef.c
    ${miniz_SOURCE_DIR}/miniz_tinfl.c
    ${miniz_SOURCE_DIR}/miniz_zip.c
)

# Link libraries
target_link_libraries(lrcgl PRIVATE 
    glad
    lua
)

# OpenGL
if(WIN32 AND USE_OPENGL)
    target_link_libraries(lrcgl PRIVATE opengl32)
endif()

# Include directories
target_include_directories(lrcgl PRIVATE
    ${libretro-common_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${glad_SOURCE_DIR}/include
    ${lua_SOURCE_DIR}
    ${miniz_SOURCE_DIR}
    ${miniz_BINARY_DIR}
)

# Compile definitions
target_compile_definitions(lrcgl PRIVATE 
    _CRT_SECURE_NO_WARNINGS
)
if(USE_OPENGL)
    target_compile_definitions(lrcgl PRIVATE USE_OPENGL)
endif()

# lrcgl.dll
set_target_properties(lrcgl PROPERTIES
    PREFIX ""
    OUTPUT_NAME "libretro_core_glad_lua"
    SUFFIX ".dll"
)
set_property(TARGET lrcgl PROPERTY C_STANDARD 99)