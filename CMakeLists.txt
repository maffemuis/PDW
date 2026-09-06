# CMakeLists.txt
#
# Base build script for building with cmake (http://www.cmake.org)

cmake_minimum_required( VERSION 3.15 )
project( PDW )
include( CTest )

# Keep release artifacts portable by linking the Microsoft C/C++ runtime
# statically. This avoids requiring MSVCP140.dll / VCRUNTIME140.dll to be
# preinstalled on the target Windows system.
if( MSVC )
  set( CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" )
endif()

# Enforce Win32 (x86)
if( CMAKE_SIZEOF_VOID_P EQUAL 8 )
  message( FATAL_ERROR "PDW must be built as 32-bit (Win32). Use -A Win32 with CMake." )
endif()

# Project version number
set( PDW_VERSION_MAJOR 3 )
set( PDW_VERSION_MINOR 1 )
set( PDW_VERSION_MICRO 0 )

# Set base variables
set( PDW_GFXS )
set( PDW_INCLUDES )
set( PWD_SRCS )

# Subdirectories to go to
add_subdirectory( core )
add_subdirectory( GFX )
add_subdirectory( Headers )
add_subdirectory( utils )
include_directories( "${CMAKE_CURRENT_SOURCE_DIR}/openssl-3.5.5/include" )
link_directories( "${CMAKE_CURRENT_SOURCE_DIR}/openssl-3.5.5/lib" )

# Microsoft Visual Studio Specific definitions
if( MSVC )
  add_definitions( "-D_CRT_SECURE_NO_WARNINGS" )
endif( MSVC )

# Include Files
set( PDW_INCLUDES
        ${PDW_INCLUDES}
        resrc1.h
)

# Source Files
set( PDW_SRCS
        ${PDW_SRCS}
        Flex.cpp
        Gfx.cpp
        gfx_startup_guard.cpp
        Initapp.cpp
        Misc.cpp
        Rsrc.rc
        Rsrc.nl.rc
        PDW.cpp
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

# Keep the legacy ShowMessage implementation untouched inside Misc.cpp.
# Other translation units include misc.h and are routed through the
# opt-in preservation observation wrapper.
set_source_files_properties( Misc.cpp PROPERTIES
        COMPILE_DEFINITIONS PDW_SHOWMESSAGE_IMPLEMENTATION
)

# Keep the legacy GDI title-bar implementation intact under internal names.
# A small startup guard prevents Windows 11 re-entrant paints from entering
# the layout code before cxChar/cyChar have been initialized.
set_source_files_properties( Gfx.cpp PROPERTIES
        COMPILE_DEFINITIONS "DrawTitleBarGfx=LegacyDrawTitleBarGfx;SetMessageItemPositionsWidth=LegacySetMessageItemPositionsWidth"
)

# Keep the legacy WinMM Start_Capturing implementation intact and expose it
# under an internal name. The preservation adapter provides Start_Capturing()
# and delegates here unless replay mode was explicitly requested.
set_source_files_properties( sound_in.cpp PROPERTIES
        COMPILE_DEFINITIONS "Start_Capturing=LegacyStart_Capturing"
)

# Keep the legacy InitApplication implementation intact under an internal
# name. A tiny preservation wrapper intercepts only explicit one-shot replay
# before INI loading, window-class registration or other GUI startup work.
set_source_files_properties( Initapp.cpp PROPERTIES
        COMPILE_DEFINITIONS "InitApplication=LegacyInitApplication"
)

# Create executable
add_executable( PDW WIN32
        ${PDW_INCLUDES}
        ${PDW_SRCS}
)

# Required libraries
target_link_libraries( PDW
        Comctl32.lib
        Dwmapi.lib
        UxTheme.lib
        Winmm.lib
        Ws2_32.lib
        libssl_static.lib
        libcrypto_static.lib
        legacy_stdio_definitions.lib
        Crypt32.lib
        User32.lib
)

if( BUILD_TESTING )
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
