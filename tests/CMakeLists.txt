add_executable( pdw_preservation_capture_test
    preservation/capture_test.cpp
    ${CMAKE_SOURCE_DIR}/utils/preservation_capture.cpp
)

target_include_directories( pdw_preservation_capture_test PRIVATE
    ${CMAKE_SOURCE_DIR}/utils
)

add_test(
    NAME pdw_preservation_capture
    COMMAND pdw_preservation_capture_test
)

add_executable( pdw_preservation_wav_test
    preservation/wav_test.cpp
    ${CMAKE_SOURCE_DIR}/utils/preservation_wav.cpp
)

target_include_directories( pdw_preservation_wav_test PRIVATE
    ${CMAKE_SOURCE_DIR}/utils
)

add_test(
    NAME pdw_preservation_wav
    COMMAND pdw_preservation_wav_test
)
