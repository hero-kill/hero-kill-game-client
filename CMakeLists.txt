# SPDX-License-Identifier: GPL-3.0-or-later
# ------------------------------------------------------------
#  此为新月杀的项目组织文件，采用CMake+QT
# 2022-01-24 新建文件夹 2023-02-21 发布v0.0.1版本
# ------------------------------------------------------------

cmake_minimum_required(VERSION 3.22)

project(FreeKill VERSION 0.5.17)
add_definitions(-DFK_VERSION=\"${CMAKE_PROJECT_VERSION}\")

find_package(Qt6 REQUIRED COMPONENTS
  Network
  Gui
  Qml
  Quick
  QuickEffects
  Widgets
  Multimedia
  QuickControls2
  Concurrent
  LinguistTools
)

if (DEFINED RPC_DEBUG)
  add_definitions(-DRPC_DEBUG)
endif()

set(CMAKE_COLOR_DIAGNOSTICS ON)

find_package(OpenSSL)
find_package(Lua)
find_package(SQLite3)

# 查找 libgit2
if (APPLE)
  find_package(PkgConfig)
  if (PkgConfig_FOUND)
    pkg_check_modules(LIBGIT2 libgit2)
    if (LIBGIT2_FOUND)
      message(STATUS "Found libgit2: ${LIBGIT2_LIBRARIES}")
    endif ()
  endif ()
elseif (WIN32)
  set(GIT_LIB ${PROJECT_SOURCE_DIR}/lib/win/libgit2.dll)
elseif (ANDROID)
  if (DEFINED ENV{FK_ANDROID_SSL_DIR})
    set(GIT_LIB $ENV{FK_ANDROID_SSL_DIR}/libgit2.so)
  else ()
    set(GIT_LIB ${PROJECT_SOURCE_DIR}/lib/android/libgit2.so)
  endif ()
else ()
  set(GIT_LIB git2)
endif ()

set(CMAKE_AUTOMOC ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(REQUIRED_QT_VERSION "6.8")

include_directories(include)
include_directories(src)
# 谁加的，把我Github Action搞坏了，如果是MacOS的话麻烦加if吧我现在忙着调CI没空管
# include_directories(${LUA_INCLUDE_DIR})
# include_directories(${LIBGIT2_INCLUDE_DIRS})
include_directories(include/lua)
include_directories(include/libgit2)

if (${CMAKE_BUILD_TYPE}0 STREQUAL "Debug0")
  # add_compile_options(-fsanitize=address -fsanitize=undefined)
  # add_link_options(-fsanitize=address -fsanitize=undefined)
endif ()

if (DEFINED FK_USE_JEMALLOC)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules (JEMALLOC jemalloc)

  pkg_search_module(JEMALLOC REQUIRED jemalloc)
  include_directories(${JEMALLOC_INCLUDE_DIRS})
endif ()

qt_add_executable(FreeKill)
link_directories(${LIBGIT2_LIBRARY_DIRS})

qt_add_translations(FreeKill
  TS_FILES lang/zh_CN.ts
  QM_FILES_OUTPUT_VARIABLE zh_CN.qm

  TS_FILES lang/en_US.ts
  QM_FILES_OUTPUT_VARIABLE en_US.qm

  TS_FILES lang/vi_VN.ts
  QM_FILES_OUTPUT_VARIABLE vi_VN.qm
)

add_custom_command(
  TARGET FreeKill
  POST_BUILD
  COMMENT "Generating version file fk_ver"
  COMMAND echo ${CMAKE_PROJECT_VERSION} > ${PROJECT_SOURCE_DIR}/fk_ver
  COMMAND ${PROJECT_SOURCE_DIR}/genfkver.sh
)

add_subdirectory(src)

if (DEFINED FK_TESTS)
enable_testing()
add_subdirectory(test)
endif ()
