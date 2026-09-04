# CMakeLists.txt
# Portable decoded-message core. Keep this layer free of Win32 and legacy globals.

set( PDW_INCLUDES
  ${PDW_INCLUDES}
  ${CMAKE_CURRENT_SOURCE_DIR}/decoded_message.h
  ${CMAKE_CURRENT_SOURCE_DIR}/filter_text_match.h

  PARENT_SCOPE
)

set( PDW_SRCS
  ${PDW_SRCS}
  ${CMAKE_CURRENT_SOURCE_DIR}/decoded_message.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/filter_text_match.cpp

  PARENT_SCOPE
)
