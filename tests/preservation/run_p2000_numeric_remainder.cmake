if(NOT DEFINED PDW_EXECUTABLE OR NOT DEFINED COMPARE_EXECUTABLE OR NOT DEFINED PYTHON_EXECUTABLE OR NOT DEFINED SOURCE_DIR OR NOT DEFINED OUTPUT_DIR)
    message(FATAL_ERROR "missing required P2000 numeric remainder test argument")
endif()

file(MAKE_DIRECTORY "${OUTPUT_DIR}")
set(fixture "${OUTPUT_DIR}/p2000-1200-numeric-binary-remainder.wav")
set(capture "${OUTPUT_DIR}/p2000-1200-numeric-binary-remainder.full.jsonl")
set(actual "${OUTPUT_DIR}/p2000-1200-numeric-binary-remainder.actual.jsonl")
set(expected "${SOURCE_DIR}/tests/preservation/golden/p2000-1200-numeric-binary-remainder.jsonl")

execute_process(
    COMMAND "${PYTHON_EXECUTABLE}" "${SOURCE_DIR}/tests/preservation/generate_p2000_fixture.py" "${fixture}" numeric-binary-remainder
    RESULT_VARIABLE fixture_result
    OUTPUT_VARIABLE fixture_stdout
    ERROR_VARIABLE fixture_stderr
)
if(NOT fixture_result EQUAL 0)
    message(FATAL_ERROR "numeric remainder fixture generation failed: ${fixture_result}\n${fixture_stdout}\n${fixture_stderr}")
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
    message(FATAL_ERROR "numeric remainder replay failed: ${pdw_result}")
endif()

execute_process(
    COMMAND "${COMPARE_EXECUTABLE}" "${expected}" "${actual}"
    RESULT_VARIABLE compare_result
)
if(NOT compare_result EQUAL 0)
    message(FATAL_ERROR "numeric binary remainder golden mismatch")
endif()
