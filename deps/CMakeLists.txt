
########### rp2040 init ###########
# pico_sdk_init()

########### CAN2040 stuff ###########
add_library(
    can2040 STATIC
    can2040/src/can2040.c
)
target_include_directories(can2040 PUBLIC ${CMAKE_CURRENT_LIST_DIR}/can2040/src/)
target_link_libraries(can2040 PUBLIC pico_stdlib hardware_pio cmsis_core)

add_library(
    can2040_interface INTERFACE
)
target_sources(
    can2040_interface INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/can2040/src/can2040.c
)
target_include_directories(can2040_interface INTERFACE ${CMAKE_CURRENT_LIST_DIR}/can2040/src/)

########### MicroPython stuff ###########
set(MP_DIR "${CMAKE_CURRENT_LIST_DIR}/micropython" CACHE STRING "" )
set(MP_CROSS_DIR ${MP_DIR}/mpy-cross)
set(MPY_CROSS ${MP_CROSS_DIR}/build/mpy-cross CACHE STRING "")
set(MPY_TOOL ${MP_DIR}/tools/mpy-tool.py CACHE STRING "")
set(MPY_LD ${MP_DIR}/tools/mpy_ld.py CACHE STRING "")


#TODO: This should already be compiled... where is it?
function(check_mpy_cross)
    if (NOT TARGET mpy_cross)
        ExternalProject_Add( mpy_cross
            SOURCE_DIR ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/micropython/mpy-cross
            BINARY_DIR ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/micropython/mpy-cross
            # No download step
            # No update
            # No patch
            CONFIGURE_COMMAND "" # No configure needed, but must be specified
            # default build command
            INSTALL_COMMAND "" # Don't actually install
            # No test
        )
    endif()
endfunction()


function(add_mpy_py TARGET FILE)
    check_mpy_cross()
    set(output_file ${CMAKE_CURRENT_BINARY_DIR}/${TARGET}.mpy)
    add_custom_command(
        OUTPUT ${output_file}
        COMMAND echo "BUILDING ${FILE}"
        COMMAND ${MPY_CROSS} ${FILE} -o ${output_file}
        DEPENDS mpy_cross ${FILE} ${MPY_CROSS} 
    )

    add_custom_target(
        ${TARGET} ALL
        DEPENDS ${output_file}
    )
    install(FILES ${output_file} DESTINATION ${CMAKE_INSTALL_PREFIX}/${ARGN})
endfunction()


function(add_mpy_c_module TARGET)

    set(MUTLI_VALUE_ARGS SRC INCLUDE DEPS)
    cmake_parse_arguments(PARSE_ARGV 1 MPY_C "" "" "${MUTLI_VALUE_ARGS}")

    add_library(
        ${TARGET} INTERFACE
    )

    target_sources(
        ${TARGET} INTERFACE
        ${MPY_C_SRC}
    )

    if(NOT "${MPY_C_INCLUDE}" STREQUAL "" )
        target_include_directories(
            ${TARGET} INTERFACE
            ${MPY_C_INCLUDE}
        )
    endif()

    target_link_libraries(${TARGET} INTERFACE ${MPY_C_DEPS})

    target_link_libraries(usermod INTERFACE ${TARGET})

endfunction()

########### MicroPython-lib stuff ###########
set(usbd_dir ${CMAKE_CURRENT_LIST_DIR}/micropython-lib/micropython/usbd)
set(usbd_srcs
    ${usbd_dir}/cdc.py
    ${usbd_dir}/device.py
    ${usbd_dir}/hid.py
    ${usbd_dir}/midi.py
    ${usbd_dir}/utils.py
    ${usbd_dir}/__init__.py
)

# install(FILES ${output_file} DESTINATION ${CMAKE_INSTALL_PREFIX}/${ARGN})
foreach(X ${usbd_srcs})
    get_filename_component(USBD_FILE ${X} NAME_WE)
    add_mpy_py(${USBD_FILE} ${X} "usbd/")
endforeach()