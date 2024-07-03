import time
import machine


class leds() :
    _leds = (
        (True, False, None, None),
        (True, None, False, None),
        (True, None, None, False),
        (False, True, None, None),
        (False, None, True, None),
        (False, None, None, True),
        (None, True, False, None),
        (None, True, None, False),
        (None, False, True, None),
        (None, False, None, True),
        (None, None, True, False),
        (None, None, False, True),
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
        self.count = 0
        self.itteration = 0
        self.speed = 100
        self.reverse = False

    def do_loop_step(self) -> None:
        if (self.itteration % self.speed) == 0:
            # self.itteration = 0
            for state, lineNo in zip(self._leds[self.count], range(len(self._lines))):
                if state == None:
                    self._lines[lineNo].init(mode=machine.Pin.IN)
                else:
                    self._lines[lineNo].init(mode=machine.Pin.OUT)
                    if state is True:
                        self._lines[lineNo].high()
                    else:
                        self._lines[lineNo].low()

            if self.reverse:
                self.count = (self.count-1) % len(self._leds)
            else:
                self.count = (self.count+1) % len(self._leds)
        self.itteration += 1


# _lines = [
#     machine.Pin(6, machine.Pin.OUT),
#     machine.Pin(5, machine.Pin.OUT),
#     machine.Pin(4, machine.Pin.OUT),
#     machine.Pin(3, machine.Pin.OUT),
# ]



# def do_led(a):
#     for s, l in zip(a, range(4)):
#         if s is None:
#             _lines[l].init(mode=machine.Pin.IN)
#         else:
#             _lines[l].init(mode=machine.Pin.OUT)
#             if s is True:
#                 _lines[l].high()
#             else:
#                 _lines[l].low()