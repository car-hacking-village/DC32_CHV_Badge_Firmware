from usb.device.cdc import CDCInterface
import usb.device
import time
import os

class cdc_data():
    _dev = None

    def __init__(self) -> None:
        if self._dev == None:
            self._dev = CDCInterface()
            self._dev.init(timeout=0)

            usb.device.get().init(self._dev, builtin_driver=True)

            while not self._dev.is_open():
                time.sleep_ms(100)

            os.dupterm(self._dev)

    @property
    def dev(self):
        return self._dev
        
    @classmethod
    def __set_dev(cls, dev):
        if cls._dev == None:
            cls._dev = dev
        else:
            raise ValueError("Error: cdc data device already initialized")

