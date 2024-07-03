# cmake_minimum_required(VERSION 3.20)
# include(deps/pico-sdk/pico_sdk_init.cmake) # Has to be here
# project(CHV_DC31 C CXX ASM)

# add_subdirectory(deps/micropython/ports/rp2)
include(ExternalProject)
set(INSTALL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/release)
file(MAKE_DIRECTORY ${INSTALL_DIR})



add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/deps ${CMAKE_CURRENT_BINARY_DIR}/usermod_deps)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/verification ${CMAKE_CURRENT_BINARY_DIR}/usermod_verification)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/firmware ${CMAKE_CURRENT_BINARY_DIR}/usermod_firmware)



## SCREW MICROPYTHON. I hate how they are doing their cmake. Honestly it's so bad.
## Who ever designed it had to have come from a Make background and doesn't fully understand
## cmake. 
#
## We have to add this here and not in the actual directory because they don't support the generator
## expressions added in 3.13 for get_target_properties.
## This wouldn't be a problem except that they are forcing their project to be the toplevel which is
## trash for letting other people extend their project. screw them.
add_mpy_c_module(can2040_py 
    SRC ${CMAKE_CURRENT_LIST_DIR}/firmware/py/can2040_py/can2040_bindings.c 
    DEPS hardware_claim can2040_interface)

# Install the firmware so everything is in one nice folder
install(FILES ${CMAKE_CURRENT_LIST_DIR}/build/firmware.uf2
        DESTINATION ${CMAKE_INSTALL_PREFIX}
)
