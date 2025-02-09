cmake_minimum_required(VERSION 3.28)
project(JMMOrganizer VERSION 0.0.0)

set(CMAKE_CXX_STANDARD 20)

add_executable(${PROJECT_NAME})

target_sources(${PROJECT_NAME}
    PRIVATE
        src/main.cpp
    PRIVATE FILE_SET CXX_MODULES FILES
        src/track_processing.cpp
        src/utilities.cpp
)
target_link_libraries(${PROJECT_NAME} be)
target_include_directories(${PROJECT_NAME}
    PRIVATE
        /boot/system/develop/headers/private/interface/
)
