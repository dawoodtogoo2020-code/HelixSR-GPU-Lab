import os
from pathlib import Path
import subprocess,json,re
r=Path(__file__).resolve().parents[1];deps=Path(os.environ['GPULAB_DEPS']);out=Path(os.environ.get('GPULAB_BUILD_DIR',str(r/'build')));native=deps/'vulkan/tooling/native';sdk=Path(os.environ['PS5_PAYLOAD_SDK']);llvm=Path(os.environ.get('LLVM_BIN','/opt/homebrew/opt/llvm@18/bin'))
for lib,src in [('libSceAgc','agc_canary_link_stub.c'),('libSceAgcDriver','agc_driver_canary_link_stub.c')]:
 subprocess.run([str(llvm/'clang'),'-target','x86_64-sie-ps5','-fvisibility-nodllstorageclass=default','-isysroot',str(sdk),'-isystem',str(sdk/'target/include'),'-fPIC','-O2','-c',str(deps/'vulkan/vendor/ps5/sdk/stubs'/src),'-o',str(out/(lib+'.o'))],check=True)
 subprocess.run([str(llvm/'ld.lld'),'-m','elf_x86_64','--shared','-z','max-page-size=0x4000','-soname',lib+'.prx','-o',str(out/(lib+'.so')),str(out/(lib+'.o'))],check=True)
s=(deps/'vulkan/tools/radv-link.sh').read_text().replace('\\\n',' ');wraps=re.findall(r'--wrap=(\w+)',s);defs=[]
for match in re.finditer(r'for name in ([^;]+); do\s+radv_link_flags\+=\("--(wrap|defsym)=',s):
 for name in match[1].split():
  (wraps if match[2]=='wrap' else defs).append(name)
symbols=subprocess.check_output([str(llvm/'llvm-nm'),'--defined-only',str(deps/'platform-build/libps5platform.a')],text=True)
defs += [name for name in ['localeconv','strcasestr','mkstemp','readlink','link','symlink'] if ' T ps5_'+name+'\n' in symbols];defs=sorted(set(defs));wraps=sorted(set(wraps));local=out/'platform.map';local.write_text('{ local: '+''.join(name+';' for name in defs)+'fstatat; };\n')
flags=[*['--wrap='+name for name in wraps],*['--defsym='+name+'=ps5_'+name for name in defs if name!='fstatat'],'--defsym=fstatat=helix_native_fstatat','--version-script',str(local)]
cmd=[str(llvm/'ld.lld'),'-m','elf_x86_64','-pie','-T',str(deps/'vulkan/tooling/psbc/ps5-pie-unwind.ld'),'-L'+str(native),'-L'+str(sdk/'target/lib'),'--eh-frame-hdr','-z','max-page-size=0x4000','-mllvm','-emulated-tls','--hash-style=gnu',*flags,'--version-script',str(native/'app-symbols.map'),'--exclude-libs=ALL','-e','_start','-o',str(out/'llvm-pie.elf'),*json.loads((out/'objects.json').read_text()),str(out/'libSceAgc.so'),str(out/'libSceAgcDriver.so'),'--start-group',str(sdk/'target/lib/libc++.a'),str(sdk/'target/lib/libc++abi.a'),str(sdk/'target/lib/libunwind.a'),str(deps/'platform-build/libps5platform.a'),str(deps/'compiler-rt/emutls.o'),'--end-group','--as-needed',*map(str,sorted((sdk/'target/lib').glob('*.so')))];result=subprocess.run(cmd);
if result.returncode:raise SystemExit(result.returncode)
subprocess.run([str(deps/'ps5-native-tool'),'link','--in',str(out/'llvm-pie.elf'),'--out',str(out/'eboot.elf'),'--stub-dir',str(sdk/'target/lib'),'--stub',str(out/'libSceAgc.so'),'--stub',str(out/'libSceAgcDriver.so'),'--module-sdk','0x02000009','--companion-sdk','0x08050001','--file-name','eboot.elf'],check=True)
subprocess.run([str(deps/'ps5-native-tool'),'self','--sign','--in',str(out/'eboot.elf'),'--out',str(out/'eboot.bin'),'--magic','0x1D3D154F'],check=True)
