# Set minimum required version of CMake
cmake_minimum_required(VERSION 3.15)

# Set name of project (as PROJECT_NAME) and C/C++ standards
project(baoclone VERSION 1.6 DESCRIPTION "BaoClone Utility")
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
add_compile_options(-Wall -Werror -Wshadow)

# Build library
add_library(radio STATIC
    bf-888s.c
    bf-t1.c
    radio.c
    util.c
    uv-5r.c
    uv-b5.c
)

# Build executable file
add_executable(${PROJECT_NAME} main.c)
target_link_libraries(${PROJECT_NAME} radio)

# Get git commit hash and revision count
execute_process(
    COMMAND git log -1 --format=%h
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_STRIP_TRAILING_WHITESPACE
    OUTPUT_VARIABLE GIT_HASH
)
execute_process(
    COMMAND git rev-list HEAD --count
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_STRIP_TRAILING_WHITESPACE
    OUTPUT_VARIABLE GIT_REVCOUNT
)
set_source_files_properties(radio.c PROPERTIES COMPILE_FLAGS
    -DVERSION=\\"${CMAKE_PROJECT_VERSION}.${GIT_REVCOUNT}-${GIT_HASH}\\"
)

install(TARGETS
    ${PROJECT_NAME}
    DESTINATION bin
)

#--------------------------------------------------------------
# GoogleTest
#
include(FetchContent)
include(ExternalProject)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG release-1.12.1
)
set(BUILD_SHARED_LIBS ON)
FetchContent_GetProperties(googletest)
if(NOT googletest_POPULATED)
    FetchContent_Populate(googletest)
    set(BUILD_GMOCK OFF)
    set(INSTALL_GTEST OFF)
endif()

add_subdirectory(${googletest_SOURCE_DIR} ${googletest_BINARY_DIR})

include(GoogleTest)
enable_testing()
set_target_properties(gtest PROPERTIES DEBUG_POSTFIX "")
set_target_properties(gtest_main PROPERTIES DEBUG_POSTFIX "")
set_target_properties(gtest PROPERTIES EXCLUDE_FROM_ALL TRUE)
set_target_properties(gtest_main PROPERTIES EXCLUDE_FROM_ALL TRUE)
file(WRITE "${CMAKE_BINARY_DIR}/CTestCustom.cmake"
    "set(CTEST_CUSTOM_PRE_TEST \"${CMAKE_MAKE_PROGRAM} unit_tests\")\n"
)

add_subdirectory(tests)
