function(CAPNP_GENERATE_POD_CPP SOURCES HEADERS)
    if(NOT ARGN)
        message(SEND_ERROR "CAPNP_GENERATE_POD_CPP() called without any source files.")
    endif()

    if(DEFINED CAPNP_PODGEN_OUTPUT_DIR)
        # Prepend a ':' to get the format for the '-o' flag right
        set(output_dir ":${CAPNP_PODGEN_OUTPUT_DIR}")
    else()
        set(output_dir ":.")
    endif()
            
    set(${SOURCES})
    set(${HEADERS})

    foreach (src ${ARGN})
        get_filename_component(filename ${src} NAME_WE)        
        list(APPEND ${HEADERS} "${CAPNP_PODGEN_OUTPUT_DIR}/${filename}.pod.hpp")
        list(APPEND ${HEADERS} "${CAPNP_PODGEN_OUTPUT_DIR}/${filename}.convert.hpp")
        list(APPEND ${SOURCES} "${CAPNP_PODGEN_OUTPUT_DIR}/${filename}.convert.cpp")         
    endforeach ()

    add_custom_command(OUTPUT ${${HEADERS}} ${${SOURCES}}
            COMMAND $<TARGET_FILE:podgen> ${ARGN} -c "${CAPNP_INCLUDE_DIRS}" -t "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../include" -o "${CAPNP_PODGEN_OUTPUT_DIR}"
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
            DEPENDS ${ARGN} $<TARGET_FILE:podgen>
            COMMENT "Generating Cap'n Proto POD sources"
            VERBATIM)    

    set_source_files_properties(${${SOURCES}} ${${HEADERS}} PROPERTIES GENERATED TRUE)
    set(${SOURCES} ${${SOURCES}} PARENT_SCOPE)
    set(${HEADERS} ${${HEADERS}} PARENT_SCOPE)
endfunction()
