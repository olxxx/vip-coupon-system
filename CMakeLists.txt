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

# 1. nlohmann/json 依赖 (v3.12.0)
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.12.0
)

# 2. ftxui 终端 UI 库依赖 (v7.0.3)
FetchContent_Declare(
    ftxui
    GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI.git
    GIT_TAG        v7.0.3
)

# 下载并注入依赖
FetchContent_MakeAvailable(json ftxui)

# ---------------------------------------------------------------------------
#  Sources & Target Definition (显式列出所有源文件)
# ---------------------------------------------------------------------------
# 核心业务逻辑与 TUI 终端层源文件清单
set(CORE_SRC_FILES
    # 1. 用户权益与 RBAC 权限管理模块
    src/user/AdminAccount.cpp
    src/user/CustomerAccount.cpp
    src/user/TransactionRecord.cpp
    src/user/UserManager.cpp

    # 2. 商品货架与购物车管理模块
    src/shop/Cart.cpp
    src/shop/Product.cpp
    src/shop/ProductManager.cpp

    # 3. 多态优惠券与平行让利规划引擎模块
    src/coupon/ConcreteCoupons.cpp
    src/coupon/CouponManager.cpp
    src/coupon/CouponPlanner.cpp

    # 4. 数据持久化与冷启动仓储模块
    src/persist/JsonFactories.cpp
    src/persist/StorageManager.cpp

    # 5. FTXUI 终端状态机、主题与基础控件
    src/ui/AppContext.cpp
    src/ui/Page.cpp
    src/ui/PageManager.cpp
    src/ui/Theme.cpp

    # 6. FTXUI 7 大业务全屏页面
    src/ui/pages/AdminPage.cpp
    src/ui/pages/CartPage.cpp
    src/ui/pages/CheckoutPage.cpp
    src/ui/pages/CouponSquarePage.cpp
    src/ui/pages/LoginPage.cpp
    src/ui/pages/MallPage.cpp
    src/ui/pages/UserCenterPage.cpp
)

# 主可执行文件目标
add_executable(vip-coupon-system
    src/main.cpp
    ${CORE_SRC_FILES}
)

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

# ---------------------------------------------------------------------------
#  Automated Testing System (CTest)
# ---------------------------------------------------------------------------
enable_testing()

set(TEST_SOURCES
    tests/test_cart.cpp
    tests/test_concrete_coupons.cpp
    tests/test_coupon_manager.cpp
    tests/test_coupon_planner.cpp
    tests/test_product.cpp
    tests/test_product_manager.cpp
    tests/test_storage_manager.cpp
    tests/test_ui_integration.cpp
    tests/test_user_module.cpp
)

foreach(TEST_SRC ${TEST_SOURCES})
    get_filename_component(TEST_NAME ${TEST_SRC} NAME_WE)

    add_executable(${TEST_NAME} ${TEST_SRC} ${CORE_SRC_FILES})
    target_compile_features(${TEST_NAME} PRIVATE cxx_std_17)
    target_include_directories(${TEST_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")

    if(MSVC)
        target_compile_options(${TEST_NAME} PRIVATE /W4 /utf-8 /permissive- /U_NDEBUG /UNDEBUG)
    else()
        target_compile_options(${TEST_NAME} PRIVATE -Wall -Wextra -UNDEBUG)
    endif()
    target_compile_definitions(${TEST_NAME} PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)

    target_link_libraries(${TEST_NAME} PRIVATE
        nlohmann_json::nlohmann_json
        ftxui::screen
        ftxui::dom
        ftxui::component
    )

    # 测试产出二进制隔离存放在 build/bin/tests 目录
    set_target_properties(${TEST_NAME} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/tests"
        RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin/tests"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin/tests"
    )

    # 注册至 CTest 自动化测试套件
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
endforeach()
