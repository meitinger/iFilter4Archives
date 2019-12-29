cmake_minimum_required(VERSION 3.8) # needed for C++ 17
set(CMAKE_CXX_STANDARD 17)          # enable C++ 17 and...
set(CMAKE_CXX_STANDARD_REQUIRED ON) # ...require it
add_compile_options(/DNOMINMAX)     # disable min/max in Windows.h

project(iFilter4Archives)

add_subdirectory(archive)
add_subdirectory(com)
add_subdirectory(native)
add_subdirectory(streams)

add_library(iFilter4Archives SHARED "iFilter4Archives.cpp" "iFilter4Archives.def")
target_link_libraries(iFilter4Archives com)


if (NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
    install(TARGETS iFilter4Archives RUNTIME DESTINATION bin COMPONENT application)
    install(DIRECTORY lib/${CMAKE_SYSTEM_PROCESSOR}/ DESTINATION bin COMPONENT libraries)

    set(CMAKE_INSTALL_UCRT_LIBRARIES TRUE)
    include(InstallRequiredSystemLibraries)

    set(CPACK_GENERATOR WIX)
    set(CPACK_PACKAGE_VENDOR "Manuel Meitinger")
    set(CPACK_PACKAGE_VERSION "1.0.0")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Windows Search iFilter for 7-Zip and other archives.")
    #set(CPACK_PACKAGE_ICON )
    set(CPACK_WIX_UPGRADE_GUID "E1F45AF2-8437-45AA-B702-49263BC8B1BA")
    set(CPACK_WIX_PROPERTY_ARPNOMODIFY 1)
    #set(CPACK_WIX_PRODUCT_ICON )
    include(CPack)
endif ()
