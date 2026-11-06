cmake_minimum_required(VERSION 3.15.3)

project(Compiler LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(ir_core INTERFACE)

target_include_directories(ir_core INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(Compiler
        Compile.cpp
        front-end/Lexer.cpp
)

target_include_directories(Compiler PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/front-end
)

target_link_libraries(Compiler PRIVATE ir_core)

include(CTest)
if(BUILD_TESTING)
    add_executable(ir_core_tests test-case/ir/ir-core-tests.cpp)
    target_link_libraries(ir_core_tests PRIVATE ir_core)
    add_test(NAME ir_core COMMAND ir_core_tests)

    # Python/llvmlite 仅用于端到端验证，不是提交编译器的运行依赖。
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        execute_process(COMMAND "${Python3_EXECUTABLE}" -c "import llvmlite.binding"
                RESULT_VARIABLE HAVE_LLVM_PYTHON OUTPUT_QUIET ERROR_QUIET)
        if(HAVE_LLVM_PYTHON EQUAL 0)
            add_test(NAME llvm_codegen COMMAND "${Python3_EXECUTABLE}"
                    "${CMAKE_CURRENT_SOURCE_DIR}/test-case/codegen/run-codegen-tests.py"
                    --compiler $<TARGET_FILE:Compiler>)
            set_tests_properties(llvm_codegen PROPERTIES TIMEOUT 180)
        endif()
    endif()
endif()
