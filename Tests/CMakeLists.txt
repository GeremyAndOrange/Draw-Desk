# 文件用途: 单元测试构建脚本, 使用 Qt Test 覆盖纯逻辑与配置序列化
find_package(Qt6 REQUIRED COMPONENTS Test)

qt_add_executable(DrawDeskTests
    DrawDeskTests.cpp
    ${CMAKE_SOURCE_DIR}/Src/Platform/ConfigStore.cpp
    ${CMAKE_SOURCE_DIR}/Src/Platform/Log.cpp
    ${CMAKE_SOURCE_DIR}/Src/Platform/WindowApi.cpp
    ${CMAKE_SOURCE_DIR}/Src/Platform/HotkeyService.cpp
    ${CMAKE_SOURCE_DIR}/Src/Services/DrawerService.cpp
)

target_include_directories(DrawDeskTests PRIVATE ${CMAKE_SOURCE_DIR}/Src)

target_compile_definitions(DrawDeskTests PRIVATE
    UNICODE
    _UNICODE
    NOMINMAX
)

target_link_libraries(DrawDeskTests PRIVATE
    Qt6::Core
    Qt6::Test
    user32
    dwmapi
)

if(MSVC)
    target_compile_options(DrawDeskTests PRIVATE /W4)
else()
    target_compile_options(DrawDeskTests PRIVATE -Wall -Wextra)
endif()

set_target_properties(DrawDeskTests PROPERTIES WIN32_EXECUTABLE FALSE)

drawdesk_add_moc(DrawDeskTests Tests/DrawDeskTests.h)
drawdesk_add_moc(DrawDeskTests Src/Services/DrawerService.h)

add_test(NAME DrawDeskTests COMMAND DrawDeskTests)