"""確認 ET100 打包與現有 NCS zip schema 相容；其他 repo 僅讀取。"""
import json
from pathlib import Path
import struct
import subprocess
import sys
import zipfile
import zlib
ROOT=Path(__file__).resolve().parents[2]
out=ROOT/'fw/build_serial/output'
with zipfile.ZipFile(out/'dfu_application.zip') as z:
    m=json.loads(z.read('manifest.json'));b=z.read('et100.signed.bin')
    assert set(z.namelist())=={'manifest.json','et100.signed.bin'}
with zipfile.ZipFile(ROOT.parent/'eup_gateway/build_ms88sf2_app_ota/dfu_application.zip') as z:
    reference=json.loads(z.read('manifest.json'))
assert set(m)==set(reference) and set(m['files'][0])==set(reference['files'][0])
assert m['files'][0]['size']==len(b) and m['files'][0]['load_address']==0x08020200
i=b.index(b'EUPFWV1\0');crc=struct.unpack_from('<I',b,i+282)[0]
assert crc and crc==zlib.crc32(b[i:i+282])
scan=subprocess.run([sys.executable,str(ROOT.parent/'eup_aiot/tools/eup_fw_info_scan.py'),
                     str(out/'et100.signed.bin'),'--pretty'],check=True,capture_output=True,text=True)
print(scan.stdout)
assert 'EUP_ET100' in scan.stdout and 'ET100_EVB_DEV1.4' in scan.stdout
print('PASS NCS zip/manifest key sets, metadata CRC and read-only existing scanner')
