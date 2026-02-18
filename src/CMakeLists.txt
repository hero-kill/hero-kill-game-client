# SPDX-License-Identifier: GPL-3.0-or-later

file(GLOB SWIG_FILES "${PROJECT_SOURCE_DIR}/src/swig/*.i")
set(SWIG_SOURCE ${PROJECT_SOURCE_DIR}/src/swig/herokill.i)

add_custom_command(
  OUTPUT ${PROJECT_SOURCE_DIR}/src/swig/herokill-wrap.cxx
  DEPENDS ${SWIG_FILES}
  COMMENT "Generating herokill-wrap.cxx"
  COMMAND swig -c++ -lua -Wall -o
    ${PROJECT_SOURCE_DIR}/src/swig/herokill-wrap.cxx
    ${SWIG_SOURCE}
)

set(herokill_EXESRCS
  "main.cpp"
)

set(herokill_SRCS
  # "main.cpp"
  "herokill.cpp"

  "core/player.cpp"
  "core/util.cpp"
  "core/c-wrapper.cpp"
  "core/packman.cpp"

  "client/client.cpp"
  "client/clientplayer.cpp"
  "client/replayer.cpp"
  "client/update_client.cpp"

  "network/client_socket.cpp"
  "network/router.cpp"
  "network/data_service_proxy.cpp"

  "ui/qmlbackend.cpp"

  "swig/herokill-wrap.cxx"
)
set_source_files_properties(
  "swig/herokill-wrap.cxx" PROPERTIES GENERATED TRUE)

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
  list(APPEND herokill_EXESRCS ${app_icon_resource_windows})
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

  set_target_properties(HeroKill PROPERTIES
    QT_ANDROID_PACKAGE_SOURCE_DIR ${PROJECT_SOURCE_DIR}/android
    QT_ANDROID_EXTRA_LIBS "${LUA_LIB};${SQLITE3_LIB};${CRYPTO_LIB};${SSL_LIB};${GIT_LIB};${SSH_LIB}"
  )
  list(REMOVE_ITEM QT_LIB Qt6::QuickControls2)
elseif (APPLE)
  set(LUA_LIB ${LUA_LIBRARIES})
  set(SQLITE3_LIB sqlite3)
  set(CRYPTO_LIB OpenSSL::Crypto)
  set(READLINE_LIB readline)
  set(GIT_LIB ${LIBGIT2_LIBRARIES})
  target_link_directories(HeroKill PRIVATE ${LIBGIT2_LIBRARY_DIRS})
else ()
  set(LUA_LIB lua5.4)
  set(SQLITE3_LIB sqlite3)
  set(CRYPTO_LIB OpenSSL::Crypto)
  set(READLINE_LIB readline)
  set(GIT_LIB git2)
endif ()

add_library(libHeroKill STATIC ${herokill_SRCS})
target_precompile_headers(libHeroKill PRIVATE "pch.h")
target_link_libraries(libHeroKill PRIVATE
  ${LUA_LIB}
  ${SQLITE3_LIB}
  ${CRYPTO_LIB}
  ${SSL_LIB}
  ${READLINE_LIB}
  ${QT_LIB}
  ${SSH_LIB}
  ${GIT_LIB}
  ${IDBFS_LIB}
  ${JEMALLOC_LIBRARIES}
)

if (${CMAKE_BUILD_TYPE}0 STREQUAL "Debug0")
  target_compile_definitions(libHeroKill PRIVATE QT_QML_DEBUG)
endif ()

target_sources(HeroKill PRIVATE ${herokill_EXESRCS})
target_link_libraries(HeroKill PRIVATE
  libHeroKill
)

install(TARGETS HeroKill DESTINATION usr/bin)
install(DIRECTORY
  ${PROJECT_SOURCE_DIR}/audio
  ${PROJECT_SOURCE_DIR}/fonts
  ${PROJECT_SOURCE_DIR}/image
  ${PROJECT_SOURCE_DIR}/client
  DESTINATION usr/share/HeroKill
)
install(DIRECTORY
  ${PROJECT_SOURCE_DIR}/packages/herokill-core
  DESTINATION usr/share/HeroKill/packages
)
install(FILES
  ${PROJECT_SOURCE_DIR}/fk_ver
  DESTINATION usr/share/HeroKill
)
install(FILES
  ${PROJECT_SOURCE_DIR}/image/icon.png
  DESTINATION usr/share/icons
  RENAME HeroKill.png
)
install(FILES
  ${PROJECT_SOURCE_DIR}/herokill.desktop
  DESTINATION usr/share/applications
)


if(CPACK_GENERATOR STREQUAL "AppImage")
  install(CODE "
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E create_symlink
                usr/share/icons/HeroKill.png
                \${CMAKE_INSTALL_PREFIX}/HeroKill.png
    )
  ")

  qt_generate_deploy_qml_app_script(
    TARGET HeroKill
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
  )
  install(SCRIPT ${deploy_script})

  set(CPACK_PACKAGE_ICON HeroKill)
  include(CPack)
endif()
