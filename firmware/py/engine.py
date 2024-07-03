import time
import collections

from leds import leds

RUN_ENGINE = True
BE_QUIET_SILLY_ENGINE = False

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
    global RUN_ENGINE
    RUN_ENGINE = False

def stop_random_traffic():
    global BE_QUIET_SILLY_ENGINE
    BE_QUIET_SILLY_ENGINE = True

def __nam__(msg, led_handler, bus):
        if msg[0] == 0x10 and msg[1] >= 1:
            led_handler.speed = int.from_bytes(msg[2],'little')
            bus._send_report(arbid=0x10 + 0x40, dlc=1, data=b'\x01')
        elif msg[0] == 0x12 and msg[1] == 7:
            if msg[2] == b'forward':
                led_handler.reverse = False
                bus._send_report(arbid=0x10 + 0x40, dlc=8, data=b'onwards!')
            elif msg[2] == b'reverse':
                led_handler.reverse = True
                bus._send_report(arbid=0x10 + 0x40, dlc=8, data=b'retreat!')
        elif msg[0] == 0x100 and (len(msg) >= 5 and msg[4] == True):
            if msg[1] == 8:
                if msg[2] == b'DC31CHV\xa9':
                    bus._send_report(arbid=0x100 + 0x40, dlc=8, data=b'flag{\xd7\n\xe6')
            elif msg[1] == 7:
                if msg[2] == b'\xe0\xb2\xa0_\xe0\xb2\xa0':
                    bus._send_report(arbid=0x100 + 0x40, dlc=7, data=b'n0*;ozC')
            elif msg[1] == 6:
                if msg[2] == b'linted':
                    bus._send_report(arbid=0x100 + 0x40, dlc=6, data=b'\k!X"U')
            elif msg[1] == 5:
                if msg[2] == str(15 << 175 + 3**3 // 10 <<3)[:5].encode():
                    bus._send_report(arbid=0x100 + 0x40, dlc=5, data=b')du+4')
            elif msg[1] == 4:
                if msg[2] == b'\xa5100':
                    bus._send_report(arbid=0x100 + 0x40, dlc=4, data=b'6e0T')
            elif msg[1] == 3:
                if msg[2] == b'\xec\x9b\x83':
                    bus._send_report(arbid=0x100 + 0x40, dlc=3, data=b'Emd')
            elif msg[1] == 2:
                if msg[2] == b'\xd9\xbc':
                    bus._send_report(arbid=0x100 + 0x40, dlc=2, data=b'fl')
            elif msg[1] == 1:
                if msg[2] == b'\xbe':
                    bus._send_report(arbid=0x100 + 0x40, dlc=1, data=b'a')
            else:
                bus._send_report(arbid=0x100 + 0x40, dlc=1, data=b'}')

def handle_canbus(bus,output):
    led_handler = leds()
    led_handler.speed = 10

    counter = 0
    while (RUN_ENGINE):
        try:
            counter += 1
            # check to see if there are messages from the host
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
            msgs.append(bus._send(counter))
            msgs.extend(bus._recv())

            
            for msg in msgs:
                if msg is None:
                    continue
                output.send(*msg)
                if not BE_QUIET_SILLY_ENGINE:
                    __nam__(msg, led_handler, bus)

            led_handler.do_loop_step()
            time.sleep(.01)
        except Exception as e:
            print(e)
            global RUN_ENGINE
            RUN_ENGINE = False
    print("Engine Stalled")
    import _thread
    _thread.exit()


