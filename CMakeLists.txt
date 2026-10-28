cmake_minimum_required(VERSION 3.20)

project(vip-coupon-system
    VERSION 1.0.0
    DESCRIPTION "User Rights Management & Coupon Planning System"
    LANGUAGES CXX
)

# ---------------------------------------------------------------------------
#  Global Output & C++ Standards
# ---------------------------------------------------------------------------
# 强制将所有生成的目标二进制集中输出到根目录 bin/
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}/bin")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/bin")

# ---------------------------------------------------------------------------
#  Modern Third-party Dependencies (FetchContent)
# ---------------------------------------------------------------------------
include(FetchContent)

# 1. nlohmann/json 依赖 (最新版本 v3.12.0)
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.12.0
)

# 2. ftxui 终端 UI 库依赖 (最新版本 v7.0.3)
FetchContent_Declare(
    ftxui
    GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI.git
    GIT_TAG        v7.0.3
)

# 下载并注入依赖
FetchContent_MakeAvailable(json ftxui)

# ---------------------------------------------------------------------------
#  Sources & Target Definition
# ---------------------------------------------------------------------------
# 自动递归检索 src 下所有 .cpp 文件，且当目录增删文件时自动触发重新配置
file(GLOB_RECURSE SRC_FILES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

add_executable(vip-coupon-system ${SRC_FILES})

# ---------------------------------------------------------------------------
#  Target Properties & Compilation Features
# ---------------------------------------------------------------------------
target_compile_features(vip-coupon-system PRIVATE cxx_std_17)
set_target_properties(vip-coupon-system PROPERTIES CXX_EXTENSIONS OFF)

# 头文件搜索路径 (包含 include/)
target_include_directories(vip-coupon-system PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")

# 跨编译器编译选项 (MSVC 中文 UTF-8 防乱码与严格模式)
if(MSVC)
    target_compile_options(vip-coupon-system PRIVATE
        /W4
        /utf-8
        /permissive-
    )
else()
    target_compile_options(vip-coupon-system PRIVATE
        -Wall
        -Wextra
    )
endif()

# Windows 防御性宏定义 (防止 windows.h 命名污染破坏 std::min/std::max)
target_compile_definitions(vip-coupon-system PRIVATE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
)

# 链接第三方命名空间目标
target_link_libraries(vip-coupon-system PRIVATE
    nlohmann_json::nlohmann_json
    ftxui::screen
    ftxui::dom
    ftxui::component
)

# ---------------------------------------------------------------------------
#  Custom Targets (Convenient Launchers)
# ---------------------------------------------------------------------------
add_custom_target(run
    COMMAND $<TARGET_FILE:vip-coupon-system>
    DEPENDS vip-coupon-system
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Launching application..."
)
