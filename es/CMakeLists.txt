
cmake_minimum_required (VERSION 2.8.3)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/version.hpp.in ${CMAKE_CURRENT_SOURCE_DIR}/version.hpp)
file(GLOB HEADER_FILES "*.hpp")

install(FILES ${HEADER_FILES} DESTINATION include/es)

