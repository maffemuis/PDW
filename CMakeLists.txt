# CMakeLists.txt
#
# Base build script for building with cmake (http://www.cmake.org)

cmake_minimum_required( VERSION 3.15 )
project( PDW )
include( CTest )

if( MSVC )
  set( CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" )
endif()

if( CMAKE_SIZEOF_VOID_P EQUAL 8 )
  message( FATAL_ERROR "PDW must be built as 32-bit (Win32). Use -A Win32 with CMake." )
endif()

set( PDW_VERSION_MAJOR 3 )
set( PDW_VERSION_MINOR 1 )
set( PDW_VERSION_MICRO 0 )

set( PDW_GFXS )
set( PDW_INCLUDES )
set( PWD_SRCS )

add_subdirectory( core )
add_subdirectory( GFX )
add_subdirectory( Headers )
add_subdirectory( utils )
include_directories( "${CMAKE_CURRENT_SOURCE_DIR}/openssl-3.5.5/include" )
link_directories( "${CMAKE_CURRENT_SOURCE_DIR}/openssl-3.5.5/lib" )

if( MSVC )
  add_definitions( "-D_CRT_SECURE_NO_WARNINGS" )
endif( MSVC )

set( PDW_INCLUDES
        ${PDW_INCLUDES}
        resrc1.h
)

set( PDW_SRCS
        ${PDW_SRCS}
        Flex.cpp
        Gfx.cpp
        gfx_startup_guard.cpp
        Initapp.cpp
        Misc.cpp
        Rsrc.rc
        Rsrc.nl.rc
        Rsrc.nl.scrollback.rc
        Rsrc.nl.interface.rc
        Rsrc.nl.options.rc
        Rsrc.nl.filters.rc
        Rsrc.nl.remaining.rc
        Rsrc.nl.finish.rc
        PDW.cpp
        startup_defaults.cpp
        Pocsag.cpp
        acars.cpp
        decode.cpp
        ermes.cpp
        helper_funcs.cpp
        language.cpp
        menu.cpp
        mobitex.cpp
        toolbar.cpp
        printer.cpp
        sigind.cpp
        sound_in.cpp
)

set_source_files_properties( Misc.cpp PROPERTIES
        COMPILE_DEFINITIONS PDW_SHOWMESSAGE_IMPLEMENTATION
)
set_source_files_properties( Gfx.cpp PROPERTIES
        COMPILE_DEFINITIONS "DrawTitleBarGfx=LegacyDrawTitleBarGfx;SetMessageItemPositionsWidth=LegacySetMessageItemPositionsWidth"
)
set_source_files_properties( sound_in.cpp PROPERTIES
        COMPILE_DEFINITIONS "Start_Capturing=LegacyStart_Capturing"
)
set_source_files_properties( Initapp.cpp PROPERTIES
        COMPILE_DEFINITIONS "InitApplication=LegacyInitApplication"
)
set_source_files_properties( PDW.cpp PROPERTIES
        COMPILE_DEFINITIONS "GetPrivateProfileSettings=LegacyGetPrivateProfileSettings"
)

add_executable( PDW WIN32
        ${PDW_INCLUDES}
        ${PDW_SRCS}
)

target_link_libraries( PDW
        Comctl32.lib
        Dwmapi.lib
        UxTheme.lib
        Winmm.lib
        Winhttp.lib
        Advapi32.lib
        Ws2_32.lib
        libssl_static.lib
        libcrypto_static.lib
        legacy_stdio_definitions.lib
        Crypt32.lib
        User32.lib
)

if( BUILD_TESTING )
  add_executable( pdw_ui_theme_test
      tests/utils/ui_theme_test.cpp
      utils/ui_theme.cpp
  )
  target_include_directories( pdw_ui_theme_test PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/utils
  )
  add_test(
      NAME pdw_ui_theme
      COMMAND pdw_ui_theme_test
  )

  add_executable( pdw_integration_boundary_test
      tests/core/integration_boundary_test.cpp
      core/decoded_message.cpp
      core/integration_event.cpp
      core/integration_worker.cpp
  )
  target_include_directories( pdw_integration_boundary_test PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/core
  )
  add_test(
      NAME pdw_integration_boundary
      COMMAND pdw_integration_boundary_test
  )

  add_executable( pdw_webhook_transport_policy_test
      tests/core/webhook_transport_policy_test.cpp
      core/integration_worker.cpp
      utils/webhook_transport_winhttp.cpp
  )
  target_include_directories( pdw_webhook_transport_policy_test PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/core
      ${CMAKE_CURRENT_SOURCE_DIR}/utils
  )
  target_link_libraries( pdw_webhook_transport_policy_test
      Winhttp.lib
      Advapi32.lib
  )
  add_test(
      NAME pdw_webhook_transport_policy
      COMMAND pdw_webhook_transport_policy_test
  )

  add_executable( pdw_webhook_runtime_test
      tests/core/webhook_runtime_test.cpp
      core/integration_worker.cpp
      utils/webhook_runtime.cpp
      utils/webhook_transport_winhttp.cpp
  )
  target_include_directories( pdw_webhook_runtime_test PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/core
      ${CMAKE_CURRENT_SOURCE_DIR}/utils
  )
  target_link_libraries( pdw_webhook_runtime_test
      Winhttp.lib
      Advapi32.lib
  )
  add_test(
      NAME pdw_webhook_runtime
      COMMAND pdw_webhook_runtime_test
  )

  add_subdirectory( tests )

  if( MSVC )
    add_test(
      NAME pdw_portable_no_dynamic_msvc_runtime
      COMMAND powershell -NoProfile -ExecutionPolicy Bypass
              -File "${CMAKE_CURRENT_SOURCE_DIR}/tests/portable/check_msvc_runtime.ps1"
              -Executable "$<TARGET_FILE:PDW>"
    )
  endif()
endif()
