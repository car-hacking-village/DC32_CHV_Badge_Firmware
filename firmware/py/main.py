# use the startup function to hide extra variables that 
# we don't really want users to mess with without
# understanding what they do

import _thread
import engine
import slcan
canbus = engine.can()
output = slcan.slcan()

badge_help = '''
Car Hacking Village Main Badge Defcon 32
You can get started by sending and receiving CAN
messages in python using the "canbus" variable.

I'm sorry that tab completion is broken. Also multiline
commands won't work either...
      

You can also use the slcan interface by running
# sudo slcand -o -s6 -t hw /dev/ttyACM1
# sudo ip link set slcan0 up

'''

del slcan # hide this from the scope so people don't accidentally mess with it

engine.start_engine(canbus,output)