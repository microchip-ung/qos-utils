cmake_minimum_required(VERSION 3.4)

PROJECT(QosUserspaceTools)

find_library (LIBNL_LIBRARY nl-3)
find_library (LIBNL_GENL_LIBRARY nl-genl-3)

set(LIBNL_LIBRARIES ${LIBNL_LIBRARY} ${LIBNL_GENL_LIBRARY})

find_path (LIBNL_INCLUDE_DIR NAMES netlink/netlink.h PATH_SUFFIXES libnl3)
include_directories(include ${LIBNL_INCLUDE_DIR})

include_directories(src)

#
# Derive a version string from git and compile it into every tool (--version).
#
set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules ${CMAKE_MODULE_PATH})
include(GetGitRevisionDescription)
git_describe(VERSION --tags --always --dirty=-dirty)
string(REGEX REPLACE "^v([0-9]+)\\..*" "\\1" VERSION_MAJOR "${VERSION}")
string(REGEX REPLACE "^v[0-9]+\\.([0-9]+).*" "\\1" VERSION_MINOR "${VERSION}")
set(VERSION_SHORT "${VERSION_MAJOR}.${VERSION_MINOR}")
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake/version.c.in
               ${CMAKE_CURRENT_BINARY_DIR}/version.c)
set(version_file "${CMAKE_CURRENT_BINARY_DIR}/version.c")

add_executable(fp src/fp.c src/common.c ${version_file})
target_link_libraries(fp ${LIBNL_LIBRARIES})
install(TARGETS fp DESTINATION bin)

add_executable(psfp src/psfp.c src/common.c ${version_file})
target_link_libraries(psfp ${LIBNL_LIBRARIES})
install(TARGETS psfp DESTINATION bin)

add_executable(frer src/frer.c src/common.c ${version_file})
target_link_libraries(frer ${LIBNL_LIBRARIES})
install(TARGETS frer DESTINATION bin)

add_executable(qos src/qos.c src/common.c ${version_file})
target_link_libraries(qos ${LIBNL_LIBRARIES})
install(TARGETS qos DESTINATION bin)

add_executable(afi src/afi.c ${version_file})
target_link_libraries(afi ${LIBNL_LIBRARIES})
install(TARGETS afi DESTINATION bin)

