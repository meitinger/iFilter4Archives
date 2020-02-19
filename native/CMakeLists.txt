add_library(native STATIC "com.cpp" "registry.cpp" "settings.cpp" "win32.cpp")
target_include_directories(native PUBLIC ".")
