# 文件用途: DrawDesk 顶层构建脚本
# 依赖: Qt 6.11.2 与 MinGW 13.1.0 工具链, 路径见 CMakePresets.json

cmake_minimum_required(VERSION 3.24)

project(DrawDesk VERSION 0.1.1 LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Quick Qml Svg)

qt_standard_project_setup()

qt_policy(SET QTP0001 NEW)

# 当前开发环境限制子进程启动, CMake 的 AutoMoc 无法运行
# 统一关闭自动 moc, 含 Q_OBJECT 的头文件在文件末尾用 drawdesk_add_moc 手动注册
set(CMAKE_AUTOMOC OFF)
set(CMAKE_AUTOUIC OFF)

set_source_files_properties(Src/Ui/Main.qml PROPERTIES
    QT_RESOURCE_ALIAS Main.qml
)

qt_add_executable(DrawDesk
    Src/Main.cpp
    Src/Platform/Log.cpp
    Src/Platform/ConfigStore.cpp
    Src/Platform/WindowApi.cpp
    Src/Platform/SingleInstance.cpp
    Src/Platform/HotkeyService.cpp
    Src/Platform/TrayIcon.cpp
    Src/Services/DrawerService.cpp
    Src/Services/SelfTest.cpp
    Src/Ui/AppIcon.cpp
    Src/Resources/DrawDesk.rc
)

qt_add_qml_module(DrawDesk
    URI DrawDesk
    VERSION 1.0
    NO_GENERATE_QMLTYPES
    QML_FILES
        Src/Ui/Main.qml
    RESOURCES
        Src/Ui/AppIcon.svg
)

target_include_directories(DrawDesk PRIVATE Src)

target_compile_definitions(DrawDesk PRIVATE
    UNICODE
    _UNICODE
    NOMINMAX
)

# 打开常规警告, 保持代码整洁
if(MSVC)
    target_compile_options(DrawDesk PRIVATE /W4)
else()
    target_compile_options(DrawDesk PRIVATE -Wall -Wextra)
endif()

target_link_libraries(DrawDesk PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Quick
    Qt6::Qml
    Qt6::Svg
    user32
    dwmapi
)

set_target_properties(DrawDesk PROPERTIES
    WIN32_EXECUTABLE TRUE
)

# 本机环境 CMake 的 AutoMoc 受沙箱限制不可用, 使用手动 moc
get_filename_component(QT_PREFIX "${Qt6_DIR}/../../.." ABSOLUTE)

# 对含 Q_OBJECT 的头文件运行 moc, 生成的源加入目标
function(drawdesk_add_moc target)
    foreach(header IN LISTS ARGN)
        get_filename_component(header_name ${header} NAME_WE)
        set(moc_file ${CMAKE_CURRENT_BINARY_DIR}/moc_${header_name}.cpp)
        add_custom_command(
            OUTPUT ${moc_file}
            COMMAND $<TARGET_FILE:Qt6::moc>
                -I${CMAKE_SOURCE_DIR}/Src
                -I${QT_PREFIX}/include
                -I${QT_PREFIX}/include/QtCore
                ${CMAKE_SOURCE_DIR}/${header}
                -o ${moc_file}
            DEPENDS ${CMAKE_SOURCE_DIR}/${header}
            COMMENT "Running moc for ${header}"
            VERBATIM
        )
        target_sources(${target} PRIVATE ${moc_file})
    endforeach()
endfunction()

drawdesk_add_moc(DrawDesk
    Src/Services/DrawerService.h
)

# 单元测试目标, 使用 Qt Test
enable_testing()
add_subdirectory(Tests)