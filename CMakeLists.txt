cmake_minimum_required(VERSION 3.20)
project(vip-coupon-system VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}/bin")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/bin")

include(FetchContent)
FetchContent_Declare(json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.12.0)
FetchContent_Declare(ftxui
    GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI.git
    GIT_TAG v7.0.3)
FetchContent_MakeAvailable(json ftxui)

file(GLOB_RECURSE CORE_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
list(FILTER CORE_SOURCES EXCLUDE REGEX "/main\\.cpp$")
add_library(vip_core STATIC ${CORE_SOURCES})
target_compile_features(vip_core PUBLIC cxx_std_17)
set_target_properties(vip_core PROPERTIES CXX_EXTENSIONS OFF)
target_include_directories(vip_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_definitions(vip_core PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN)
target_link_libraries(vip_core PUBLIC nlohmann_json::nlohmann_json ftxui::screen ftxui::dom ftxui::component)
if(MSVC)
    target_compile_options(vip_core PRIVATE /W4 /utf-8 /permissive-)
else()
    target_compile_options(vip_core PRIVATE -Wall -Wextra)
endif()

add_executable(vip-coupon-system src/main.cpp)
target_link_libraries(vip-coupon-system PRIVATE vip_core)
if(MSVC)
    target_compile_options(vip-coupon-system PRIVATE /W4 /utf-8 /permissive-)
endif()
add_custom_target(run COMMAND $<TARGET_FILE:vip-coupon-system>
    DEPENDS vip-coupon-system WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

enable_testing()
file(GLOB TEST_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp")
foreach(TEST_SRC ${TEST_SOURCES})
    get_filename_component(TEST_NAME ${TEST_SRC} NAME_WE)
    add_executable(${TEST_NAME} ${TEST_SRC})
    target_link_libraries(${TEST_NAME} PRIVATE vip_core)
    if(MSVC)
        target_compile_options(${TEST_NAME} PRIVATE /W4 /utf-8 /permissive- /UNDEBUG)
    else()
        target_compile_options(${TEST_NAME} PRIVATE -Wall -Wextra -UNDEBUG)
    endif()
    set_target_properties(${TEST_NAME} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/tests"
        RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin/tests"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin/tests")
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
endforeach()

# PTY tests run only where Python and a Unix pseudo-terminal are available.
if(UNIX)
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        add_test(NAME test_tui_terminal
            COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_tui_pty.py
                    $<TARGET_FILE:vip-coupon-system>)
        set_tests_properties(test_tui_terminal PROPERTIES TIMEOUT 30)
    endif()
endif()
