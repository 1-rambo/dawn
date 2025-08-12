# BUILD.cmake for libprotobuf-mutator
# This file provides CMake configuration for libprotobuf-mutator

# Since libprotobuf-mutator has complex dependencies and is mainly used for fuzzing,
# we'll create dummy targets to avoid build issues for regular Dawn compilation

message(STATUS "Creating dummy libprotobuf-mutator targets (fuzzing disabled)")

# Create dummy interface targets that other parts of Dawn might depend on
add_library(libprotobuf-mutator INTERFACE)
add_library(protobuf-mutator INTERFACE)
add_library(libprotobuf-mutator-libfuzzer INTERFACE)

# Set some properties that might be expected
set_target_properties(libprotobuf-mutator PROPERTIES
    INTERFACE_COMPILE_DEFINITIONS "LPM_DUMMY_TARGET=1"
)

set_target_properties(protobuf-mutator PROPERTIES  
    INTERFACE_COMPILE_DEFINITIONS "LPM_DUMMY_TARGET=1"
)

set_target_properties(libprotobuf-mutator-libfuzzer PROPERTIES
    INTERFACE_COMPILE_DEFINITIONS "LPM_DUMMY_TARGET=1"
)
