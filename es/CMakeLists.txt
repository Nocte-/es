cmake_minimum_required (VERSION 2.8.3)
set(LIBNAME es)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/version.hpp.in ${CMAKE_CURRENT_SOURCE_DIR}/version.hpp)
file(GLOB SOURCE_FILES "*.cpp")
file(GLOB HEADER_FILES "*.hpp")

add_custom_target(format COMMAND clang-format-3.5 -style=file -i ${SOURCE_FILES} ${HEADER_FILES})

set (LIBNAME_S "${LIBNAME}-s")
add_library(${LIBNAME_S} STATIC ${SOURCE_FILES} ${HEADER_FILES})
add_library(${LIBNAME}   SHARED ${SOURCE_FILES} ${HEADER_FILES})
set_target_properties(${LIBNAME} PROPERTIES SOVERSION ${VERSION_SO} VERSION ${VERSION})
set_target_properties(${LIBNAME_S} PROPERTIES VERSION ${VERSION})

install(TARGETS ${LIBNAME_S} ${LIBNAME} DESTINATION lib)
install(FILES ${HEADER_FILES} DESTINATION include/es)

