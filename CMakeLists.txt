cmake_minimum_required (VERSION 2.6)
project (esort)

if(UNIX)
    add_compile_options( -std=c++11 -Wall -pthread)
elseif(WIN32)
    add_compile_options( /W4 /D_CRT_SECURE_NO_WARNINGS )
endif()

include_directories(${PROJECT_SOURCE_DIR}/inc)

add_library(common  ${PROJECT_SOURCE_DIR}/inc/FileReader.h
                    ${PROJECT_SOURCE_DIR}/src/DataTypes.cpp
                    ${PROJECT_SOURCE_DIR}/inc/DataTypes.h
                    ${PROJECT_SOURCE_DIR}/src/Utils.cpp
                    ${PROJECT_SOURCE_DIR}/inc/Utils.h
                    ${PROJECT_SOURCE_DIR}/inc/ProgressIndicator.h
                    ${PROJECT_SOURCE_DIR}/inc/Algorithms.h)

add_executable(ecollect ${PROJECT_SOURCE_DIR}/src/ecollect.cpp
                        ${PROJECT_SOURCE_DIR}/inc/Hash.h)
TARGET_LINK_LIBRARIES(ecollect common)

add_executable(esort ${PROJECT_SOURCE_DIR}/src/esort.cpp
                     ${PROJECT_SOURCE_DIR}/src/Tuple.cpp
                     ${PROJECT_SOURCE_DIR}/inc/Tuple.h)
TARGET_LINK_LIBRARIES(esort common)

add_executable(eshuffle ${PROJECT_SOURCE_DIR}/src/eshuffle.cpp)
TARGET_LINK_LIBRARIES(eshuffle common)

if(UNIX)
    TARGET_LINK_LIBRARIES(ecollect pthread)
    TARGET_LINK_LIBRARIES(eshuffle pthread)
    TARGET_LINK_LIBRARIES(esort pthread)
endif()
