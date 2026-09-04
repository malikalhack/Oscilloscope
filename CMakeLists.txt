cmake_minimum_required(VERSION 3.16)

project(Oscilloscope VERSION 0.2.6 LANGUAGES CXX C)
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

function(enable_project_warnings target_name)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target_name} PRIVATE
            -Wall -Wextra -Wpedantic
        )
    endif()
endfunction()

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_RELEASE "-O2")

set(SOURCES_LIST
    app/main.cpp
    usb/src/usb_device.cpp
    usb/src/firmware_loader.cpp
    capture/src/acquisition_loop.cpp
    capture/src/raw_packet_queue.cpp
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
    capture/inc/acquisition_loop.h
    capture/inc/raw_packet_queue.h
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
    ${CMAKE_CURRENT_SOURCE_DIR}/capture/inc
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)

target_link_libraries(${APP_NAME} PRIVATE
    SDL2::SDL2
    OpenGL::GL
    PkgConfig::LIBUSB
    Threads::Threads
)

enable_project_warnings(${APP_NAME})

include(CTest)

if(BUILD_TESTING)
    add_executable(raw_packet_queue_tests
        tests/raw_packet_queue_test.cpp
        capture/src/raw_packet_queue.cpp
    )
    target_include_directories(raw_packet_queue_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/capture/inc
    )
    target_link_libraries(raw_packet_queue_tests PRIVATE Threads::Threads)
    enable_project_warnings(raw_packet_queue_tests)
    add_test(NAME raw_packet_queue COMMAND raw_packet_queue_tests)
endif()

