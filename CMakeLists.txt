cmake_minimum_required(VERSION 3.14)
project(TradingSystem)
#
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)


add_executable(
    TradingSystem
    ./Common/src/logging.cpp
    ./Common/src/tcp_socket.cpp
    ./Common/src/tcp_server.cpp
    ./main.cpp
)

# Enable clang-format
find_program(CLANG_FORMAT clang-format)
if(CLANG_FORMAT)
    file(GLOB_RECURSE ALL_SOURCE_FILES *.cpp *.h)
    add_custom_target(
        format
        COMMAND ${CLANG_FORMAT} -i -style=file ${ALL_SOURCE_FILES}
    )
endif()

# Enable clang-tidy only for source files in specified directories
find_program(CLANG_TIDY clang-tidy)
if(CLANG_TIDY)
    set(CLANG_TIDY_CHECKS
        "modernize-use-auto"
        "readability-identifier-naming"
        "performance-inefficient-string-concatenation"
        "bugprone-bool-pointer-implicit-conversion"
    )

    # Define the directories to search for source files
    set(SOURCE_DIRS
        ./include
        ./src
    )

    # Gather all *.cpp and *.h files in the specified directories
    file(GLOB_RECURSE MY_SOURCE_FILES
        ${SOURCE_DIRS}/*.cpp
        ${SOURCE_DIRS}/*.h
    )

    add_custom_target(
        tidy
        COMMAND ${CLANG_TIDY} -checks='${CLANG_TIDY_CHECKS}' ${MY_SOURCE_FILES} -p compile_commands.json
        COMMENT "Running clang-tidy"
    )
endif()


target_include_directories(TradingSystem PUBLIC ./Common/include)

target_link_libraries(
  TradingSystem PUBLIC
  pthread
)

