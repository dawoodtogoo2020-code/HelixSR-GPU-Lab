#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Hash the actual local native build inputs without disclosing local paths."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import os

def readable(p):
    try:
        p.stat()
        return p
    except OSError:
        # Resolve Linux symlinks through WSL, then read ordinary drive files
        # directly. Do not change or copy the dependency tree.
        if os.name=='nt' and p.drive.endswith(':'):
            linux='/mnt/'+p.drive[0].lower()+'/'+ '/'.join(p.parts[1:])
            target=subprocess.check_output(['wsl','-d','Ubuntu','--','readlink','-f',linux],text=True).strip()
            if not target.startswith('/mnt/') or len(target)<8 or target[6]!='/':raise
            q=Path(target[5].upper()+':/'+target[7:])
            q.stat()
            return q
        raise

def info(p):
    p=readable(p)
    with p.open('rb') as f:
        return dict(bytes=p.stat().st_size, sha256=hashlib.file_digest(f,'sha256').hexdigest())

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--vulkan-root',required=True)
    p.add_argument('--compiler',required=True)
    p.add_argument('--compiler-driver',required=True)
    p.add_argument('--compiler-version',help='Recorded version, for cross-host inspection of the compiler executable')
    p.add_argument('--builtins',required=True)
    p.add_argument('--output',required=True)
    a=p.parse_args();root=Path(a.vulkan_root);sdk=root/'.deps/native/ps5-payload-sdk'
    files={}
    for name in ('tooling/prospero-clang18','tools/radv-link.sh','tools/setup-native-dependencies.sh',
                 'tooling/native/app_crt.cpp','tooling/native/app_cpp_runtime.cpp','tooling/native/app-symbols.map',
                 'tooling/psbc/ps5-pie-unwind.ld','build/host/ps5-native-tool','runtime/libc.prx',
                 'vendor/ps5/sdk/stubs/agc_canary_link_stub.c','vendor/ps5/sdk/stubs/agc_driver_canary_link_stub.c'):
        files['PS5_Vulkan/'+name]=info(root/name)
    for path in sorted((sdk/'target/lib').glob('*')):
        if path.is_file():files['SDK/target/lib/'+path.name]=info(path)
    for name in ('prospero-clang','prospero-lld','llvm-ar'):
        files['SDK/bin/'+name]=info(sdk/'bin'/name)
    headers=hashlib.sha256();count=0
    for path in sorted((sdk/'target/include').rglob('*')):
        if readable(path).is_file():
            row=info(path);headers.update((str(path.relative_to(sdk)).replace('\\','/')+'\0'+row['sha256']+'\n').encode());count+=1
    receipt=dict(format='GPULAB_BUILD_INPUTS_1',
        ps5_vulkan_commit=subprocess.check_output(['git','-C',str(root),'rev-parse','HEAD'],text=True).strip(),
        sdk_revision=(sdk/'.ps5-sdk-revision').read_text().strip(),
        compiler_version=a.compiler_version or subprocess.check_output([a.compiler,'--version'],text=True).splitlines()[0],
        compiler=info(Path(a.compiler)),compiler_driver=info(Path(a.compiler_driver)),builtins=info(Path(a.builtins)),
        sdk_header_count=count,sdk_headers_tree_sha256=headers.hexdigest(),inputs=files,
        limits='Exact local inputs. Dependency bootstrap on an independent machine has not been reproduced.')
    Path(a.output).write_text(json.dumps(receipt,indent=2)+'\n')
    print('Recorded native dependencies:',len(files),'files and',count,'headers')

if __name__=='__main__':main()
