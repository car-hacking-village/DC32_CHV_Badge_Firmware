# add_mpy_py(boot ${CMAKE_CURRENT_SOURCE_DIR}/boot.py)
# add_mpy_py(main ${CMAKE_CURRENT_SOURCE_DIR}/main.py)
add_mpy_py(slcan ${CMAKE_CURRENT_SOURCE_DIR}/slcan.py)
add_mpy_py(leds ${CMAKE_CURRENT_SOURCE_DIR}/leds.py)
add_mpy_py(engine ${CMAKE_CURRENT_SOURCE_DIR}/engine.py)
add_mpy_py(usbd_cdc ${CMAKE_CURRENT_SOURCE_DIR}/usbd_cdc.py)

# I'm tired of talking about why this is commented out
# add_subdirectory(can2040_py)


install(
    FILES 
        ${CMAKE_CURRENT_SOURCE_DIR}/main.py
    DESTINATION 
        ${CMAKE_INSTALL_PREFIX}
)

install (
    FILES
        ${CMAKE_CURRENT_LIST_DIR}/install.sh 
    DESTINATION
        ${CMAKE_INSTALL_PREFIX}
    PERMISSIONS
        OWNER_READ OWNER_WRITE OWNER_EXECUTE
        GROUP_READ GROUP_EXECUTE
        WORLD_READ WORLD_EXECUTE
)