cmake_minimum_required(VERSION 3.16)
project(ign_proto C)

add_library(ign_proto STATIC
        src/ign_proto.c
        src/ign_proto_framing.c
        src/ign_proto_core.c
)

target_include_directories(ign_proto PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/include
)

target_compile_features(ign_proto PUBLIC c_std_11)

# Mandatory firmware-specific command dispatch implementation file.
# Consumer must set:
# set(IGN_PROTO_CMD_FILE /path/to/boot_cmd.c)  or cfg_cmd.c
if (NOT DEFINED IGN_PROTO_CMD_FILE)
    message(FATAL_ERROR "IGN_PROTO_CMD_FILE not defined. Set it to path of your xxx_cmd.c")
endif ()

target_sources(ign_proto PRIVATE ${IGN_PROTO_CMD_FILE})