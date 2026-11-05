#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Stage a private unified-app candidate using the verified public beta data.

No game inputs, NVIDIA inputs, my-ps5 state or local build directories are copied.
The output is a host-validation candidate, not a published or qualified release.
"""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path, PurePosixPath
import shutil
import subprocess
import zipfile

ROOT = Path(__file__).resolve().parents[1]
BETA_SHA = 'b292262570ccba6564c1893e7ce3c33eafe570257b7a46472b75b1208db1e058'
BETA_SOURCE_SHA = '3798d63f88d5ac8d1ba9e9fabe9a8a840bc308c6170a0c64e8e461c50e0fe028'
NAME = 'HelixSR-GPU-Lab-migration-candidate-r2'
RUNTIME_SELF_SHA = 'e6ff45d16adf687855cc3b33b0c8a4132b6504360b221e0a34c7e99fb3ba0036'
RUNTIME_ELF_SHA = '8ee6e124993e1af26420cb455890fd002f5d6c7e78883c860ce45734e7d002bb'
RUNTIME_FTP_SHA = '7e82ce9a4259d0db815e7e734ed49afd7bd4aaf33beaefa968ba6d1ff8a8b315'

def digest(raw):
    return hashlib.sha256(raw).hexdigest()

def normalized_elf(raw):
    if raw[:6] != b'\x7fELF\x02\x01' or len(raw) < 64:
        raise ValueError('Expected a complete ELF64 little-endian image')
    clean = bytearray(raw)
    clean[40:48] = bytes(8)
    clean[58:64] = bytes(6)
    return dict(bytes=len(raw), sha256=digest(clean),
                zero_ranges=[[40,48],[58,64]], max_zero_tail=75)

def source_files():
    raw = subprocess.check_output(['git', 'ls-files', '--cached', '--others', '--exclude-standard', '-z'], cwd=ROOT)
    paths = sorted({p for p in raw.decode().split('\0') if p})
    for p in paths:
        if any(x in {'my-ps5', 'build', 'build-repeat', '__pycache__', '.git'} for x in Path(p).parts):
            raise ValueError('Unexpected source path: ' + p)
    return paths

def build(args):
    archive = Path(args.beta_zip)
    if digest(archive.read_bytes()) != BETA_SHA:
        raise ValueError('Public beta ZIP hash mismatch')
    subprocess.run([__import__('sys').executable, str(ROOT/'tools/check_public_tree.py')], check=True)
    out = Path(args.output).resolve()
    stage = out/NAME
    if stage.exists():
        raise ValueError('Use a new staging directory; refusing to mix candidates')
    stage.mkdir(parents=True, exist_ok=True)
    spec = importlib.util.spec_from_file_location('self_image', ROOT/'native/gpulab/app/tools/self_image.py')
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    app = ROOT/'native/gpulab/app/build'
    payload = ROOT/'native/gpulab/payloads/build'
    self_data = (app/'dist/PPSA99272/eboot.bin').read_bytes()
    installed, _ = module.installed(self_data, (app/'eboot.elf').stat().st_size)
    runtime_generator_elf = Path(args.runtime_elf).read_bytes()
    if digest(runtime_generator_elf) != RUNTIME_ELF_SHA:
        raise ValueError('Runtime ELF must match the public generator reproduction')
    with zipfile.ZipFile(archive) as z:
        for item in z.infolist():
            parts = PurePosixPath(item.filename).parts
            if not parts or parts[0] != 'HelixSR-GPU-Lab-v0.2.0-beta' or '..' in parts:
                raise ValueError('Unexpected archive entry: '+item.filename)
            if item.is_dir() or len(parts)<2:
                continue
            target = stage.joinpath(*parts[1:])
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(z.read(item))
    for model in ('ps5', 'ps5-pro'):
        folder = stage/'console'/model
        for src in (app/'dist/PPSA99272').rglob('*'):
            if src.is_file():
                dst = folder/'PPSA99272'/src.relative_to(app/'dist/PPSA99272')
                dst.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(src, dst)
        for name in ('helixsr-launch.elf', 'helixsr-inventory.elf'):
            shutil.copyfile(payload/name, folder/'payloads'/name)
        path = ROOT/'console'/model/'console.json'
        manifest = json.loads(path.read_text())
        for row in manifest['files']:
            data = (stage/row['bundle']).read_bytes()
            row['sha256'] = digest(data)
            row['accept'] = [[len(data), digest(data)]]
            if row['remote'].endswith('/PPSA99272/eboot.bin'):
                row['accept'].append([len(installed), digest(installed)])
                row['normalized_elf'] = normalized_elf(installed)
            elif row['remote'].endswith('/PPSA99272/sce_module/libc.prx'):
                if digest(data) != RUNTIME_SELF_SHA:
                    raise ValueError('Runtime SELF does not match the pinned generator output')
                # FTP reconstructs only SELF-backed segments. The general-purpose
                # host extractor additionally restores an unbacked 26-byte trailer.
                runtime_elf, _ = module.installed(data, len(runtime_generator_elf))
                if digest(runtime_elf) != RUNTIME_FTP_SHA:
                    raise ValueError('Runtime FTP reconstruction differs from the verified representation')
                row['accept'].append([len(runtime_elf), digest(runtime_elf)])
                row['normalized_elf'] = normalized_elf(runtime_elf)
        path.write_text(json.dumps(manifest, indent=1)+'\n')
    paths = source_files()
    for name in paths:
        dst = stage/name
        dst.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(ROOT/name, dst)
    for model in ('ps5','ps5-pro'):
        for row in json.loads((stage/'console'/model/'console.json').read_text())['files']:
            assert digest((stage/row['bundle']).read_bytes())==row['sha256']
    for path in (stage/'games').glob('*/game.json'):
        for row in json.loads(path.read_text())['files']:
            if 'bundle' in row:
                assert digest((stage/row['bundle']).read_bytes())==row['sha256']
    assert (stage/'console/ps5/PPSA99272/eboot.bin').read_bytes()==(stage/'console/ps5-pro/PPSA99272/eboot.bin').read_bytes()
    receipt = dict(status='host-validation candidate; not released or console-tested',
                   base_commit='a967f4c9b8985300b768ebcead6c5b4d82fb4697', beta_zip_sha256=BETA_SHA,
                   app_self=dict(bytes=len(self_data),sha256=digest(self_data)),
                   app_installed_elf=dict(bytes=len(installed),sha256=digest(installed)),
                   app_normalized_elf=normalized_elf(installed),
                   runtime_self_sha256=RUNTIME_SELF_SHA,
                   runtime_generator_elf_sha256=RUNTIME_ELF_SHA,
                   runtime_elf=dict(bytes=len(runtime_elf),sha256=digest(runtime_elf)),
                   runtime_normalized_elf=normalized_elf(runtime_elf),
                   files={str(p.relative_to(stage)).replace('\\','/'):
                          dict(bytes=p.stat().st_size,sha256=digest(p.read_bytes())) for p in sorted(stage.rglob('*')) if p.is_file()})
    (out/'candidate-manifest.json').write_text(json.dumps(receipt,indent=2)+'\n')
    if args.zip:
        if not args.beta_source_zip or digest(Path(args.beta_source_zip).read_bytes()) != BETA_SOURCE_SHA:
            raise ValueError('Provide the verified beta corresponding-source ZIP for unchanged binaries')
        for suffix, selected in (('',sorted(stage.rglob('*'))),('-source',[stage/p for p in paths])):
            target=out/(NAME+suffix+'.zip')
            with zipfile.ZipFile(target,'w',compression=zipfile.ZIP_DEFLATED,compresslevel=9) as z:
                for p in selected:
                    if not p.is_file(): continue
                    info=zipfile.ZipInfo(NAME+suffix+'/'+str(p.relative_to(stage)).replace('\\','/'),date_time=(2026,11,2,0,0,0))
                    info.compress_type=zipfile.ZIP_DEFLATED
                    info.external_attr=0o100644<<16
                    z.writestr(info,p.read_bytes())
                if suffix:
                    # Corresponding source for the unchanged beta SOTC hook and prior native baseline.
                    info=zipfile.ZipInfo(NAME+suffix+'/baseline-native-source.zip',date_time=(2026,11,2,0,0,0))
                    info.compress_type=zipfile.ZIP_DEFLATED
                    info.external_attr=0o100644<<16
                    z.writestr(info,Path(args.beta_source_zip).read_bytes())
            print(target.name,target.stat().st_size,digest(target.read_bytes()))
    print('Verified unified app staging:',stage)

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--beta-zip',required=True)
    p.add_argument('--output',required=True)
    p.add_argument('--zip',action='store_true')
    p.add_argument('--beta-source-zip')
    p.add_argument('--runtime-elf', required=True,
                   help='ELF extracted from the pinned public runtime SELF; checked against the generator digest')
    build(p.parse_args())
