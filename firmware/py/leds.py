import machine
import asyncio


class leds() :
    _leds = (
        (None, None, False, True),
        (None, False, None, True),
        (False, None, None, True),
        (None, None, True, False),
        (None, False, True, None),
        (False, None, True, None),
        (None, True, None, False),
        (None, True, False, None),
        (False, True, None, None),
        (True, None, None, False),
        (True, None, False, None),
        (True, False, None, None),
    )

    _lines = (
        (
            machine.Pin(2, machine.Pin.OUT),
            machine.Pin(3, machine.Pin.OUT),
            machine.Pin(4, machine.Pin.OUT),
            machine.Pin(5, machine.Pin.OUT),
        ),
        (
            machine.Pin(8, machine.Pin.OUT),
            machine.Pin(9, machine.Pin.OUT),
            machine.Pin(10, machine.Pin.OUT),
            machine.Pin(11, machine.Pin.OUT),
        ),
        (
            machine.Pin(12, machine.Pin.OUT),
            machine.Pin(13, machine.Pin.OUT),
            machine.Pin(14, machine.Pin.OUT),
            machine.Pin(15, machine.Pin.OUT),
        ),
    )


    def __init__(self) -> None:
        self._count = 1
        self._itteration = 0
        self._speed = 100
        self._reverse = False

    def set_cars(self, count:int) -> None:
        if 0 < count <= 3:
            self._count = count
        else:
            self._count = 1

    def set_speed(self, speed:int) -> None:
        if 0 <= speed <= 10000:
            self.speed = speed

    def set_direction(self, direction:bool) -> None:
        self.reverse = direction

    async def _do_leds(self, run) -> None:
        while not run.is_set():
            # do reset
            touched = [False, False, False]

            for x in range(self._count):
                ledNum = (self._itteration+((36//self._count)*x)) % 12
                ledSet = ((self._itteration+((36//self._count)*x)) // 12) %3
                touched[ledSet] = True
                for i in range(4):
                    if self._leds[ledNum][i] == None:
                        self._lines[ledSet][i].init(mode=machine.Pin.IN)    
                    else:
                        self._lines[ledSet][i].init(mode=machine.Pin.OUT)
                        if self._leds[ledNum][i] == True:
                            self._lines[ledSet][i].high()
                        else:
                            self._lines[ledSet][i].low()

                for p in range(3):
                    if not touched[p]:
                        for q in range(4):
                            self._lines[p][q].init(mode=machine.Pin.IN)
                     
            self._itteration = (self._itteration + 1) % 36

            await asyncio.sleep_ms(self._speed)

