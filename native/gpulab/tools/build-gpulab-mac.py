import os
from pathlib import Path
import subprocess,hashlib,json
r=Path(__file__).resolve().parents[1];deps=Path(os.environ['GPULAB_DEPS']);out=Path(os.environ.get('GPULAB_BUILD_DIR',str(r/'build')));out.mkdir(exist_ok=True)
src=r/'app/source';native=deps/'vulkan/tooling/native';sdk=Path(os.environ['PS5_PAYLOAD_SDK']);llvm=Path(os.environ.get('LLVM_BIN','/opt/homebrew/opt/llvm@18/bin'))
common=['-target','x86_64-sie-ps5','-fvisibility-nodllstorageclass=default','-isysroot',str(sdk),'-isystem',str(sdk/'target/include/c++/v1'),'-isystem',str(sdk/'target/include'),'-I'+str(deps/'sdk/platform/include'),'-fno-stack-protector','-fno-plt','-femulated-tls','-fdenormal-fp-math=ieee','-Dlstat=helix_native_lstat','-ffile-prefix-map='+str(src.parent)+'=gpulab-app','-O2','-Wall','-Wextra','-Werror','-ffunction-sections','-fdata-sections','-I'+str(src)]
objects=[]
for f in sorted(src.glob('*.c'))+[src/'native_main.cpp',src/'display.cpp',native/'app_crt.cpp',native/'app_cpp_runtime.cpp']:
 dest=out/(f.stem+'.o');flags=['-std=c++20','-fno-exceptions','-fno-rtti'] if f.suffix=='.cpp' else ['-std=c11']
 subprocess.run([str(llvm/'clang'),*common,*flags,'-c',str(f),'-o',str(dest)],check=True);objects.append(dest)
print('Compiled',len(objects),'native application objects')
(out/'objects.json').write_text(json.dumps([str(o) for o in objects],indent=2)+'\n')
