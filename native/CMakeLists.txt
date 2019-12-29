add_library(native STATIC "com.cpp" "object.cpp" "registry.cpp" "settings.cpp" "win32.cpp")
target_include_directories(native PUBLIC ".")
