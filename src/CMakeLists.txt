# SPDX-License-Identifier: GPL-3.0-or-later

file(GLOB SWIG_FILES "${PROJECT_SOURCE_DIR}/src/swig/*.i")
set(SWIG_SOURCE ${PROJECT_SOURCE_DIR}/src/swig/freekill.i)

add_custom_command(
  OUTPUT ${PROJECT_SOURCE_DIR}/src/swig/freekill-wrap.cxx
  DEPENDS ${SWIG_FILES}
  COMMENT "Generating freekill-wrap.cxx"
  COMMAND swig -c++ -lua -Wall -o
    ${PROJECT_SOURCE_DIR}/src/swig/freekill-wrap.cxx
    ${SWIG_SOURCE}
)

set(freekill_EXESRCS
  "main.cpp"
)

set(freekill_SRCS
  # "main.cpp"
  "freekill.cpp"

  "core/player.cpp"
  "core/util.cpp"
  "core/c-wrapper.cpp"

  "client/client.cpp"
  "client/clientplayer.cpp"
  "client/replayer.cpp"

  "network/client_socket.cpp"
  "network/router.cpp"
  "network/data_service_proxy.cpp"
  "network/admin_service_proxy.cpp"

  "ui/qmlbackend.cpp"

  "swig/freekill-wrap.cxx"
)
set_source_files_properties(
  "swig/freekill-wrap.cxx" PROPERTIES GENERATED TRUE)

set(QT_LIB
  Qt6::Network
  Qt6::Qml
  Qt6::Quick
  Qt6::QuickEffects
  Qt6::Gui
  Qt6::Widgets
  Qt6::Multimedia
  Qt6::QuickControls2
  Qt6::Concurrent
)

if (WIN32)
  set(LUA_LIB ${PROJECT_SOURCE_DIR}/lib/win/lua54.dll)
  set(SQLITE3_LIB ${PROJECT_SOURCE_DIR}/lib/win/sqlite3.dll)
  set(CRYPTO_LIB OpenSSL::Crypto)
  set(app_icon_resource_windows "${PROJECT_SOURCE_DIR}/image/icon.rc")
  list(APPEND freekill_EXESRCS ${app_icon_resource_windows})
elseif (ANDROID)
  if (DEFINED ENV{FK_ANDROID_SSL_DIR})
    set(SSL_DIR $ENV{FK_ANDROID_SSL_DIR})
    message(STATUS "Using SSL libraries from environment variable: ${SSL_DIR}")
    set(CRYPTO_LIB ${SSL_DIR}/libcrypto_3.so)
    set(SSL_LIB ${SSL_DIR}/libssl_3.so)
  else ()
    set(CRYPTO_LIB ${PROJECT_SOURCE_DIR}/lib/android/libcrypto.so)
    set(SSL_LIB ${PROJECT_SOURCE_DIR}/lib/android/libssl.so)
    set(SSH_LIB ${PROJECT_SOURCE_DIR}/lib/android/libssh2.so)
  endif ()
  set(LUA_LIB ${PROJECT_SOURCE_DIR}/lib/android/liblua54.so)
  set(SQLITE3_LIB ${PROJECT_SOURCE_DIR}/lib/android/libsqlite3.so)

  set_target_properties(FreeKill PROPERTIES
    QT_ANDROID_PACKAGE_SOURCE_DIR ${PROJECT_SOURCE_DIR}/android
    QT_ANDROID_EXTRA_LIBS "${LUA_LIB};${SQLITE3_LIB};${CRYPTO_LIB};${SSL_LIB};${GIT_LIB};${SSH_LIB}"
  )
  list(REMOVE_ITEM QT_LIB Qt6::QuickControls2)
elseif (APPLE)
  set(LUA_LIB ${LUA_LIBRARIES})
  set(SQLITE3_LIB sqlite3)
  set(CRYPTO_LIB OpenSSL::Crypto)
  set(READLINE_LIB readline)
else ()
  set(LUA_LIB lua5.4)
  set(SQLITE3_LIB sqlite3)
  set(CRYPTO_LIB OpenSSL::Crypto)
  set(READLINE_LIB readline)
endif ()

add_library(libFreeKill STATIC ${freekill_SRCS})
target_precompile_headers(libFreeKill PRIVATE "pch.h")
target_link_libraries(libFreeKill PRIVATE
  ${LUA_LIB}
  ${SQLITE3_LIB}
  ${CRYPTO_LIB}
  ${SSL_LIB}
  ${READLINE_LIB}
  ${QT_LIB}
  ${SSH_LIB}
  ${IDBFS_LIB}
  ${JEMALLOC_LIBRARIES}
)

if (${CMAKE_BUILD_TYPE}0 STREQUAL "Debug0")
  target_compile_definitions(libFreeKill PRIVATE QT_QML_DEBUG)
endif ()

target_sources(FreeKill PRIVATE ${freekill_EXESRCS})
target_link_libraries(FreeKill PRIVATE
  libFreeKill
)

install(TARGETS FreeKill DESTINATION usr/bin)
install(DIRECTORY
  ${PROJECT_SOURCE_DIR}/audio
  ${PROJECT_SOURCE_DIR}/fonts
  ${PROJECT_SOURCE_DIR}/image
  ${PROJECT_SOURCE_DIR}/client
  DESTINATION usr/share/FreeKill
)
install(DIRECTORY
  ${PROJECT_SOURCE_DIR}/packages/freekill-core
  DESTINATION usr/share/FreeKill/packages
)
install(FILES
  ${PROJECT_SOURCE_DIR}/fk_ver
  DESTINATION usr/share/FreeKill
)
install(FILES
  ${PROJECT_SOURCE_DIR}/image/icon.png
  DESTINATION usr/share/icons
  RENAME FreeKill.png
)
install(FILES
  ${PROJECT_SOURCE_DIR}/freekill.desktop
  DESTINATION usr/share/applications
)

install(FILES
  ${CMAKE_BINARY_DIR}/zh_CN.qm
  ${CMAKE_BINARY_DIR}/en_US.qm
  ${CMAKE_BINARY_DIR}/vi_VN.qm
  DESTINATION usr/share/FreeKill
)

if(CPACK_GENERATOR STREQUAL "AppImage")
  install(CODE "
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E create_symlink
                usr/share/icons/FreeKill.png
                \${CMAKE_INSTALL_PREFIX}/FreeKill.png
    )
  ")

  qt_generate_deploy_qml_app_script(
    TARGET FreeKill
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
  )
  install(SCRIPT ${deploy_script})

  set(CPACK_PACKAGE_ICON FreeKill)
  include(CPack)
endif()
