add_library(streams STATIC "BridgeStream.cpp" "FileBuffer.cpp" "ReadStream.cpp" "WriteStream.cpp")
target_include_directories(streams PUBLIC ".")
target_link_libraries(streams com native)
