# CMakeLists.txt
cmake_minimum_required (VERSION 3.8.2)
project (gpmf-parser)

set(CMAKE_SUPPRESS_REGENERATION true)
set(CMAKE_CONFIGURATION_TYPES "Debug;Release")

file(GLOB LIB_SOURCES "GPMF_parser.c" "GPMF_utils.c")
file(GLOB SOURCES ${LIB_SOURCES} "demo/GPMF_demo.c" "demo/GPMF_print.c" "demo/GPMF_mp4reader.c")

add_executable(GPMF_PARSER_BIN ${SOURCES})
set_target_properties(GPMF_PARSER_BIN PROPERTIES OUTPUT_NAME "${PROJECT_NAME}")
add_library(GPMF_PARSER_LIB ${LIB_SOURCES})
set_target_properties(GPMF_PARSER_LIB PROPERTIES OUTPUT_NAME "${PROJECT_NAME}")

set(PC_LINK_FLAGS "-l${PROJECT_NAME}")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT_NAME}.pc.in" "${CMAKE_BINARY_DIR}/${PROJECT_NAME}.pc" @ONLY)

install(TARGETS GPMF_PARSER_BIN DESTINATION "bin")
install(TARGETS GPMF_PARSER_LIB DESTINATION "lib")
install(FILES "${CMAKE_BINARY_DIR}/${PROJECT_NAME}.pc" DESTINATION "lib/pkgconfig")

install(FILES "GPMF_parser.h" "GPMF_common.h" "GPMF_utils.h" DESTINATION "include/gpmf-parser")
install(FILES "demo/GPMF_mp4reader.h" DESTINATION "include/gpmf-parser/demo")
