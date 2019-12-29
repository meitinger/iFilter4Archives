add_library(streams STATIC "BridgeStream.cpp" "ReadStreams.cpp" "WriteStreams.cpp")
target_include_directories(streams PUBLIC ".")
target_link_libraries(streams com native)
