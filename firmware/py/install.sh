#!/bin/bash

wait_for_file () {
    echo -e "\033[100DWaiting for $1"
    while [ 1 ]; do
        ls $1 1>/dev/null 2>&1 && break
        sleep 0.5
    done
    echo -en "\033[1A\033[100D                         "
}

while [ 1 ]; do
    # flash with micropython
    echo -e "\033[100D%%%% Connect badge and enter bootloader %%%%"
    echo "press reset + bootsel and release reset first"
    wait_for_file /media/$USER/RPI-RP2
    echo -en "\033[100D                                                        "
    echo -en "\033[1A\033[100D                                                        "
    echo -en "\033[1A\033[100DFlashing firmware                                       "
    cp firmware.uf2 /media/$USER/RPI-RP2/
    sleep 2

    # flash with python code
    wait_for_file /dev/ttyACM0
    for files in *py usbd/ ; do
        echo -en "\033[100DFlashing $files                                            "
        ampy -p /dev/ttyACM0 put $files
        sleep 1
    done

    # reset to make sure everything worked
    echo -en "\033[100D##### PRESS RESET BUTTON #####                                 "
    sleep 5
    echo -en '\033[100D                                       '
done