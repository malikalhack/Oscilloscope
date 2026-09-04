cmake_minimum_required(VERSION 3.16)

project(Oscilloscope VERSION 0.2.4 LANGUAGES CXX C)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.90.9
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(imgui)

find_package(OpenGL REQUIRED)
find_package(SDL2 REQUIRED)
find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)
pkg_check_modules(LIBUSB REQUIRED IMPORTED_TARGET libusb-1.0)

set(APP_NAME run)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_RELEASE "-O2")

set(SOURCES_LIST
    app/main.cpp
    usb/src/usb_device.cpp
    usb/src/firmware_loader.cpp
    capture/acquisition_loop.cpp
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl2.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)

set(HEADERS_LIST
    usb/inc/usb_device.h
    usb/inc/firmware_loader.h
    capture/acquisition_loop.h
)

if(CMAKE_BUILD_TYPE MATCHES "Debug")
    message(STATUS ">>> Debug build")
    add_compile_definitions(_DEBUG)
endif()

if(CMAKE_SYSTEM_NAME MATCHES "Windows")
    message(STATUS ">>> Windows")
    add_compile_definitions(_WINDOWS)
elseif(CMAKE_SYSTEM_NAME MATCHES "Linux")
    message(STATUS ">>> Linux")
endif()

add_executable(${APP_NAME} ${SOURCES_LIST} ${HEADERS_LIST})

add_custom_command(
    TARGET ${APP_NAME}
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/oscilloscope.bmp
        $<TARGET_FILE_DIR:${APP_NAME}>/oscilloscope.bmp
    VERBATIM
)

target_include_directories(${APP_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/usb/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/capture
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)

target_link_libraries(${APP_NAME} PRIVATE
    SDL2::SDL2
    OpenGL::GL
    PkgConfig::LIBUSB
    Threads::Threads
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${APP_NAME} PRIVATE -Wall -Wextra -Wpedantic)
endif()

