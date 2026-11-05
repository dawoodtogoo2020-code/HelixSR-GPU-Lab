import os
from pathlib import Path
import subprocess as sp,hashlib,json
r=Path(__file__).resolve().parents[1];src=r/'payloads';out=Path(os.environ.get('GPULAB_BUILD_DIR',str(r/'build')))/'payloads';out.mkdir(parents=True,exist_ok=True)
sdk=Path(os.environ['PS5_PAYLOAD_SDK']);llvm=Path(os.environ.get('LLVM_BIN','/opt/homebrew/opt/llvm@18/bin'))
for name,sources in [('helixsr-launch',['helixsr_launch.c','scan_engine.c','helixsr_swap.c']),('helixsr-inventory',['helixsr_inventory.c'])]:
 objs=[]
 for f in sources:
  o=out/(Path(f).stem+'.o');sp.run([str(llvm/'clang'),'--target=x86_64-sie-ps5','-isysroot',str(sdk),'-isystem',str(sdk/'target/include'),'-fvisibility-nodllstorageclass=default','-fno-stack-protector','-fno-plt','-femulated-tls','-std=gnu11','-O2','-Wall','-Wextra','-Werror','-c',str(src/f),'-o',str(o)],check=True);objs.append(o)
 sp.run([str(llvm/'ld.lld'),'-m','elf_x86_64','-pie','-T'+str(sdk/'ldscripts/elf_x86_64.x'),'--eh-frame-hdr','--gc-sections','-z','max-page-size=0x4000','--hash-style=gnu','-L'+str(sdk/'target/lib'),'-o',str(out/(name+'.elf')),*map(str,objs),str(sdk/'target/lib/crt1.o'),'-lc','-lkernel_web','-lSceLibcInternal','-lSceNet','-lSceSystemService','-lSceUserService'],check=True)
print(json.dumps({p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in out.glob('*.elf')},indent=2))
