add_library(com STATIC "CachedChunk.cpp" "ClassFactory.cpp" "FileDescription.cpp" "Filter.cpp" "ItemTask.cpp" "Registrar.cpp")
target_include_directories(com PUBLIC ".")
target_link_libraries(com archive native streams)
