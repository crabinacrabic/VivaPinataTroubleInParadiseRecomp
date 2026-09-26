# Copies packaged/ next to retip.exe after a build.
#
# Usage: cmake -DSRC=<repo>/packaged -DDST=<exe dir> -P stage-packaged.cmake
#
# Everything is copied when it differs, except retip.toml: the launcher writes
# the player's settings into it (rex::cvar::SaveConfig), so an existing copy is
# left alone. Delete it next to the exe to get the defaults back.

if(NOT DEFINED SRC OR NOT DEFINED DST)
    message(FATAL_ERROR "stage-packaged.cmake: SRC and DST are required")
endif()

file(GLOB_RECURSE packaged_files RELATIVE "${SRC}" "${SRC}/*")
foreach(rel IN LISTS packaged_files)
    set(from "${SRC}/${rel}")
    set(to "${DST}/${rel}")
    if(rel STREQUAL "retip.toml" AND EXISTS "${to}")
        continue()
    endif()
    get_filename_component(to_dir "${to}" DIRECTORY)
    file(MAKE_DIRECTORY "${to_dir}")
    execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different "${from}" "${to}")
endforeach()
