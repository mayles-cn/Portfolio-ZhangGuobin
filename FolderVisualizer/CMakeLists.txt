cmake_minimum_required(VERSION 3.15)
project(FolderVisualizer VERSION 1.0.0 LANGUAGES CXX)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 导出 compile_commands.json
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Windows 特定配置
if(WIN32)
    add_definitions(-D_WIN32_WINNT=0x0601)  # Windows 7 或更高版本
    add_definitions(-DUNICODE -D_UNICODE)
endif()

# 查找 Windows SDK 库（Windows 平台）
if(WIN32)
    # Windows 下直接使用系统库
    set(WINDOWS_LIBS gdiplus comctl32 shell32 ole32 uuid)
else()
    set(WINDOWS_LIBS "")
endif()

# 源文件列表
set(SOURCES
    FolderVisualizerGUI.cpp
    FileTree.cpp
    TreeNode.cpp
    IconManager.cpp
    FileTypeManager.cpp
)

# Windows 资源文件（图标、版本信息等）
if(WIN32)
    set(WINDOWS_RESOURCES resource.rc)
endif()

# 头文件列表
set(HEADERS
    FileTree.h
    TreeNode.h
    IconManager.h
    FileTypeManager.h
)

# 创建可执行文件（包含资源文件）
if(WIN32)
    add_executable(${PROJECT_NAME} WIN32 ${SOURCES} ${HEADERS} ${WINDOWS_RESOURCES})
else()
    add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})
endif()

# 包含目录
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# 链接库
if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${WINDOWS_LIBS})
endif()

# Windows 子系统设置
if(MSVC)
    # 启用 Unicode
    target_compile_options(${PROJECT_NAME} PRIVATE /UTF-8)
    
    # 关闭 NOMINMAX 避免 min/max 宏冲突
    target_compile_definitions(${PROJECT_NAME} PRIVATE NOMINMAX)
    
    # 运行时库设置
    set_property(TARGET ${PROJECT_NAME} PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()

# 安装规则
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
)

# 复制图标目录（如果存在）
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/icons")
    install(DIRECTORY icons DESTINATION share/${PROJECT_NAME})
endif()
