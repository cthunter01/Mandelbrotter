# Integration test: the built executable renders headlessly. Invoked by ctest with -DEXE=... -DOUT_DIR=...
# and an empty DISPLAY, which proves the --render path never touches the GUI toolkit.

file(REMOVE_RECURSE "${OUT_DIR}")
file(MAKE_DIRECTORY "${OUT_DIR}")

function(run_cli expected_rc)
    execute_process(COMMAND "${EXE}" ${ARGN}
        RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
    if(NOT rc EQUAL expected_rc)
        message(FATAL_ERROR "Mandelbrotter ${ARGN}: exit code ${rc}, expected ${expected_rc}\n${out}\n${err}")
    endif()
    set(cli_out "${out}" PARENT_SCOPE)
    set(cli_err "${err}" PARENT_SCOPE)
endfunction()

function(check_png path)
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "${path} was not written")
    endif()
    file(SIZE "${path}" size)
    if(size LESS 100)
        message(FATAL_ERROR "${path} is only ${size} bytes")
    endif()
    file(READ "${path}" header HEX LIMIT 8)
    if(NOT header STREQUAL "89504e470d0a1a0a")
        message(FATAL_ERROR "${path} does not start with the PNG signature (got ${header})")
    endif()
endfunction()

run_cli(0 --help)
if(NOT cli_out MATCHES "--render")
    message(FATAL_ERROR "--help output does not mention --render:\n${cli_out}")
endif()

run_cli(2 --bogus)
if(NOT cli_err MATCHES "unknown option")
    message(FATAL_ERROR "unexpected error output for --bogus:\n${cli_err}")
endif()

run_cli(0 --render "${OUT_DIR}/ship.png" --size 96x64 --iterations 60 --fractal burning-ship
        --center -1.75,-0.03 --zoom 40 --supersample 2 --palette fire)
check_png("${OUT_DIR}/ship.png")
if(NOT cli_out MATCHES "96x64")
    message(FATAL_ERROR "unexpected --render output:\n${cli_out}")
endif()

file(WRITE "${OUT_DIR}/view.json" [[
{"version": 1, "settings": {
  "fractal": {"family": "tricorn", "exponent": 3, "julia": false, "seed": {"re": 0, "im": 0}},
  "view": {"center": {"re": -0.25, "im": 0.0}, "zoom": 1.5},
  "iterations": {"max": 5000, "auto": false},
  "coloring": {"palette": "ocean", "density": 32.0, "offset": 0.25}}}
]])
run_cli(0 --view "${OUT_DIR}/view.json" --render "${OUT_DIR}/view.png" --size 48x48 --iterations 30)
check_png("${OUT_DIR}/view.png")
if(NOT cli_out MATCHES "tricorn" OR NOT cli_out MATCHES "30 iterations")
    message(FATAL_ERROR "--iterations did not override the view file:\n${cli_out}")
endif()

run_cli(1 --view "${OUT_DIR}/missing.json" --render "${OUT_DIR}/never.png")
if(EXISTS "${OUT_DIR}/never.png")
    message(FATAL_ERROR "a PNG was written despite the missing view file")
endif()

message(STATUS "CLI render test passed")
