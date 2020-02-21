cmake_minimum_required(VERSION 3.8) # needed for C++ 17
set(CMAKE_CXX_STANDARD 17)          # enable C++ 17 and...
set(CMAKE_CXX_STANDARD_REQUIRED ON) # ...require it
add_compile_options(/DNOMINMAX)     # disable min/max in Windows.h

# force static builds (don't add additional dependencies when deployed together with 7-Zip)
string(TOUPPER "${CMAKE_BUILD_TYPE}" config)
string(REGEX REPLACE "/MD(d?)" "/MT\\1" CMAKE_C_FLAGS_${config}   "${CMAKE_C_FLAGS_${config}}"  )
string(REGEX REPLACE "/MD(d?)" "/MT\\1" CMAKE_CXX_FLAGS_${config} "${CMAKE_CXX_FLAGS_${config}}")

project(iFilter4Archives)

add_subdirectory(archive)
add_subdirectory(com)
add_subdirectory(native)
add_subdirectory(streams)

add_library(iFilter4Archives SHARED "iFilter4Archives.cpp" "iFilter4Archives.rc" "iFilter4Archives.def")
target_link_libraries(iFilter4Archives com native)
