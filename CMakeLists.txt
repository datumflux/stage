# CentOS의 경우 -- sudo yum install cmake3 설치 후
cmake_minimum_required(VERSION 3.1)
project(Stage.LUA)

set(CMAKE_INCLUDE_PATH
    src
    include
	lua/helper
	lua/lua-compat-5.3/c-api
    ${CMAKE_INCLUDE_PATH})

set(CMAKE_LIBRARY_PATH
    lua/5.3/lib
    lua/jit/lib
    ${CMAKE_LIBRARY_PATH})

set(LINK_LIBRARY_FILES
    log4cxx
    jsoncpp
    odbc
    z
    jemalloc
    pthread
    stdc++
    dl
    ${LINK_LIBRARY_FILES})

set(SOURCE_DIRS
    src
    libsrc
    lua/helper
    ${SOURCE_DIRS})

foreach (DIR ${SOURCE_DIRS})
    file(GLOB_RECURSE
        SRCS
        ${PROJECT_SOURCE_DIR}/${DIR}/*.c*)
    set(SOURCE_FILES ${SOURCE_FILES} ${SRCS})
endforeach ()

#
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c11 -fPIC -rdynamic -Wall")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -fPIC -rdynamic -Wall")

# set(LINK_LIBRARY_FILES debug asan ${LINK_LIBRARY_FILES})
# set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -ggdb -fsanitize=address")
set(CMAKE_CXX_FLAGS_DEBUG "-DDEBUG ${CMAKE_CXX_FLAGS_DEBUG} -ggdb")
set(CMAKE_CXX_FLAGS_RELEASE "-DRELEASE ${CMAKE_CXX_FLAGS_RELEASE}")

add_definitions(
        -D_GNU_SOURCE
        -D__STDC_LIMIT_MACROS
        -D_REENTRANT
        -DLUA_BUNDLE
)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin")

#
#

#
# stage.lua
if(IS_DIRECTORY "${PROJECT_SOURCE_DIR}/lua/5.3/include")
    set(CMAKE_INCLUDE_PATH lua/5.3/include ${CMAKE_INCLUDE_PATH})
    set(LINK_LIBRARY_FILES lua-5.3 ${LINK_LIBRARY_FILES})
elseif(IS_DIRECTORY "${PROJECT_SOURCE_DIR}/lua/jit/include") # stage.luajit
    set(CMAKE_INCLUDE_PATH lua/jit/include ${CMAKE_INCLUDE_PATH})
    set(LINK_LIBRARY_FILES luajit-5.1 ${LINK_LIBRARY_FILES})
endif()

include_directories(${CMAKE_INCLUDE_PATH})
link_directories(${CMAKE_LIBRARY_PATH})
set(STAGE_SOURCE_FILES ${SOURCE_FILES} ${BUNDLE_SOURCES} src/lua.cpp main.cpp)

add_executable(stage ${STAGE_SOURCE_FILES})
target_link_libraries(stage ${LINK_LIBRARY_FILES})
