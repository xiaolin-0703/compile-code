cmake_minimum_required(VERSION 4.1)
project(code)

set(CMAKE_CXX_STANDARD 14)

include_directories(src/front-end)

add_executable(code
        src/front-end/compileError.hpp
        src/front-end/Lexer.cpp
        src/front-end/token.hpp
        src/Compile.cpp
        src/front-end/Lexer.hpp)
