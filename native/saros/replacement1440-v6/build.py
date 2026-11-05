"""Build the project’s fresh projection caller and size receipt. Does not install or start the game."""
import argparse,hashlib,json,re,sys,tempfile,subprocess as sp
from pathlib import Path
here=Path(__file__).resolve().parent; dev=here.parent
ap=argparse.ArgumentParser(description=__doc__)
ap.add_argument('observed_profile',type=Path);ap.add_argument('clone_dir',type=Path);ap.add_argument('out',type=Path)
ap.add_argument('--llvm',type=Path,default=Path('/opt/homebrew/opt/llvm@18/bin'))
ap.add_argument('--optimization',choices=('baseline','staged-lto'),default='baseline',help='staged-lto is an offline size experiment, not hardware-qualified')
ap.add_argument('--producer',choices=('stock','compact'),default='stock',help='compact is an exact-build specialized producer; requires separate hardware qualification')
ap.add_argument('--outline',action='store_true')
ap.add_argument('--producer-lto',action='store_true')
a=ap.parse_args();B=a.llvm;target=a.out.resolve();target.mkdir(parents=True,exist_ok=True)
def run(*args):return sp.check_output([str(s) for s in args],text=True)
def sha(p):return hashlib.file_digest(p.open('rb'),'sha256').hexdigest()
if sha(a.clone_dir/'graph_clone.o')!='ef69695411a48b866958147bce0c4ea92973577dc4f529148f53183644a84388':raise ValueError('unexpected private raw producer')
base=json.loads(a.observed_profile.read_text())
if base['sidecar_sha256']!='1be3a14f7f25ea298929b485ad0cdf5e17441cd5ea94cb85f22a4f8500c9eb8e':raise ValueError('not the reviewed batched-quarter packet')
with tempfile.TemporaryDirectory(dir=target) as td:
 w=Path(td)
 header=(here/'full_profile.h').read_text()
 (w/'full_profile.h').write_text(header)
 common='--target=x86_64-unknown-none -Oz -ffreestanding -fPIC -ffunction-sections -fdata-sections -fno-builtin -fno-stack-protector -fno-vectorize -fno-slp-vectorize -fno-jump-tables -fno-unwind-tables -fno-asynchronous-unwind-tables -fvisibility=hidden -mno-red-zone'.split()
 if a.optimization=='staged-lto':common+=['-flto=full']
 for n in ('full','trip','motion','live','dimensions','publish','history','returned_output'):
    f=['-mgeneral-regs-only'] if n in ('full','trip','motion') else []
    if n=='full':f+=['-DHX_LIVE_TILE']
    if n=='trip':f+=['-Dcanary_tick=ht_step','-DHT_CONFIRMED_HISTORY']
    source=dev/'output-observer/returned_output.c' if n=='returned_output' else here/(n+'.c')
    if a.optimization=='staged-lto' and n=='full':
     source=w/'full.c'
     source.write_text((here/'full.c').read_text().replace('params a={4,0,0,0,p};','params a; volatile hx32 *q=(volatile hx32 *)&a; q[0]=4; q[1]=0; q[2]=0; q[3]=0; a.capture=p;'))
    run(B/'clang',*common,*f,'-I',w,'-I',dev/'output-observer','-I',dev/'raw-chain','-I',dev/'scene-triplet','-I',dev/'backend','-I',dev/'velocity-trial','-c',source,'-o',w/(n+'.o'))
 for n,c in [('entry',here/'entry.S'),('capture',dev/'clear-trial/graph_capture.S'),('grid',dev/'raw-chain/grid.S')]:
    run(B/'clang','--target=x86_64-unknown-none','-c',c,'-o',w/(n+'.o'))
 v=dev/'velocity-trial'
 fragments=[v/'hook.ld',v/'velocity.ld',dev/'scene-triplet/triplet.ld',dev/'clear-trial/graph_buffers.ld',dev/'learned-trial/enc0_imports.ld',a.clone_dir/'graph_clone.ld',dev/'image-publication/image.ld',dev/'composition/composition.ld']
 ld='\n'.join(k.read_text() for k in fragments)
 ld+='\nht_postprocess_color_name = 0x7096c12;\n'
 ld+='\nhx_extract_texture = 0xb6bc36;\n'
 ld+='\nhx_import_buffer = 0x74e1fa; hx_extract_buffer = 0xb6b956;\n'
 ld+='\nrhi_global = 0x8ed14a8; rhi_vtable = 0x7dd2290; shader_factory = 0xe0a57a; shader_vtable = 0x7dd3138;\n'
 (w/'recipe.ld').write_text(ld)
 objs=[w/(n+'.o') for n in ('entry','full','trip','motion','live','dimensions','publish','history','returned_output','capture','grid')]+[a.clone_dir/'graph_clone.o']
 if a.producer_lto:
  run(B/'clang',*common,'-mgeneral-regs-only','-DHX_DYNAMIC_GRID','-I',dev/'clear-trial','-c',here/'producer.c','-o',w/'producer.bc')
 if a.optimization=='staged-lto':
  # Optimize before title addresses exist, retaining PC-relative external relocations.
  # Volatile parameter stores above prevent a constant pool in execute-only text.
  run(B/'llvm-link',*[w/(n+'.o') for n in ('full','trip','motion','live','dimensions','publish','history','returned_output')]+([w/'producer.bc'] if a.producer_lto else []),'-o',w/'merged.bc')
  run(B/'opt','-passes=internalize,globaldce,default<Oz>,mergefunc','-internalize-public-api-list=canary_tick',w/'merged.bc','-o',w/'optimized.bc')
  run(B/'llc',*(['-enable-machine-outliner=always'] if a.outline else []),'-function-sections','-data-sections','-filetype=obj','-relocation-model=pic',w/'optimized.bc','-o',w/'optimized.o')
  objs=[w/'optimized.o']+[w/(n+'.o') for n in ('entry','capture','grid')]+[a.clone_dir/'graph_clone.o']
 if a.producer_lto:
  objs=[o for o in objs if o.name not in ('capture.o','grid.o','graph_clone.o')]
 elif a.producer=='compact':
  run(B/'clang',*common[:common.index('-flto=full')] if '-flto=full' in common else common,'-mgeneral-regs-only','-DHX_DYNAMIC_GRID','-I',dev/'clear-trial','-c',here/'producer.c','-o',w/'producer.o')
  objs=[o for o in objs if o.name not in ('capture.o','grid.o','graph_clone.o')]+[w/'producer.o']
 elf=w/'stage' 
 run(B/'ld.lld','--emit-relocs','--gc-sections','--icf=safe','-T',w/'recipe.ld',*objs,'-o',elf)
 rel=run(B/'llvm-readelf','-Wr',elf)
 types=re.findall(r'R_X86_64_\w+',rel)
 if not types or set(types)-{'R_X86_64_PC32','R_X86_64_PLT32'}:raise ValueError('absolute or unknown title relocation in linked text: '+str(set(types)))
 (target/'audit-relocs.txt').write_text(rel)
 (target/'audit-disasm.txt').write_text(run(B/'llvm-objdump','-d','--no-show-raw-insn',elf))
 sections=run(B/'llvm-size','-A',elf)
 sec={line.split()[0]:int(line.split()[1]) for line in sections.splitlines() if line.startswith('.')}
 if set(sec)!={'.text'} or sec['.text']>10368:
  (target/'size-rejection.json').write_text(json.dumps({'sections':sec,'budget':10368,'packed':False},indent=2)+'\n');raise ValueError(sec)
 out=target/'probe.bin'
 run(B/'llvm-objcopy','-O','binary','--only-section=.text',elf,out)
 if out.stat().st_size!=sec['.text']:raise ValueError('flat size mismatch')
 sym={r.split()[2]:int(r.split()[0],16) for r in run(B/'llvm-nm',elf).splitlines() if len(r.split())==3}
 if sym['canary_entry']!=0x6ce5780:raise ValueError('entry mismatch')
 receipt={'producer':a.producer,'optimization':a.optimization,'compiler':run(B/'clang','--version').splitlines()[0],'stub_bytes':out.stat().st_size,'stub_sha256':sha(out),'entry_symbol':'canary_entry','entry_address':hex(sym['canary_entry']),'budget_bytes':10368,
  'spare_bytes':10368-out.stat().st_size,'sections':sec,'linked_relocations':{k:types.count(k) for k in sorted(set(types))},'base_packet_sha256':base['sidecar_sha256'],'source':'in-process newly fenced color / depth / motion; private graph only',
  'private_copy_sha256':sha(a.clone_dir/'graph_clone.o'),'capture_ordinal':60,'result_offset_in_arena':1269504,'result_bytes':131072,'guard_each':256,
  'owner_offsets':{'capture_phase':16,'model_phase':20,'capture_owner':24,'model_receipt':32},
  'model_receipt_offsets':{'stage':16,'scheduled':20,'polls':24,'guards_bad':32,'finite_rgb_pixels':36,'output_fnv1a32':44,'asset_fnv1a64':56,'tile_pixels':64,'tile_bytes':80,'live_inputs':84,'source_render_tick_u64':88,'submission_render_tick_u64':96},
  'hardware':'not exercised by this build','scope':'same-frame GPU inputs and private Model E INIT; no display publication'}
 for key in ('owner_offsets','capture_ordinal','result_offset_in_arena'):receipt.pop(key,None)
 receipt.update(source='same-frame native GPU color/depth/motion conversion',scope='full 3840x2160 Model E GPU reconstruction, persistent history; diagnostic readback only',capture_frames=1 if here.name=='firstframe' else 60,result_offset_in_gpu_arena=197075456,full_image_bytes=29491200,gpu_feature_bytes=231255296,gpu_parameter_bytes=1923840,guard_each=256)
 receipt.update(scope='guard-identification diagnostic; strict Model E; no TSR branch; fault register metadata',capture_frames=None,result_bytes=3840,guard_each=None)
 receipt['model_receipt_offsets']={'stage':16,'passes':20,'published':24,'imports':28,'withheld_publications':32,'frames':40,'epochs':48,'asset_fnv1a64':56,'trace_pointer':64,'trace_bytes':80,'live_inputs':84,'source_tick':88,'submitted_tick':96}
 receipt.update(runtime_setting_test={},setting_policy='Stock dynamic-resolution settings and input dimensions preserved; no shadow or volumetric-fog writes; k5n coverage added up to the existing 2304x1296 input allocation; no TSR branch; diagnostic stops on contract failure; hardware qualification pending',input_policy='Use same-frame stock TSR input, without resizing or forcing a render resolution',reconstruction=[2560,1440],presentation=[3840,2160],hardware_qualified=False)
 print(json.dumps(receipt,indent=2));(target/'build.json').write_text(json.dumps(receipt,indent=2)+'\n')
