cmake_minimum_required(VERSION 3.10)
project(terrain CXX)

#### Parameter set-up and library support detection.

if ("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
    set(CMAKE_CXX_FLAGS_RELEASE "-Os -DNDEBUG -Wall")
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-Os -g -DNDEBUG -Wall")
    set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g -Wall")
    set(ProfilerOptions -fprofile-instr-generate)
    set(ProfilerLinkOptions -fprofile-instr-generate)
    set(CxxStd -std=c++17)
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG -Wall")
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O2 -g -DNDEBUG -Wall")
    set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g -Wall")
    set(ProfilerOptions -pg)
    set(ProfilerLinkOptions -pg)
    set(CxxStd -std=c++17)
endif()

#### Main programs

set(Programs generatechanges slowrenderchanges renderchanges heightfield2color heightfield2model heightfield2texture)

add_custom_target(parsers COMMENT "Generating types from README.md"
    DEPENDS ${CMAKE_CURRENT_LIST_DIR}/README.md
    COMMAND edicta -i ${CMAKE_CURRENT_LIST_DIR}/README.md -o pspecs render_io generate_io heightfield2color_io heightfield2model_io heightfield2texture_io
    COMMAND specificjson --input pspecs
    BYPRODUCTS render_io.cpp render_io.hpp generate_io.cpp generate_io.hpp heightfield2color_io.cpp heightfield2color_io.hpp heightfield2model_io.cpp heightfield2model_io.hpp heightfield2texture_io.cpp heightfield2texture_io.hpp)

function(setup_main_program TGTNAME MAIN IO)
    add_executable(${TGTNAME} ${MAIN} ${CMAKE_CURRENT_BINARY_DIR}/${IO}.cpp ${ARGN})
    add_dependencies(${TGTNAME} parsers)
    target_include_directories(${TGTNAME} SYSTEM PRIVATE /usr/local/include)
    target_include_directories(${TGTNAME} PRIVATE src)
    target_include_directories(${TGTNAME} PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
    target_compile_options(${TGTNAME} PRIVATE ${CxxStd})
endfunction()

setup_main_program(generatechanges src/generatechanges.cpp generate_io)
setup_main_program(slowrenderchanges src/slowrenderchanges.cpp render_io)
setup_main_program(renderchanges src/renderchanges.cpp render_io)
setup_main_program(referencerenderchanges src/referencerenderchanges.cpp render_io)
setup_main_program(heightfield2color src/heightfield2color.cpp heightfield2color_io src/colormap.cpp)
setup_main_program(heightfield2model src/heightfield2model.cpp heightfield2model_io src/colormap.cpp)
setup_main_program(heightfield2texture src/heightfield2texture.cpp heightfield2texture_io src/colormap.cpp)

install(TARGETS ${Programs} RUNTIME DESTINATION bin)

#### Tests

enable_testing()

function(setup_unittest_program TGTNAME MAIN IO)
    add_executable(${TGTNAME} ${MAIN} ${CMAKE_CURRENT_BINARY_DIR}/${IO}.cpp ${ARGN})
    add_dependencies(${TGTNAME} parsers)
    target_include_directories(${TGTNAME} SYSTEM PRIVATE /usr/local/include)
    target_include_directories(${TGTNAME} PRIVATE src)
    target_include_directories(${TGTNAME} PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
    target_compile_definitions(${TGTNAME} PRIVATE UNITTEST)
    target_compile_options(${TGTNAME} PRIVATE ${CxxStd})
    add_test(NAME ${TGTNAME} COMMAND ${TGTNAME})
endfunction()

setup_unittest_program(unittest-generate src/generatechanges.cpp generate_io)
setup_unittest_program(unittest-slowrender src/slowrenderchanges.cpp render_io)
setup_unittest_program(unittest-render src/renderchanges.cpp render_io)
setup_unittest_program(unittest-referencerender src/referencerenderchanges.cpp render_io)
setup_unittest_program(unittest-heightfield2color src/heightfield2color.cpp heightfield2color_io src/colormap.cpp)
setup_unittest_program(unittest-heightfield2model src/heightfield2model.cpp heightfield2model_io src/colormap.cpp)
setup_unittest_program(unittest-heightfield2texture src/heightfield2texture.cpp heightfield2texture_io src/colormap.cpp)

function(add_test_prog PROG)
    add_executable(${PROG} IMPORTED)
    set_property(TARGET ${PROG} PROPERTY IMPORTED_LOCATION ${CMAKE_CURRENT_LIST_DIR}/test/${PROG})
endfunction()

function(new_test TEST_NAME PROG EXECUTABLE INPUT LOWR HIGHR LOWO HIGHO)
    add_test(NAME ${TEST_NAME} COMMAND ${PROG} ${EXECUTABLE} ${CMAKE_CURRENT_LIST_DIR}/test/${INPUT} ${LOWR} ${HIGHR} ${LOWO} ${HIGHO})
    set_property(TEST ${TEST_NAME} PROPERTY ENVIRONMENT "PATH=${CMAKE_CURRENT_LIST_DIR}:${CMAKE_CURRENT_LIST_DIR}/test:$ENV{PATH}")
endfunction()

function(new_test4 TEST_NAME PROG EXECUTABLE INPUT LOWR HIGHR LOWO HIGHO A1 A2 A3 A4)
    add_test(NAME ${TEST_NAME} COMMAND ${PROG} ${EXECUTABLE} ${CMAKE_CURRENT_LIST_DIR}/test/${INPUT} ${LOWR} ${HIGHR} ${LOWO} ${HIGHO} ${A1} ${A2} ${A3} ${A4})
    set_property(TEST ${TEST_NAME} PROPERTY ENVIRONMENT "PATH=${CMAKE_CURRENT_LIST_DIR}:${CMAKE_CURRENT_LIST_DIR}/test:$ENV{PATH}")
endfunction()

add_test_prog(check.sh)
new_test(minmax check.sh $<TARGET_FILE:generatechanges> minmax.json 0.5 0.5 0.5 0.5)
new_test(minrange check.sh $<TARGET_FILE:generatechanges> minrange.json 0.5 1.0 -0.5 0.5)
new_test(maxrange check.sh $<TARGET_FILE:generatechanges> maxrange.json 0 0.5 -0.5 0.5)
new_test(range check.sh $<TARGET_FILE:generatechanges> range.json 0 0.5 -1 0)
new_test(min check.sh $<TARGET_FILE:generatechanges> min.json 0.5 1.5 0.5 2.5)
new_test(max check.sh $<TARGET_FILE:generatechanges> max.json -0.5 0.5 -1.5 0.5)
