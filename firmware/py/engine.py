import time
import collections
import asyncio
import aiorepl

from leds import leds

RUN_ENGINE = asyncio.Event()
BE_QUIET_SILLY_ENGINE = False

BADGE_HELP = '''
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

class can():
    def __init__(self) -> None:
        import _canbus # do the import here so it's hidden from engine's scope
        self.bus = _canbus.bus()
        self.rx_queue = collections.deque(tuple(), 20)
        self.tx_queue = collections.deque(tuple(), 20)
    
    def send(self, arbid:int, dlc:int, data:bytes, extended=False, remote:bool=False) -> None:
        self.tx_queue.append((arbid,dlc,data,extended, remote))

    def recv(self):# -> Tuple[int, int, bytes]:
        try:
            return self.rx_queue.popleft()
        except IndexError:
            return None

    def _send_report(self, arbid:int, dlc:int, data:bytes, extended=False, remote:bool=False) -> None:
        self.send(arbid, dlc, data, extended, remote)
        try:
            self.rx_queue.append((arbid, dlc, data, extended, remote))
        except Exception:
            pass

    # Don't call this. It doesn't exist.
    # Stop looking at it
    def _send(self, counter) -> None:
        if self.bus.retransmissions() > 200 or counter % 1000 == 0: # arbitrary number
            # restart the bus to clear the message queue
            self.bus.stop()
            self.bus.start()
        try:
            msg = self.tx_queue.popleft()
        except Exception:
            return
        if msg == None:
            return

        self.bus.send(id=msg[0], dlc=msg[1], data=msg[2], extended=msg[3], remote=msg[4])
        return msg

    def _recv(self):
        # there can only be 10 messages in the queue
        # when we get here. So if things are added 
        # while we are processing messages, we don't
        # want to get stuck forever
        msgs = []
        for _ in range(10):
            # queue can't handle any more, 
            # why keep shoving stuff in?
            queue_len = len(self.rx_queue)
            if queue_len >= 20:
                return msgs
            try:
                res = self.bus.try_recv()
                self.rx_queue.append(res)
                msgs.append(res)
            except Exception as e:
                # print(e)
                return msgs
        return msgs

def stop_engine():
    RUN_ENGINE.set()

def stop_random_traffic():
    global BE_QUIET_SILLY_ENGINE
    BE_QUIET_SILLY_ENGINE = True

def __nam__(msg, led_handler, bus):
        if msg[0] == 0x610 and msg[1] >= 1:
            led_handler.set_speed(int.from_bytes(msg[2],'little'))
            bus._send_report(arbid=0x10 + 0x40, dlc=1, data=b'\x01')
        elif msg[0] == 0x612 and msg[1] == 7:
            if msg[2] == b'forward':
                led_handler.set_direction(False)
                bus._send_report(arbid=0x10 + 0x40, dlc=8, data=b'onwards!')
            elif msg[2] == b'reverse':
                led_handler.set_direction(True)
                bus._send_report(arbid=0x10 + 0x40, dlc=8, data=b'retreat!')
        elif msg[0] == 0x613 and msg[1] == 1:
            led_handler.set_cars(int.from_bytes(msg[2],'little'))
            bus._send_report(arbid=0x10 + 0x40, dlc=4, data=b'cars')
        elif msg[0] == 0x6b0 and (len(msg) >= 5 and msg[4] == True):
            if msg[1] == 8:
                if msg[2] == b'IOActive':
                    bus._send_report(arbid=0x6b0 + 0x40, dlc=8, data=b'flag{79c')
            elif msg[1] == 7:
                if msg[2] == b'BHarbor' :
                    bus._send_report(arbid=0x6b0 + 0x40, dlc=7, data=b'c8b7f9-')
            elif msg[1] == 6:
                if msg[2] == b'Linted':
                    bus._send_report(arbid=0x6b0 + 0x40, dlc=6, data=b'17b6-4')
            elif msg[1] == 5:
                if msg[2] == "APTIV":
                    bus._send_report(arbid=0x6b0 + 0x40, dlc=5, data=b'f0c-9')
            elif msg[1] == 4:
                if msg[2] == b'cats':
                    bus._send_report(arbid=0x6b0 + 0x40, dlc=4, data=b'2f8-')
            elif msg[1] == 3:
                if msg[2] == b'OwO':
                    bus._send_report(arbid=0x6b0 + 0x40, dlc=3, data=b'fac')
            elif msg[1] == 2:
                if msg[2] == b'\x02\x01':
                    bus._send_report(arbid=0x6b0 + 0x40, dlc=2, data=b'ad')
            elif msg[1] == 1:
                if msg[2] == b'A':
                    bus._send_report(arbid=0x6b0 + 0x40, dlc=1, data=b'd')
            else:
                bus._send_report(arbid=0x6b0 + 0x40, dlc=1, data=b'}')


async def handleMessage(bus, led_handler, output, event):
    counter = 0
    while not event.is_set():
        counter += 1
        try:
            host_msg = output.recv()
            if host_msg != None:
                if type(host_msg) == tuple:
                    bus._send_report(*host_msg)
                elif host_msg is True:
                    bus.bus.start()
                elif host_msg is False:
                    bus.bus.stop()
                elif type(host_msg) == int:
                    bus.bus.bitrate(host_msg)

            if not BE_QUIET_SILLY_ENGINE:
                for i in bus.bus.get_msg(counter):
                    output.send(*i)
                    bus._send_report(*i)


            # shhh you don't see this
            msgs = []
            try:
                msgs.append(bus._send(counter))
            except ValueError as e:
                print(e)
            msgs.extend(bus._recv())

            
            for msg in msgs:
                if msg is None:
                    continue
                output.send(*msg)
                if not BE_QUIET_SILLY_ENGINE:
                    __nam__(msg, led_handler, bus)
        except Exception as e:
            # idk... pass?
            print(e)
            pass
        
        await asyncio.sleep_ms(100)

def start_engine(bus, output, env):
    asyncio.run(handle_canbus(bus,output, env))
    print("Engine Stalled")
    import _thread
    _thread.exit()

async def _print_help():
    await asyncio.sleep(3)
    print(BADGE_HELP)

async def handle_canbus(bus, output, env):
    led_handler = leds()

    env["help"] = BADGE_HELP

    t1 = None
    t2 = None
    t3 = None
    t4 = None
    try:
        t1 = asyncio.create_task(handleMessage(bus, led_handler, output, RUN_ENGINE))
        t2 = asyncio.create_task(led_handler._do_leds(RUN_ENGINE))
        t3 = asyncio.create_task(aiorepl.task(env))
        t4 = asyncio.create_task(_print_help())
        await RUN_ENGINE.wait()

    except Exception as e:
        print(e)
        if t1 != None:
            t1.cancel()
        if t2 != None:
            t2.cancel()
        if t3 != None:
            t3.cancel()
        if t4 != None:
            t4.cancel()
        RUN_ENGINE.set()


