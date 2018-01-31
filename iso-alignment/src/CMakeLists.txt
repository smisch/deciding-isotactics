cmake_minimum_required(VERSION 3.0)

include_directories(${Boost_INCLUDE_DIRS})
include_directories(${isotactics_SOURCE_DIR}/iso-alignment/include)
include_directories(${isotactics_SOURCE_DIR}/iso-lib/include)

link_directories(${isotactics_SOURCE_DIR}/iso-lib/src)



file(GLOB SRC_FILES *.cpp)

add_executable(iso-alignment ${SRC_FILES})

target_link_libraries(iso-alignment iso-lib ${Boost_LIBRARIES})
