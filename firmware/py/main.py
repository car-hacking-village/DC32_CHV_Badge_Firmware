# use the startup function to hide extra variables that 
# we don't really want users to mess with without
# understanding what they do

import engine
import slcan
canbus = engine.can()
output = slcan.slcan()

badge_help = '''
Car Hacking Village Main Badge Defcon 32
You can get started by sending and receiving CAN
messages in python using the "canbus" variable.

--> canbus.send(0x1, 0x1, b'\\x01')
--> canbus.recv()

This is an async repl, so you can await async functions!

--> await asyncio.sleep(1)

I'm sorry that tab completion is broken. Also multiline
commands won't work either. you should probably run
os.remove('main.py') before trying to reflash the firmware.

You can also use the slcan interface by running
# sudo slcand -o -s6 -t hw /dev/ttyACM1
# sudo ip link set slcan0 up

flag{good_job_you_used_the_badge}

'''

del slcan # hide this from the scope so people don't accidentally mess with it

engine.start_engine(canbus,output)