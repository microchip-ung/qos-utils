cmake_minimum_required(VERSION 3.4)

PROJECT(QosUserspaceTools)

find_library (LIBNL_LIBRARY nl-3)
find_library (LIBNL_GENL_LIBRARY nl-genl-3)

set(LIBNL_LIBRARIES ${LIBNL_LIBRARY} ${LIBNL_GENL_LIBRARY})

find_path (LIBNL_INCLUDE_DIR NAMES netlink/netlink.h PATH_SUFFIXES libnl3)
include_directories(include ${LIBNL_INCLUDE_DIR})

include_directories(src)

add_executable(fp src/fp.c)
target_link_libraries(fp ${LIBNL_LIBRARIES})
install(TARGETS fp DESTINATION bin)

add_executable(psfp src/psfp.c)
target_link_libraries(psfp ${LIBNL_LIBRARIES})
install(TARGETS psfp DESTINATION bin)

add_executable(frer src/frer.c src/common.c)
target_link_libraries(frer ${LIBNL_LIBRARIES})
install(TARGETS frer DESTINATION bin)

