cmake_minimum_required(VERSION 3.16)

project(Oscilloscope VERSION 0.1.0 LANGUAGES CXX C)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_C_STANDARD 11)

set(APP_NAME run)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_RELEASE "-O2")

set(SOURCES_LIST
    app/main.cpp
)

set(HEADERS_LIST
    
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

add_executable(${APP_NAME} ${SOURCES_LIST})

