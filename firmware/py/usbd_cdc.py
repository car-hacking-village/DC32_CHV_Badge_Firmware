from usbd import device, cdc

class cdc_data():
    _dev = None

    def __init__(self) -> None:
        if self._dev == None:
            ud = device.get_usbdevice()
            cdc.setup_CDC_device()
            ctrl_cdc = cdc.CDCControlInterface('cdc interface')
            data_cdc = cdc.CDCDataInterface('data interface')
            ud.add_interface(ctrl_cdc)
            ud.add_interface(data_cdc)
            ud.reenumerate()
            self.__set_dev(data_cdc)
    
    @property
    def dev(self):
        return self._dev
        
    @classmethod
    def __set_dev(cls, dev):
        if cls._dev == None:
            cls._dev = dev
        else:
            raise ValueError("Error: cdc data device already initialized")
