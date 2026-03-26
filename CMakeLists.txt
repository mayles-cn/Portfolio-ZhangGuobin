cmake_minimum_required(VERSION 3.21)
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

set(FOLDER_VISUALIZER_TARGET ${PROJECT_NAME})

if(WIN32)
    add_executable(${FOLDER_VISUALIZER_TARGET} WIN32)
else()
    add_executable(${FOLDER_VISUALIZER_TARGET})
endif()

add_subdirectory(src)

if(WIN32)
    add_subdirectory(resources/windows)
endif()

target_include_directories(${FOLDER_VISUALIZER_TARGET} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/resources/windows"
)

target_compile_definitions(${FOLDER_VISUALIZER_TARGET} PRIVATE
    UNICODE
    _UNICODE
    _WIN32_WINNT=0x0601
    NOMINMAX
)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(${FOLDER_VISUALIZER_TARGET} PRIVATE -Wall -Wextra)
endif()

if(WIN32)
    target_link_libraries(${FOLDER_VISUALIZER_TARGET} PRIVATE gdiplus comctl32 shell32 ole32 uuid)
endif()

set(RELEASE_DIR "${CMAKE_SOURCE_DIR}/release")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${RELEASE_DIR}")

foreach(cfg IN ITEMS Debug Release RelWithDebInfo MinSizeRel)
    string(TOUPPER "${cfg}" cfg_upper)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${cfg_upper} "${RELEASE_DIR}")
endforeach()

set(ASSETS_DIR "${CMAKE_SOURCE_DIR}/assets")

function(copy_runtime_file source_file destination_name)
    if(EXISTS "${source_file}")
        add_custom_command(TARGET ${FOLDER_VISUALIZER_TARGET} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${source_file}"
                "${RELEASE_DIR}/${destination_name}"
            COMMENT "Copy ${destination_name} to release/"
        )
    endif()
endfunction()

copy_runtime_file("${ASSETS_DIR}/settings.json" "settings.json")
copy_runtime_file("${ASSETS_DIR}/tree.ico" "tree.ico")

if(EXISTS "${ASSETS_DIR}/icons")
    add_custom_command(TARGET ${FOLDER_VISUALIZER_TARGET} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${ASSETS_DIR}/icons"
            "${RELEASE_DIR}/icons"
        COMMENT "Copy icons/ to release/"
    )
endif()
