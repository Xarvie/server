PROJECT(main)
cmake_minimum_required (VERSION 3.2)
set(CMAKE_CXX_STANDARD 17)
list(APPEND CMAKE_MODULE_PATH
  "${CMAKE_SOURCE_DIR}/cmake/macros")
include(AutoCollect)
# set(CMAKE_CL_64 1)
# cd build
# cmake .. -G "MinGW Makefiles" -DCMAKE_SH="CMAKE_SH-NOTFOUND"
PickIncludeDirectories(
    ${CMAKE_CURRENT_SOURCE_DIR}/code
    INCLUDEDIRS
    # Exclude
    ${CMAKE_CURRENT_SOURCE_DIR}/build)

PickSourceFiles(
    ${CMAKE_CURRENT_SOURCE_DIR}/code
    CODEFILES
    # Exclude
    ${CMAKE_CURRENT_SOURCE_DIR}/build)

MakeFilter(
    ${CMAKE_CURRENT_SOURCE_DIR}/code)

IF(CMAKE_SYSTEM_NAME MATCHES "Linux")
    set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -g")
ENDIF()

ADD_EXECUTABLE(main
    ${CODEFILES})
target_include_directories(main
    PUBLIC ${INCLUDEDIRS})

target_link_libraries(main pthread)