cmake_minimum_required(VERSION 3.14)
project(libretro_core_glad_lua C)
include(FetchContent)
FetchContent_Declare(
    libretro-common
    GIT_REPOSITORY https://github.com/libretro/libretro-common.git
    GIT_TAG master
)
FetchContent_MakeAvailable(libretro-common)
FetchContent_Declare(
    glad
    GIT_REPOSITORY https://github.com/Dav1dde/glad.git
    GIT_TAG v0.1.36
)
FetchContent_MakeAvailable(glad)
set(GLAD_PROFILE "core" CACHE STRING "OpenGL profile")
set(GLAD_API "gl=3.3" CACHE STRING "API type/version")
set(GLAD_GENERATOR "c" CACHE STRING "Language to generate")

# Fetch Lua (source only, no CMake build)
FetchContent_Declare(
    lua
    GIT_REPOSITORY https://github.com/lua/lua.git
    GIT_TAG v5.4.7
)
FetchContent_Populate(lua)

# Lua doesn't have a CMake build, so we manually compile it
# List Lua source files (core and libraries, excluding lua.c and luac.c)
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

# testing for opengl and software render toggle
set(USE_OPENGL ON)
# lrcgl library
add_library(lrcgl SHARED src/lib.c)
# glad
target_link_libraries(lrcgl PRIVATE 
  glad
  lua
)
# opengl
if(WIN32 AND USE_OPENGL)
    target_link_libraries(lrcgl PRIVATE opengl32)
endif()
# include folder for headers
target_include_directories(lrcgl PRIVATE
    ${libretro-common_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${glad_SOURCE_DIR}/include
    ${lua_SOURCE_DIR}
)
# compile definitions
target_compile_definitions(lrcgl PRIVATE _CRT_SECURE_NO_WARNINGS)
if(USE_OPENGL)
    target_compile_definitions(lrcgl PRIVATE USE_OPENGL)
endif()
# lrcgl.dll
set_target_properties(lrcgl PROPERTIES
    PREFIX ""
    OUTPUT_NAME "libretro_core_glad_lua" # file name
    SUFFIX ".dll"
)
set_property(TARGET lrcgl PROPERTY C_STANDARD 99)