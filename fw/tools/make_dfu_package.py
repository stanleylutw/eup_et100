#!/usr/bin/env python3
"""ET100 原型 DFU 打包；不驗簽章，不能用於量產。"""
import argparse
import hashlib
import importlib.metadata
import json
from pathlib import Path
import re
import shutil
import struct
import subprocess
import sys
import zipfile
import zlib

MAGIC=b'EUPFWV1\0'
ROOT=Path(__file__).resolve().parents[1]

def patch_metadata(blob):
    data=bytearray(blob);offset=data.find(MAGIC)
    if offset<0 or offset+286>len(data) or data.find(MAGIC,offset+1)>=0:
        raise ValueError('metadata missing/truncated/ambiguous')
    if struct.unpack_from('<H',data,offset+8)[0]!=1:raise ValueError('schema')
    crc=zlib.crc32(data[offset:offset+282]) & 0xffffffff
    struct.pack_into('<I',data,offset+282,crc)
    version=bytes(data[offset+54:offset+86]).split(b'\0')[0].decode('ascii')
    return data,version

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('binary',type=Path)
    args=p.parse_args()
    expected=(ROOT/'tools/requirements-imgtool.txt').read_text().strip()
    if Path(sys.prefix).resolve()!=ROOT/'.venv' or 'imgtool=='+importlib.metadata.version('imgtool')!=expected:
        raise SystemExit('Use fw/.venv/bin/python; install -r fw/tools/requirements-imgtool.txt')
    data,version=patch_metadata(args.binary.read_bytes())
    numeric=re.fullmatch(r'(\d+\.\d+\.\d+)(?:-dev)?',version)
    if not numeric:raise ValueError('unsupported prototype version '+version)
    args.binary.write_bytes(data)
    out=args.binary.parent;image=out/'et100.signed.bin'
    subprocess.run([str(ROOT/'.venv/bin/imgtool'),'sign','--header-size','0x200','--pad-header',
                    '--slot-size','0x1c0000','--version',numeric[1],'--align','4','--sha','256',
                    str(args.binary),str(image)],check=True)
    b=image.read_bytes();hdr,size=struct.unpack_from('<H',b,8)[0],struct.unpack_from('<I',b,12)[0]
    assert hdr==512 and b[512:512+size]==data and len(b)==hdr+size+40<=0x1c0000
    assert hashlib.sha256(b[:hdr+size]).digest()==b[-32:]
    shutil.copyfile(image,out/'App_mcuboot.bin')
    stamp=int(args.binary.stat().st_mtime)
    manifest={'format-version':1,'time':stamp,'files':[{'type':'application','board':'dev14_fr3068ep_c',
        'soc':'fr3068e_c','load_address':0x08020200,'image_index':'0','slot_index_primary':'1',
        'slot_index_secondary':'2','version_MCUBOOT':numeric[1]+'+0','size':len(b),
        'file':image.name,'modtime':stamp}],'name':'et100'}
    (out/'manifest.json').write_text(json.dumps(manifest,indent=4)+'\n')
    with zipfile.ZipFile(out/'dfu_application.zip','w',zipfile.ZIP_DEFLATED) as z:
        z.write(image,image.name);z.write(out/'manifest.json','manifest.json')
    (out/'Project_burn.bin').unlink(missing_ok=True)
    print('PASS unsigned prototype DFU:',out/'dfu_application.zip','version='+version)

if __name__=='__main__':main()
