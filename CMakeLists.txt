cmake_minimum_required(VERSION 3.5)
project(mdmni LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(mdmni
    src/main.cpp
    src/Renderer.cpp
)

target_include_directories(mdmni PRIVATE src)
