if(NOT DEFINED PDW_EXECUTABLE OR NOT DEFINED COMPARE_EXECUTABLE OR NOT DEFINED PYTHON_EXECUTABLE OR NOT DEFINED SOURCE_DIR OR NOT DEFINED OUTPUT_DIR)
    message(FATAL_ERROR "missing required P2000 scenario test argument")
endif()

file(MAKE_DIRECTORY "${OUTPUT_DIR}")

set(P2000_SCENARIO_CASES
    short-alpha
    short-alpha-divisor
    numeric-binary-remainder
    bad-address
)

foreach(case_name IN LISTS P2000_SCENARIO_CASES)
    set(fixture "${OUTPUT_DIR}/p2000-1200-${case_name}.wav")
    set(capture "${OUTPUT_DIR}/p2000-1200-${case_name}.full.jsonl")
    set(actual "${OUTPUT_DIR}/p2000-1200-${case_name}.actual.jsonl")
    set(expected "${SOURCE_DIR}/tests/preservation/golden/p2000-1200-${case_name}.jsonl")

    if(NOT EXISTS "${expected}")
        message(FATAL_ERROR "missing golden for scenario case ${case_name}: ${expected}")
    endif()

    execute_process(
        COMMAND "${PYTHON_EXECUTABLE}" "${SOURCE_DIR}/tests/preservation/generate_p2000_fixture.py" "${fixture}" "${case_name}"
        RESULT_VARIABLE fixture_result
        OUTPUT_VARIABLE fixture_stdout
        ERROR_VARIABLE fixture_stderr
    )
    if(NOT fixture_result EQUAL 0)
        message(FATAL_ERROR "P2000 scenario fixture ${case_name} failed: ${fixture_result}\n${fixture_stdout}\n${fixture_stderr}")
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
        message(FATAL_ERROR "P2000 scenario replay ${case_name} failed: ${pdw_result}")
    endif()

    execute_process(
        COMMAND "${COMPARE_EXECUTABLE}" "${expected}" "${actual}"
        RESULT_VARIABLE compare_result
    )
    if(NOT compare_result EQUAL 0)
        message(FATAL_ERROR "P2000 scenario golden mismatch: ${case_name}")
    endif()
endforeach()
