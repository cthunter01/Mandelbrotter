# Checks the help book zip built by cmake/HelpBook.cmake (invoked by ctest with -DZIP=...): wx's help controller
# looks for the .hhp at the archive root, and every page and picture must sit beside it, not under docs/help/.

if(NOT EXISTS "${ZIP}")
    message(FATAL_ERROR "${ZIP} was not built")
endif()

execute_process(COMMAND ${CMAKE_COMMAND} -E tar tf "${ZIP}"
    RESULT_VARIABLE rc OUTPUT_VARIABLE listing ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
    message(FATAL_ERROR "cannot list ${ZIP}: ${err}")
endif()
string(STRIP "${listing}" listing)
string(REPLACE "\n" ";" entries "${listing}")

foreach(required help.hhp contents.hhc index.hhk index.html)
    if(NOT required IN_LIST entries)
        message(FATAL_ERROR "${required} is missing from ${ZIP}:\n${listing}")
    endif()
endforeach()
foreach(entry IN LISTS entries)
    if(entry MATCHES "^(docs/|/|\\.\\./)")
        message(FATAL_ERROR "${ZIP} entry \"${entry}\" is not relative to the book root")
    endif()
endforeach()

list(LENGTH entries count)
message(STATUS "help book zip OK: ${count} entries")
