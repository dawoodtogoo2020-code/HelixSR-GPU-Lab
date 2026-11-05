"""Reproduce the already-qualified three-range SAROS overlay with a supplied bounded hook."""
from pathlib import Path
import hashlib,json,os,struct,subprocess as sp,sys,shutil
import argparse
ap=argparse.ArgumentParser(description=__doc__)
ap.add_argument('build',type=Path);ap.add_argument('--stock',type=Path,required=True)
a=ap.parse_args();stock=a.stock.resolve();build=a.build.resolve();out=build/'candidate.self'

def sha(p):
 with p.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
assert sha(stock)=='18a27d890ff5089ac8b0bc4e8d057b32a46271f5a3f1958c193e7e4aa3924cb3'
data=(build/'probe.bin').read_bytes();assert 0<len(data)<=10368
with stock.open('rb') as f:
 f.seek(0x390);assert f.read(8)==struct.pack('<Q',0x544601)
 f.seek(0xb0baa6);assert f.read(5)==bytes.fromhex('e8e004ebff')
 f.seek(0x6d1c900);assert not any(f.read(10368))
assert not out.exists()
if sys.platform=='darwin':
 sp.run(['cp','-c',stock,out],check=True)
else:
 shutil.copyfile(stock,out)
with out.open('r+b') as f:
 f.seek(0x390);f.write(struct.pack('<Q',0x544610))
 f.seek(0xb0baa6);f.write(b'\xe8'+struct.pack('<i',0x6ce5780-(0xad4926+5)))
 f.seek(0x6d1c900);f.write(data);f.flush();os.fsync(f.fileno())
print(json.dumps({'candidate_sha256':sha(out),'stub_sha256':hashlib.sha256(data).hexdigest(),'stub_bytes':len(data)}))
