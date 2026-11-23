cmake_minimum_required(VERSION 3.16)
project(ChocoPy LANGUAGES CXX)

set(grammar_file "${CMAKE_CURRENT_SOURCE_DIR}/parser/src/input.txt")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${grammar_file}")
file(READ "${grammar_file}" CHOCOPY_GRAMMAR)
configure_file(frontend/src/ChocoPyGrammar.h.in generated/ChocoPyGrammar.h @ONLY)

add_executable(chocopy_core
    frontend/src/main.cpp
    frontend/src/Frontend.cpp
    frontend/src/ChocoPyAst.cpp
    frontend/src/AstJson.cpp
    parser/src/Grammar.cpp
    parser/src/LR1.cpp
    scanner/src/scanner.cpp
    semantics/src/SymbolTable.cpp
    semantics/src/TypeChecker.cpp
)
target_compile_features(chocopy_core PRIVATE cxx_std_17)
target_include_directories(chocopy_core PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
set_target_properties(chocopy_core PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "$<1:${CMAKE_CURRENT_BINARY_DIR}/bin>"
)

if(MINGW)
    target_link_options(chocopy_core PRIVATE -municode)
endif()

include(CTest)
if(BUILD_TESTING)
    foreach(suite IN ITEMS diagnostics semantics export)
        add_test(NAME chocopy_${suite}
            COMMAND "${CMAKE_COMMAND}"
                "-DCOMPILER=$<TARGET_FILE:chocopy_core>"
                "-DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/tests/${suite}"
                "-DSUITE=${suite}"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/frontend.cmake"
        )
    endforeach()
endif()
