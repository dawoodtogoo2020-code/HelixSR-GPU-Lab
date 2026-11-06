"""Reproduce the SAROS overlay with a bounded hook and native translucency composition."""
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
 f.seek(0xb0b5db);assert f.read(7)==bytes.fromhex('8a8c2410010000')
 f.seek(0xb0b744);assert f.read(4)==bytes.fromhex('8a4c2440')
 f.seek(0x6d1c900);assert not any(f.read(10368))
assert not out.exists()
if sys.platform=='darwin':
 sp.run(['cp','-c',stock,out],check=True)
else:
 shutil.copyfile(stock,out)
with out.open('r+b') as f:
 f.seek(0x390);f.write(struct.pack('<Q',0x544610))
 f.seek(0xb0baa6);f.write(b'\xe8'+struct.pack('<i',0x6ce5780-(0xad4926+5)))
 # The original compositor at RVA 0xadf6de may leave colour composition
 # to TSR when its flag is set. HelixSR has no such composition dispatch.
 # Set only CL to zero at the existing flag load (RVA 0xad445b), preserving
 # the rest of RCX and the native colour/modulate/depth-aware compositor.
 f.seek(0xb0b5db);f.write(bytes.fromhex('b1009090909090'))
 # Keep composed input in the same FP16 RGBA format as the neural input.
 # The native non-alpha route otherwise selects R11G11B10 at this load.
 f.seek(0xb0b744);f.write(bytes.fromhex('b10a9090'))
 f.seek(0x6d1c900);f.write(data);f.flush();os.fsync(f.fileno())
print(json.dumps({'candidate_sha256':sha(out),'stub_sha256':hashlib.sha256(data).hexdigest(),'stub_bytes':len(data)}))
