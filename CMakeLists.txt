# Set minimum required version of CMake
cmake_minimum_required(VERSION 3.15)

# Set name of project (as PROJECT_NAME) and C/C++ standards
project(baoclone VERSION 1.6 DESCRIPTION "BaoClone Utility")
set(CMAKE_C_STANDARD 11)
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
set_source_files_properties(main.c PROPERTIES COMPILE_FLAGS
    -DVERSION=\\"${CMAKE_PROJECT_VERSION}.${GIT_REVCOUNT}-${GIT_HASH}\\"
)

install(TARGETS
    ${PROJECT_NAME}
    DESTINATION bin
)

add_subdirectory(tests)
