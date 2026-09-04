# CMakeLists.txt
# Portable decoded-message core. Keep this layer free of Win32 and legacy globals.

set( PDW_INCLUDES
  ${PDW_INCLUDES}
  ${CMAKE_CURRENT_SOURCE_DIR}/decoded_message.h
  ${CMAKE_CURRENT_SOURCE_DIR}/filter_text_match.h
  ${CMAKE_CURRENT_SOURCE_DIR}/filter_multi_edit.h
  ${CMAKE_CURRENT_SOURCE_DIR}/filter_text_storage.h
  ${CMAKE_CURRENT_SOURCE_DIR}/synthetic_message.h
  ${CMAKE_CURRENT_SOURCE_DIR}/legacy_message_projection.h

  PARENT_SCOPE
)

set( PDW_SRCS
  ${PDW_SRCS}
  ${CMAKE_CURRENT_SOURCE_DIR}/decoded_message.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/filter_text_match.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/filter_multi_edit.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/filter_text_storage.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/synthetic_message.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/legacy_message_projection.cpp

  PARENT_SCOPE
)
