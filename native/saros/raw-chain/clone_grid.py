"""Build the private copy-derived graph emitter with a second hook to publish validated private group counts."""
import argparse
import hashlib
import subprocess
from pathlib import Path
import sys
sys.dont_write_bytecode=True
HERE=Path(__file__).resolve().parent
sys.path.insert(0,str(HERE.parent/'clear-trial'))
from build_graph_clone import build, source, START

def fresh(elf,folder,llvm):
    original=source(elf)
    point=0x18cc197
    raw=bytes.fromhex('c783e001000001000000')
    if original[point-START:point-START+10]!=raw or original.count(raw)!=1:
        raise ValueError("unmatched stock z assignment")
    build(elf,folder,llvm)
    asm=folder/'graph_clone.S'
    needle='.byte '+','.join(hex(x) for x in raw)+'\n'
    txt=asm.read_text()
    if txt.count(needle)!=1:
        raise ValueError("invalid private instruction count")
    asm.write_text(txt.replace(needle,'call hx_raw_grid\n.fill 5,1,0x90\n'))
    subprocess.run([llvm/'clang','--target=x86_64-unknown-none','-c',asm,'-o',folder/'graph_clone.o'],check=True)
    return hashlib.sha256((folder/'graph_clone.o').read_bytes()).hexdigest()

if __name__=='__main__':
    a=argparse.ArgumentParser(description=__doc__)
    a.add_argument('owner_elf',type=Path);a.add_argument('private_build_dir',type=Path)
    a.add_argument('--llvm',type=Path,default=Path('/opt/homebrew/opt/llvm@18/bin'))
    q=a.parse_args();print(fresh(q.owner_elf,q.private_build_dir,q.llvm))
