cmake_minimum_required(VERSION 3.15)
project(FolderVisualizer VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(MSVC)
    message(FATAL_ERROR "This project is configured for MinGW. Please use: cmake -S . -B build_mingw -G \"MinGW Makefiles\"")
endif()

if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(APP_SOURCES
    FolderVisualizerGUI.cpp
    FileTree.cpp
    TreeNode.cpp
    IconManager.cpp
    FileTypeManager.cpp
)

set(APP_HEADERS
    FileTree.h
    TreeNode.h
    IconManager.h
    FileTypeManager.h
)

if(WIN32)
    add_executable(${PROJECT_NAME} WIN32 ${APP_SOURCES} ${APP_HEADERS} resource.rc)
else()
    add_executable(${PROJECT_NAME} ${APP_SOURCES} ${APP_HEADERS})
endif()

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_compile_definitions(${PROJECT_NAME} PRIVATE
    UNICODE
    _UNICODE
    _WIN32_WINNT=0x0601
    NOMINMAX
)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra)
endif()

if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE gdiplus comctl32 shell32 ole32 uuid)
endif()

set(RELEASE_DIR "${CMAKE_SOURCE_DIR}/release")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${RELEASE_DIR}")

foreach(cfg IN ITEMS Debug Release RelWithDebInfo MinSizeRel)
    string(TOUPPER "${cfg}" cfg_upper)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${cfg_upper} "${RELEASE_DIR}")
endforeach()

set(ASSETS_DIR "${CMAKE_SOURCE_DIR}/assets")
if(EXISTS "${ASSETS_DIR}/settings.json")
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${ASSETS_DIR}/settings.json"
            "${RELEASE_DIR}/settings.json"
        COMMENT "Copy settings.json to release/"
    )
endif()

if(EXISTS "${ASSETS_DIR}/tree.ico")
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${ASSETS_DIR}/tree.ico"
            "${RELEASE_DIR}/tree.ico"
        COMMENT "Copy tree.ico to release/"
    )
endif()

if(EXISTS "${ASSETS_DIR}/icons")
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${ASSETS_DIR}/icons"
            "${RELEASE_DIR}/icons"
        COMMENT "Copy icons/ to release/"
    )
endif()
