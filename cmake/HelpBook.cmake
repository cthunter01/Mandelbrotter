# The help book: docs/help (pages, contents, index, images) is zipped at build time and embedded into the GUI
# library as string literals, so the executable needs no files beside it. wxHtmlHelpController reads the zip
# through wx's memory file system ("memory:help.zip", see src/gui/HelpController.cpp).
#
#   Mandelbrotter_add_help_book(<target>)   adds the generated source to <target>
#
# Adding a page or image reconfigures (CONFIGURE_DEPENDS); editing one re-zips and recompiles a single file.

function(Mandelbrotter_add_help_book target)
    set(help_dir ${PROJECT_SOURCE_DIR}/docs/help)
    file(GLOB_RECURSE MANDELBROTTER_HELP_FILES CONFIGURE_DEPENDS
         ${help_dir}/*.hhp ${help_dir}/*.hhc ${help_dir}/*.hhk ${help_dir}/*.html ${help_dir}/images/*.png)
    set(help_entries "")
    foreach(f IN LISTS MANDELBROTTER_HELP_FILES)   # archive entries are relative: help.hhp, images/x.png
        cmake_path(RELATIVE_PATH f BASE_DIRECTORY ${help_dir} OUTPUT_VARIABLE rel)
        list(APPEND help_entries ${rel})
    endforeach()

    set(help_zip ${PROJECT_BINARY_DIR}/help/help.zip)
    set(help_cpp ${PROJECT_BINARY_DIR}/help/help_book_data.cpp)
    add_custom_command(OUTPUT ${help_zip}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${PROJECT_BINARY_DIR}/help
        COMMAND ${CMAKE_COMMAND} -E tar cf ${help_zip} --format=zip ${help_entries}
        WORKING_DIRECTORY ${help_dir}
        DEPENDS ${MANDELBROTTER_HELP_FILES}
        COMMENT "Packing the help book"
        VERBATIM)
    add_custom_command(OUTPUT ${help_cpp}
        COMMAND Mandelbrotter_embed ${help_zip} ${help_cpp} gui/help_book.h mandelbrotter::gui helpBookChunks
        DEPENDS ${help_zip} Mandelbrotter_embed
        COMMENT "Embedding the help book"
        VERBATIM)
    # Generated data: never fed to clang-tidy (SKIP_LINTING needs CMake 3.27; the project requires 3.28).
    set_source_files_properties(${help_cpp} PROPERTIES GENERATED TRUE SKIP_LINTING ON)
    target_sources(${target} PRIVATE ${help_cpp})

    add_custom_target(Mandelbrotter_help_book DEPENDS ${help_zip})
    set_property(GLOBAL PROPERTY MANDELBROTTER_HELP_ZIP ${help_zip})
endfunction()
