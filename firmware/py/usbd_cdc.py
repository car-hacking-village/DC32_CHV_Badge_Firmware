from usb.device.cdc import CDCInterface
import usb.device
import time
import os

import asyncio

class cdc_data():
    _dev = None
    # _lock = asyncio.Lock()
    _msg_buf = []

    def __init__(self, timeout=0) -> None:
        if self._dev == None:
            self._dev = CDCInterface()
            self._dev.init(timeout=timeout)

            usb.device.core.get().init(self._dev, builtin_driver=True)

            # while not self._dev.is_open():
            #     time.sleep_ms(timeout + 100)

            # os.dupterm(self._dev)
            
    def read(self, length):
        # async with self._lock:
        fin = self._dev.read(length)
        return fin


    def write(self, buf):
        # async with self._lock:
        self._dev.write(buf)

    def is_open(self) -> bool:
        return self._dev.is_open()

    # @property
    # def dev(self):
    #     return self._dev
        
    @classmethod
    def __set_dev(cls, dev):
        if cls._dev == None:
            cls._dev = dev
        else:
            raise ValueError("Error: cdc data device already initialized")

