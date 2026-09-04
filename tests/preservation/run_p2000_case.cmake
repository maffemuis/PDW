if(NOT DEFINED PDW_EXECUTABLE OR NOT DEFINED COMPARE_EXECUTABLE OR NOT DEFINED PYTHON_EXECUTABLE OR NOT DEFINED SOURCE_DIR OR NOT DEFINED OUTPUT_DIR OR NOT DEFINED CASE_NAME OR NOT DEFINED GOLDEN_NAME)
    message(FATAL_ERROR "missing required P2000 preservation case argument")
endif()

if(NOT CASE_NAME MATCHES "^[A-Za-z0-9-]+$")
    message(FATAL_ERROR "invalid P2000 CASE_NAME")
endif()
if(NOT GOLDEN_NAME MATCHES "^[A-Za-z0-9._-]+$")
    message(FATAL_ERROR "invalid P2000 GOLDEN_NAME")
endif()

file(MAKE_DIRECTORY "${OUTPUT_DIR}")
set(fixture "${OUTPUT_DIR}/${CASE_NAME}.wav")
set(capture "${OUTPUT_DIR}/${CASE_NAME}.full.jsonl")
set(actual "${OUTPUT_DIR}/${CASE_NAME}.actual.jsonl")
set(expected "${SOURCE_DIR}/tests/preservation/golden/${GOLDEN_NAME}")

if(NOT EXISTS "${expected}")
    message(FATAL_ERROR "P2000 golden file does not exist: ${expected}")
endif()

execute_process(
    COMMAND "${PYTHON_EXECUTABLE}" "${SOURCE_DIR}/tests/preservation/generate_p2000_fixture.py" "${fixture}" "${CASE_NAME}"
    RESULT_VARIABLE fixture_result
    OUTPUT_VARIABLE fixture_stdout
    ERROR_VARIABLE fixture_stderr
)
if(NOT fixture_result EQUAL 0)
    message(FATAL_ERROR "P2000 ${CASE_NAME} fixture generation failed: ${fixture_result}\n${fixture_stdout}\n${fixture_stderr}")
endif()

file(WRITE "${capture}" "")
file(WRITE "${actual}" "")

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env
        "PDW_PRESERVATION_REPLAY_WAV=${fixture}"
        "PDW_PRESERVATION_CAPTURE=${capture}"
        "PDW_PRESERVATION_GOLDEN_CAPTURE=${actual}"
        "PDW_PRESERVATION_REPLAY_EXIT=1"
        "${PDW_EXECUTABLE}"
    WORKING_DIRECTORY "${SOURCE_DIR}"
    RESULT_VARIABLE pdw_result
    TIMEOUT 30
)
if(NOT pdw_result EQUAL 0)
    message(FATAL_ERROR "P2000 ${CASE_NAME} replay failed: ${pdw_result}")
endif()

execute_process(
    COMMAND "${COMPARE_EXECUTABLE}" "${expected}" "${actual}"
    RESULT_VARIABLE compare_result
)
if(NOT compare_result EQUAL 0)
    message(FATAL_ERROR "P2000 ${CASE_NAME} golden mismatch")
endif()
