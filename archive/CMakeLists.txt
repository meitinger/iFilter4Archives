add_library(archive STATIC "Factory.cpp" "Format.cpp" "Module.cpp")
target_include_directories(archive PUBLIC ".")
target_link_libraries(archive native)
