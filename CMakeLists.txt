cmake_minimum_required(VERSION 3.15) # needed for CMAKE_MSVC_RUNTIME_LIBRARY
cmake_policy(SET CMP0091 NEW)        # enable new MSVC runtime library flags behavior
set(CMAKE_CXX_STANDARD 17)           # enable C++ 17 and...
set(CMAKE_CXX_STANDARD_REQUIRED ON)  # ...require it
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>") # force static builds
add_compile_options(/DNOMINMAX)      # disable min/max in Windows.h

project(iFilter4Archives)

add_subdirectory(archive)
add_subdirectory(com)
add_subdirectory(native)
add_subdirectory(streams)

add_library(iFilter4Archives SHARED "iFilter4Archives.cpp" "iFilter4Archives.rc" "iFilter4Archives.def")
target_link_libraries(iFilter4Archives com native)

install(CODE "execute_process(WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/installer COMMAND msbuild -p:Platform=$<IF:$<EQUAL:${CMAKE_SIZEOF_VOID_P},4>,x86,x64> -p:Configuration=$<IF:$<CONFIG:Debug>,Debug,Release>)")
