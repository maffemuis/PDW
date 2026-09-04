add_executable( pdw_decoded_message_test
    core/decoded_message_test.cpp
    ${CMAKE_SOURCE_DIR}/core/decoded_message.cpp
)

target_include_directories( pdw_decoded_message_test PRIVATE
    ${CMAKE_SOURCE_DIR}/core
)

add_test(
    NAME pdw_decoded_message
    COMMAND pdw_decoded_message_test
)

add_executable( pdw_synthetic_message_test
    core/synthetic_message_test.cpp
    ${CMAKE_SOURCE_DIR}/core/decoded_message.cpp
    ${CMAKE_SOURCE_DIR}/core/synthetic_message.cpp
)

target_include_directories( pdw_synthetic_message_test PRIVATE
    ${CMAKE_SOURCE_DIR}/core
)

add_test(
    NAME pdw_synthetic_message
    COMMAND pdw_synthetic_message_test
)

add_executable( pdw_legacy_message_projection_test
    core/legacy_message_projection_test.cpp
    ${CMAKE_SOURCE_DIR}/core/decoded_message.cpp
    ${CMAKE_SOURCE_DIR}/core/legacy_message_projection.cpp
)

target_include_directories( pdw_legacy_message_projection_test PRIVATE
    ${CMAKE_SOURCE_DIR}/core
)

add_test(
    NAME pdw_legacy_message_projection
    COMMAND pdw_legacy_message_projection_test
)

add_executable( pdw_synthetic_mail_contract_test
    core/synthetic_mail_contract_test.cpp
    ${CMAKE_SOURCE_DIR}/core/decoded_message.cpp
    ${CMAKE_SOURCE_DIR}/core/synthetic_message.cpp
    ${CMAKE_SOURCE_DIR}/core/legacy_message_projection.cpp
)

target_include_directories( pdw_synthetic_mail_contract_test PRIVATE
    ${CMAKE_SOURCE_DIR}/core
)

add_test(
    NAME pdw_synthetic_mail_contract
    COMMAND pdw_synthetic_mail_contract_test
)

add_executable( pdw_filter_text_match_test
    core/filter_text_match_test.cpp
    ${CMAKE_SOURCE_DIR}/core/filter_text_match.cpp
)

target_include_directories( pdw_filter_text_match_test PRIVATE
    ${CMAKE_SOURCE_DIR}/core
)

add_test(
    NAME pdw_filter_text_match
    COMMAND pdw_filter_text_match_test
)

add_executable( pdw_filter_multi_edit_test
    core/filter_multi_edit_test.cpp
    ${CMAKE_SOURCE_DIR}/core/filter_multi_edit.cpp
)

target_include_directories( pdw_filter_multi_edit_test PRIVATE
    ${CMAKE_SOURCE_DIR}/core
)

add_test(
    NAME pdw_filter_multi_edit
    COMMAND pdw_filter_multi_edit_test
)

add_executable( pdw_filter_text_storage_test
    core/filter_text_storage_test.cpp
    ${CMAKE_SOURCE_DIR}/core/filter_text_storage.cpp
)

target_include_directories( pdw_filter_text_storage_test PRIVATE
    ${CMAKE_SOURCE_DIR}/core
)

add_test(
    NAME pdw_filter_text_storage
    COMMAND pdw_filter_text_storage_test
)

add_executable( pdw_smtp_protocol_test
    smtp/smtp_protocol_test.cpp
    ${CMAKE_SOURCE_DIR}/utils/smtp_protocol.cpp
)

target_include_directories( pdw_smtp_protocol_test PRIVATE
    ${CMAKE_SOURCE_DIR}/utils
)

add_test(
    NAME pdw_smtp_protocol
    COMMAND pdw_smtp_protocol_test
)

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

add_executable( pdw_preservation_replay_policy_test
    preservation/replay_policy_test.cpp
    ${CMAKE_SOURCE_DIR}/utils/preservation_decoder_replay_policy.cpp
)

target_include_directories( pdw_preservation_replay_policy_test PRIVATE
    ${CMAKE_SOURCE_DIR}/utils
)

add_test(
    NAME pdw_preservation_replay_policy
    COMMAND pdw_preservation_replay_policy_test
)

add_executable( pdw_preservation_golden_test
    preservation/golden_test.cpp
    ${CMAKE_SOURCE_DIR}/utils/preservation_golden.cpp
)

target_include_directories( pdw_preservation_golden_test PRIVATE
    ${CMAKE_SOURCE_DIR}/utils
)

add_test(
    NAME pdw_preservation_golden
    COMMAND pdw_preservation_golden_test
)

add_executable( pdw_preservation_compare
    preservation/golden_compare_main.cpp
    ${CMAKE_SOURCE_DIR}/utils/preservation_golden.cpp
)

target_include_directories( pdw_preservation_compare PRIVATE
    ${CMAKE_SOURCE_DIR}/utils
)

find_program(PDW_PRESERVATION_PYTHON NAMES python3 python)

if(PDW_PRESERVATION_PYTHON)
    add_test(
        NAME pdw_preservation_mobitex_basic
        COMMAND ${CMAKE_COMMAND}
            -DPDW_EXECUTABLE=$<TARGET_FILE:PDW>
            -DCOMPARE_EXECUTABLE=$<TARGET_FILE:pdw_preservation_compare>
            -DPYTHON_EXECUTABLE=${PDW_PRESERVATION_PYTHON}
            -DSOURCE_DIR=${CMAKE_SOURCE_DIR}
            -DOUTPUT_DIR=${CMAKE_BINARY_DIR}/preservation-mobitex
            -P ${CMAKE_CURRENT_SOURCE_DIR}/preservation/run_mobitex.cmake
    )

    set_tests_properties(pdw_preservation_mobitex_basic PROPERTIES TIMEOUT 40)

    add_test(
        NAME pdw_preservation_p2000_numeric_binary_remainder
        COMMAND ${CMAKE_COMMAND}
            -DPDW_EXECUTABLE=$<TARGET_FILE:PDW>
            -DCOMPARE_EXECUTABLE=$<TARGET_FILE:pdw_preservation_compare>
            -DPYTHON_EXECUTABLE=${PDW_PRESERVATION_PYTHON}
            -DSOURCE_DIR=${CMAKE_SOURCE_DIR}
            -DOUTPUT_DIR=${CMAKE_BINARY_DIR}/preservation-p2000-numeric-remainder
            -P ${CMAKE_CURRENT_SOURCE_DIR}/preservation/run_p2000_numeric_remainder.cmake
    )

    set_tests_properties(pdw_preservation_p2000_numeric_binary_remainder PROPERTIES TIMEOUT 40)

    add_test(
        NAME pdw_preservation_p2000_scenario
        COMMAND ${CMAKE_COMMAND}
            -DPDW_EXECUTABLE=$<TARGET_FILE:PDW>
            -DCOMPARE_EXECUTABLE=$<TARGET_FILE:pdw_preservation_compare>
            -DPYTHON_EXECUTABLE=${PDW_PRESERVATION_PYTHON}
            -DSOURCE_DIR=${CMAKE_SOURCE_DIR}
            -DOUTPUT_DIR=${CMAKE_BINARY_DIR}/preservation-p2000-scenario
            -P ${CMAKE_CURRENT_SOURCE_DIR}/preservation/run_p2000_scenario.cmake
    )

    set_tests_properties(pdw_preservation_p2000_scenario PROPERTIES TIMEOUT 120)
else()
    message(WARNING "Python was not found; MOBITEX and P2000 generated preservation replay tests are unavailable")
endif()
