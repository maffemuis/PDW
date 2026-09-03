if(NOT DEFINED PDW_EXECUTABLE OR NOT DEFINED COMPARE_EXECUTABLE OR NOT DEFINED PYTHON_EXECUTABLE OR NOT DEFINED SOURCE_DIR OR NOT DEFINED OUTPUT_DIR)
    message(FATAL_ERROR "ERMES preservation runner is missing required paths")
endif()

file(MAKE_DIRECTORY "${OUTPUT_DIR}")

set(FIXTURE "${OUTPUT_DIR}/ermes-tone.bin")
set(CAPTURE "${OUTPUT_DIR}/ermes-tone.full.jsonl")
set(ACTUAL "${OUTPUT_DIR}/ermes-tone.actual.jsonl")
set(EXPECTED "${SOURCE_DIR}/tests/preservation/golden/ermes-tone.jsonl")
set(GENERATOR "${SOURCE_DIR}/tests/preservation/generate_ermes_fixture.py")

execute_process(
    COMMAND "${PYTHON_EXECUTABLE}" "${GENERATOR}" "${FIXTURE}"
    RESULT_VARIABLE GENERATOR_RESULT
    OUTPUT_VARIABLE GENERATOR_OUTPUT
    ERROR_VARIABLE GENERATOR_ERROR
)

if(NOT GENERATOR_RESULT EQUAL 0)
    message(FATAL_ERROR "ERMES fixture generation failed (${GENERATOR_RESULT})\n${GENERATOR_OUTPUT}\n${GENERATOR_ERROR}")
endif()

file(WRITE "${CAPTURE}" "")
file(WRITE "${ACTUAL}" "")

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env
        "PDW_PRESERVATION_PROTOCOL=ermes"
        "PDW_PRESERVATION_ERMES_SYMBOLS=${FIXTURE}"
        "PDW_PRESERVATION_CAPTURE=${CAPTURE}"
        "PDW_PRESERVATION_GOLDEN_CAPTURE=${ACTUAL}"
        "PDW_PRESERVATION_REPLAY_EXIT=1"
        "${PDW_EXECUTABLE}"
    WORKING_DIRECTORY "${SOURCE_DIR}"
    RESULT_VARIABLE REPLAY_RESULT
    OUTPUT_VARIABLE REPLAY_OUTPUT
    ERROR_VARIABLE REPLAY_ERROR
    TIMEOUT 30
)

if(NOT REPLAY_RESULT EQUAL 0)
    message(FATAL_ERROR "PDW ERMES symbol replay failed (${REPLAY_RESULT})\n${REPLAY_OUTPUT}\n${REPLAY_ERROR}")
endif()

execute_process(
    COMMAND "${COMPARE_EXECUTABLE}" "${EXPECTED}" "${ACTUAL}"
    RESULT_VARIABLE COMPARE_RESULT
    OUTPUT_VARIABLE COMPARE_OUTPUT
    ERROR_VARIABLE COMPARE_ERROR
)

if(NOT COMPARE_RESULT EQUAL 0)
    file(READ "${EXPECTED}" EXPECTED_TEXT)
    file(READ "${ACTUAL}" ACTUAL_TEXT)
    message(FATAL_ERROR
        "ERMES golden comparison failed (${COMPARE_RESULT})\n"
        "Expected:\n${EXPECTED_TEXT}\n"
        "Actual:\n${ACTUAL_TEXT}\n"
        "Comparator:\n${COMPARE_OUTPUT}\n${COMPARE_ERROR}")
endif()

message(STATUS "ERMES serial-symbol preservation replay matched golden output")
