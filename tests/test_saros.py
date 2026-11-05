"""SAROS installer dependencies and native-SELF input must fail closed."""
import sys,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import gpulab
from ps5_console import Console,Refused
class FTP:
 def __init__(self,reply='226 SELF transfer mode disabled',data=bytes.fromhex('5414f5ee')+b'owner self'):
  self.reply,self.data,self.closed,self.reads=reply,data,False,0
 def sendcmd(self,s):
  assert s=='SELF';return self.reply
 def retrbinary(self,s,callback,blocksize):
  self.reads+=1;callback(self.data)
 def close(self):self.closed=True
class NativeSelf(unittest.TestCase):
 def run_read(self,ftp,cap=100):
  c=Console('unused');normal=object();c.ftp=normal;c._connect=lambda:ftp
  try:return c.read_raw_self('/game/eboot.bin',cap)
  finally:self.assertIs(c.ftp,normal);self.assertTrue(ftp.closed)
 def test_owner_bytes_and_connection_isolation(self):
  f=FTP();self.assertEqual(self.run_read(f),f.data)
 def test_conversion_not_disabled(self):
  f=FTP('226 enabled')
  with self.assertRaises(Refused):self.run_read(f)
  self.assertEqual(f.reads,0)
 def test_reconstructed_elf_rejected(self):
  with self.assertRaises(Refused):self.run_read(FTP(data=b'\x7fELFanything'))
 def test_size_guard(self):
  with self.assertRaises(Refused):self.run_read(FTP(),cap=4)
 def test_manifest_routes_native_input(self):
  class C:
   def read_raw_self(self,*args):return args
  row={'path':'eboot.bin','raw_self':True,'accept':[[12,'sha']]}
  self.assertEqual(gpulab.read_input(C(),'/app0',row),('/app0/eboot.bin',13,None))
 def test_missing_sidecar_blocks_enable(self):
  g={'id':'saros-native-v1','name':'SAROS','files':[]}
  with self.assertRaises(Refused):gpulab.verify_support(None,g)
if __name__=='__main__':unittest.main()
