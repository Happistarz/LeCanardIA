cmake_minimum_required(VERSION 3.10)

project(MyBot)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O2 -Wall -Wno-unused-function -pedantic")

include_directories(${CMAKE_SOURCE_DIR})

file(GLOB_RECURSE HLT_SOURCE ${CMAKE_SOURCE_DIR}/hlt/*.[ch]*)
file(GLOB_RECURSE BOT_SOURCE ${CMAKE_SOURCE_DIR}/HaliteAI/Bot/*.[ch]*)

set(SOURCE_FILES ${HLT_SOURCE} ${BOT_SOURCE} MyBot.cpp)

add_executable(MyBot ${SOURCE_FILES})

if(MINGW)
    target_link_libraries(MyBot -static)
endif()
