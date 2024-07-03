import digitalio
import board
import time

leds = [
  digitalio.DigitalInOut(board.GP5),
  digitalio.DigitalInOut(board.GP6),
  digitalio.DigitalInOut(board.GP7),
  digitalio.DigitalInOut(board.GP10),
  digitalio.DigitalInOut(board.GP11),
]

for led in leds:
  led.direction = digitalio.Direction.OUTPUT

while True:
  for i in range(0,len(leds)):
    leds[i].value = True
    time.sleep(1)
    leds[i].value = False
