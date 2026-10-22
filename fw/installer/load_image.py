#!/usr/bin/env python3
"""DEV1.4 專用 loader 測試；會 reset MCU、寫 U11／內建 App，僅核准後使用。"""
import argparse
import hashlib
from pathlib import Path
import re
import struct
import time
import zlib

def frame(op,index=0,payload=b''):
    h=struct.pack('<4sBBHII',b'LDR1',op,0,len(payload),index,zlib.crc32(payload))
    return h+struct.pack('<I',zlib.crc32(h))+payload

def board(reset=False):
    from pyocd.core.helpers import ConnectHelper
    with ConnectHelper.session_with_chosen_probe(target_override='cortex_m',
            options={'connect_mode':'attach','frequency':1000000,'resume_on_disconnect':True,'reset_type':'sw'}) as s:
        t=s.board.target;b=bytes(t.read_memory_block8(0x08001000,4096))
        if reset:
            t.write32(0xe000edfc,0);t.reset()
        return hashlib.sha256(b).hexdigest()

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('image',type=Path);p.add_argument('--port',default='/dev/cu.usbmodem2024_2425_62851')
    p.add_argument('--allow-write',action='store_true');p.add_argument('--test-resets',action='store_true')
    p.add_argument('--expect-reject',type=int);p.add_argument('--version',default='0.8.4-dev')
    args=p.parse_args()
    if not args.allow_write:p.error('explicit --allow-write required')
    b=args.image.read_bytes()
    if not 560<=len(b)<=0x1c0000:p.error('image outside slot profile')
    import serial
    def emit(text):print(text,flush=True)
    before=board();emit('BINFO BEFORE '+before)
    if before!='ad7facb2586fc6e966c004d7d1d16b024f5805ff7cb47c7a85dabd8b48892ca7':
        raise RuntimeError('unexpected B-info baseline; stop without reset/write')
    with serial.Serial(args.port,115200,timeout=0.1,write_timeout=3,exclusive=True) as uart:
        uart.reset_input_buffer();assert board(True)==before
        def wait(match,timeout=120):
            deadline=time.monotonic()+timeout;pending=bytearray()
            while time.monotonic()<deadline:
                line=uart.read_until(b'\n',2048)
                if not line:continue
                pending.extend(line)
                if len(pending)>4096:raise RuntimeError('oversize UART log line')
                if not pending.endswith(b'\n'):continue
                text=pending.decode('ascii',errors='replace').strip();pending.clear();emit(text)
                if 'STOP' in text or 'HardFault' in text:raise RuntimeError(text)
                if match in text:return text
            raise TimeoutError(match)
        wait('LOADER WINDOW',20);uart.write(b'ET100-LOAD\n');uart.flush();wait('LOADER READY')
        def send(op,index=0,payload=b''):
            uart.write(frame(op,index,payload));uart.flush()
            text=wait(f'ACK {op} {index} ',120)
            return int(re.search(r'rc=(-?\d+)',text)[1])
        sid=int.from_bytes(hashlib.sha256(b).digest()[:8],'little') or 1
        begin=struct.pack('<QI',sid,len(b))+hashlib.sha256(b).digest()+struct.pack('<I',7 if args.test_resets else 0)
        assert send(1,payload=begin)==0
        for index,off in enumerate(range(0,len(b),4096)):
            assert send(2,index,b[off:off+4096])==0
        rc=send(3)
        emit(f'FINISH rc={rc} expected={args.expect_reject if args.expect_reject is not None else 0}')
        resets=[]
        if args.expect_reject is not None:
            assert rc==args.expect_reject;assert send(4)==0
        else:
            assert rc==0
            if args.test_resets:
                for phase in (1,2,4):
                    text=wait(f'INST TEST_RESET phase={phase}',180);resets.append(phase)
            text=wait('INST COMPLETE',180);assert 'rc=0 kind=2' in text
        wait('E0 JUMP',30)
        uart.baudrate=9600;time.sleep(4);uart.reset_input_buffer()
        uart.write(b'BLEGW$GCHK;INST\r\n');uart.flush()
        text=wait('AT$PD=BLEGW$GCHK;',15);assert args.version in text
        if args.test_resets:assert resets==[1,2,4]
    after=board();emit('BINFO AFTER '+after);assert after==before
    emit('PASS loader/install/reject + GCHK + B-info unchanged')

if __name__=='__main__':main()
