# use the startup function to hide extra variables that 
# we don't really want users to mess with without
# understanding what they do
def startup():
    import _thread
    import engine
    import slcan
    canbus = engine.can()
    output = slcan.slcan()
    import time
    time.sleep(3)
    _thread.start_new_thread(engine.start_engine, (canbus,output))
    return canbus



print('''
Car Hacking Village Main Badge Defcon 32
You can get started by sending and receiving CAN
messages in python using the "canbus" variable.

You can also use the slcan interface by running
# sudo slcand -o -s6 -t hw /dev/ttyACM1
# sudo ip link set slcan0 up

''')

canbus = startup()
del startup