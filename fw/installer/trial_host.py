#!/usr/bin/env python3
"""核准後才執行：精簡 trial/rollback EVB runner，不是正式 OTA transport。"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import time
import zlib
from load_image import board,frame

def state():
    from pyocd.core.helpers import ConnectHelper
    with ConnectHelper.session_with_chosen_probe(target_override='cortex_m',
            options={'connect_mode':'attach','frequency':1000000,'resume_on_disconnect':True}) as s:
        t=s.board.target;records=[]
        for off in (0x081fc000,0x081fe000):
            raw=bytes(t.read_memory_block8(off,100));magic=struct.unpack_from('<I',raw)[0]
            if magic==0x32545342:
                m,seq,kind,n,mask,h,cn,ch,last,crc,commit=struct.unpack('<5I32sI32sIII',raw)
                good=zlib.crc32(raw[:92])==crc
            elif magic==0x31545342:
                m,seq,kind,n,mask,h,crc,commit=struct.unpack('<5I32sII',raw[:60])
                cn,ch,last=n,h,0;good=zlib.crc32(raw[:52])==crc
            else:continue
            if good and commit==0x54494d43:
                records.append(dict(seq=seq,kind=kind,length=n,mask=mask,hash=h.hex(),
                    confirmed_length=cn,confirmed_hash=ch.hex(),last_update=last))
        assert records
        result=max(records,key=lambda r:r['seq'])
        assert t.read32(0xe000ed28)==t.read32(0xe000ed2c)==0
        return result

def verify_image(expected):
    from pyocd.core.helpers import ConnectHelper
    with ConnectHelper.session_with_chosen_probe(target_override='cortex_m',
            options={'connect_mode':'attach','frequency':1000000,'resume_on_disconnect':True}) as s:
        b=bytes(s.board.target.read_memory_block8(0x08020000,expected['confirmed_length']))
        assert hashlib.sha256(b).hexdigest()==expected['confirmed_hash']
    print('READBACK confirmed hash MATCH',flush=True)

def boot_version(version,rollback=False):
    import serial
    with serial.Serial('/dev/cu.usbmodem2024_2425_62851',115200,timeout=.1,exclusive=True) as u:
        board(True)
        wait(u,'E0 JUMP',120)
        gchk(u,version)
    return state()

def wait(u,match,seconds=120):
    end=time.monotonic()+seconds;pending=bytearray()
    while time.monotonic()<end:
        pending.extend(u.read_until(b'\n',2048))
        if not pending.endswith(b'\n'):continue
        text=pending.decode('ascii',errors='replace').strip();pending.clear();print(text,flush=True)
        if 'INST STOP' in text or 'FAILED STOP' in text:raise RuntimeError(text)
        if match in text:return text
    raise TimeoutError(match)

def gchk(u,version):
    u.baudrate=9600;time.sleep(4);u.reset_input_buffer();u.write(b'BLEGW$GCHK;TRIA\r\n');u.flush()
    assert version in wait(u,'AT$PD=BLEGW$GCHK;',15)

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('image',type=Path)
    p.add_argument('--scenario',choices=['setup','normal','backup-reset','no-confirm','hang'],required=True)
    p.add_argument('--version',required=True);p.add_argument('--base-version',default='0.8.4-dev')
    p.add_argument('--allow-write',action='store_true');a=p.parse_args()
    if not a.allow_write:p.error('--allow-write required')
    before=state();print('BEFORE',json.dumps(before),flush=True);assert before['kind']==2
    binfo=board();assert binfo=='ad7facb2586fc6e966c004d7d1d16b024f5805ff7cb47c7a85dabd8b48892ca7'
    b=a.image.read_bytes();assert 560<=len(b)<=0x1c0000;sha=hashlib.sha256(b).digest()
    import serial
    with serial.Serial('/dev/cu.usbmodem2024_2425_62851',115200,timeout=.1,write_timeout=3,exclusive=True) as u:
        u.reset_input_buffer();board(True);wait(u,'LOADER WINDOW',15)
        u.write(b'ET100-LOAD\n');u.flush();wait(u,'LOADER READY')
        def send(op,index=0,data=b''):
            u.write(frame(op,index,data));u.flush()
            assert 'rc=0' in wait(u,f'ACK {op} {index} ')
        sid=int.from_bytes(sha[:8],'little') or 1
        send(1,data=struct.pack('<QI',sid,len(b))+sha+struct.pack('<I',8 if a.scenario=='backup-reset' else 0))
        for i,off in enumerate(range(0,len(b),4096)):send(2,i,b[off:off+4096])
        send(3)
        if a.scenario=='backup-reset':wait(u,'INST TEST_RESET phase=8',120)
        assert 'kind=4' in wait(u,'INST COMPLETE',180)
        wait(u,'TRIAL WDT 60s');wait(u,'E0 JUMP')
        if a.scenario=='hang':
            wait(u,'TRIAL unconfirmed: ROLLBACK_REQUIRED',100)
            assert 'kind=2' in wait(u,'INST COMPLETE',180)
            wait(u,'E0 JUMP');gchk(u,a.base_version)
        else:gchk(u,a.version)
    if a.scenario=='no-confirm':
        time.sleep(32);trial=state();print('UNCONFIRMED',json.dumps(trial),flush=True);assert trial['kind']==4
        result=boot_version(a.base_version)
    elif a.scenario=='hang':result=state()
    else:
        time.sleep(32);result=state()
        if result['kind']!=2:
            time.sleep(8);result=state()
        assert result['kind']==2 and result['confirmed_hash']==sha.hex() and result['last_update']==0
        if a.scenario=='normal':result=boot_version(a.version)
    print('AFTER',json.dumps(result),flush=True)
    assert result['kind']==2
    if a.scenario in ('hang','no-confirm'):
        assert result['confirmed_hash']==before['confirmed_hash'] and result['last_update']==1
    else:assert result['confirmed_hash']==sha.hex()
    verify_image(result);assert board()==binfo
    print('PASS',a.scenario,'App hash + state + GCHK + B-info unchanged',flush=True)

if __name__=='__main__':main()
