#!/usr/bin/env python3
"""Build only the private RDG memcpy specialization from the owner's exact SAROS ELF."""
import argparse
import hashlib
import io
import json
import os
from pathlib import Path
import struct
import subprocess
from capstone import Cs, CS_ARCH_X86, CS_MODE_64, CS_GRP_CALL, CS_GRP_JUMP
from capstone.x86_const import X86_OP_IMM, X86_OP_MEM, X86_REG_RIP

START, END = 0x18cba57,0x18cc464
HOOK = 0x18cc189
JT_INSN,JT = 0x18cbddd,0x75fdb14
STOCK_SHA='26585333efaf6a14b914da8d33d3b2fc5471155ffb7dc724d84371e028979f46'
ISLAND_SHA='d23fd6372bc45c4a2892720ec42088c04e9766847a5f168df34273d2273ee928'
JUMP_TARGETS=[0x18cc42a,0x18cc200,0x18cbdfd,0x18cc3c5]

def require(condition, message):
    if not condition:raise ValueError(message)

def source(elf):
    with elf.open('rb') as f:
        require(hashlib.file_digest(f,'sha256').hexdigest()==STOCK_SHA,'unsupported decoded SAROS executable')
        f.seek(0)
        hdr=f.read(64);entsize,nph=struct.unpack_from('<HH',hdr,54)
        f.seek(struct.unpack_from('<Q',hdr,32)[0])
        ph=[struct.unpack('<II6Q',f.read(entsize)) for _ in range(nph)]
        def read(a,size):
            b=next(p for p in ph if p[0]==1 and p[3]<=a and a+size<=p[3]+p[5])
            f.seek(b[2]+a-b[3]);return f.read(size)
        blob=read(START,END-START)
        require(hashlib.sha256(blob).hexdigest()==ISLAND_SHA,'unsupported stock implementation')
        targets=[JT+k for k in struct.unpack('<4i',read(JT,16))]
        require(targets==JUMP_TARGETS,'unsupported resource dispatch table')
        return blob

def build(elf,folder,llvm):
    data=source(elf)
    folder.mkdir(parents=True,exist_ok=True)
    md=Cs(CS_ARCH_X86,CS_MODE_64);md.detail=True
    out=io.StringIO()
    out.write('.section .text.hx_private_memcpy,"ax",@progbits\n.hidden hx_private_memcpy\n.globl hx_private_memcpy\nhx_private_memcpy:\n')
    symbols={}
    relocations=[]
    end=START
    selectors_skipped=0
    def emit_bytes(b):
        if b:out.write('.byte '+','.join(hex(x) for x in b)+'\n')
    for ins in md.disasm(data,START):
        require(ins.address==end,'undecoded private implementation bytes')
        end+=ins.size
        if JT_INSN <= ins.address < 0x18cbdfd:
            # This RVA range originally reads a four-way table in stock rodata. The clone
            # is execute-only: preserve the offset/vector calculations and branch by code.
            selectors_skipped+=ins.size
            if ins.address==JT_INSN:
                out.write('jmp hx_private_select_resource_kind\n.fill 27,1,0x90\n')
            continue
        if ins.address==HOOK:
            require(bytes(ins.bytes)==bytes.fromhex('48898bd0010000'),'wrong private capture site')
            out.write('call hx_graph_capture\nnop\nnop\n')
            continue
        if ins.mnemonic=='jmp' and ins.operands[0].type!=X86_OP_IMM:
            require(ins.address==0x18cbdfb,'unhandled computed branch')
        fix=None
        for op in ins.operands:
            if op.type==X86_OP_MEM and op.mem.base==X86_REG_RIP:
                fix=(ins.disp_offset,ins.disp_size,end+op.mem.disp)
            elif op.type==X86_OP_IMM and (ins.group(CS_GRP_JUMP) or ins.group(CS_GRP_CALL)):
                if not START<=op.imm<END:fix=(ins.imm_offset,ins.imm_size,op.imm)
        if not fix:
            emit_bytes(bytes(ins.bytes));continue
        off,n,target=fix
        require(n==4,'unsupported relative field width')
        if ins.address==JT_INSN:
            require(target==JT,'wrong resource-kind dispatch table')
            symbol='hx_private_jump_table'
        else:
            require(not START<=target<END,'unexpected data reference into private implementation')
            symbol='hx_stock_%x'%target;symbols[symbol]=target
        emit_bytes(bytes(ins.bytes[:off]))
        out.write(f'.long {symbol} - . - {ins.size-off}\n')
        emit_bytes(bytes(ins.bytes[off+n:]))
        relocations.append([hex(ins.address),hex(target),symbol])
    require(end==END,'private function length mismatch')
    require(selectors_skipped==32,'unexpected switch extent')
    out.write('hx_private_select_resource_kind:\n')
    # Exact three original vector instructions preserve the host-template coordinate updates.
    for lo,hi in ((0x18cbde4,0x18cbdea),(0x18cbdf1,0x18cbdf5),(0x18cbdf5,0x18cbdfb)):
        emit_bytes(data[lo-START:hi-START])
    out.write('.intel_syntax noprefix\n')
    for kind,target in enumerate(JUMP_TARGETS[:3]):
        out.write(f'cmp eax,{kind}\nje hx_private_memcpy+{target-START}\n')
    out.write(f'jmp hx_private_memcpy+{JUMP_TARGETS[3]-START}\n')
    asm=folder/'graph_clone.S'
    asm.write_text(out.getvalue())
    (folder/'graph_clone.ld').write_text(''.join(f'{s} = 0x{v:x};\n' for s,v in sorted(symbols.items())))
    subprocess.run([str(llvm/'clang'),'--target=x86_64-unknown-none','-c',str(asm),'-o',str(folder/'graph_clone.o')],check=True)
    report={'stock_sha256':STOCK_SHA,'stock_slice_sha256':ISLAND_SHA,'stock_span':[hex(START),hex(END)],
            'private_change':'Insert a guarded shader capture before native pass publication and replace the four-way indexed table with private direct branches for execute-only code.',
            'jump_table_targets':[hex(x) for x in JUMP_TARGETS],'relocations':relocations,
            'artifact':'Private stock-derived object and assembly, scratch only.'}
    (folder/'graph_clone.json').write_text(json.dumps(report,indent=2)+'\n')
    return report

if __name__=='__main__':
    a=argparse.ArgumentParser(description=__doc__)
    a.add_argument('stock_elf',type=Path);a.add_argument('work_dir',type=Path)
    a.add_argument('--llvm',type=Path,default=Path('/opt/homebrew/opt/llvm@18/bin'))
    args=a.parse_args()
    report=build(args.stock_elf,args.work_dir,args.llvm)
    print('Verified %d relocated stock references.'%len(report['relocations']))
