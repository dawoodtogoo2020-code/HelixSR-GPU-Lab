/* Called from a private relocated copy of the native creator before the new pass is published.
 * rbx = the private pass. rdx = real frozen FShader, rcx = original nonowning shader-map reference.
 * rax = native packed dispatch XY. The stock source and caller's flags/registers are preserved. */
.intel_syntax noprefix
.text
.hidden hx_graph_capture
.globl hx_graph_capture
hx_graph_capture:
    pushfq
    push r10
    push r11
    mov r10,[rbp-0x90]
    mov r10,[r10+0x10]
    cmp dword ptr [r10+0xd8],0
    jne .Lrefuse
    lea r11,[rip+hx_memcpy_pass_vtable]
    cmp [rbx],r11
    jne .Lrefuse
    lea r11,[rip+hx_memcpy_metadata]
    cmp [rbx+0x1b8],r11
    jne .Lrefuse
    mov r11,[r15+0x40]  /* private destination UAV in the confirmed frozen binding row */
    test r11,r11
    jz .Lrefuse
    mov r11,[r11+0x30]
    cmp r11,[r10+0xe8]
    jne .Lrefuse
    mov r11,[r15+0x78]  /* private argument SRV */
    test r11,r11
    jz .Lrefuse
    mov r11,[r11+0x40]
    cmp r11,[r10+0xe0]
    jne .Lrefuse
    test rdx,rdx
    jz .Lrefuse
    cmp dword ptr [rdx+0xd4],15
    jne .Lrefuse
    movabs r11,0x0000000100000001
    cmp rax,r11
    jne .Lrefuse
    mov r11,[r10+8]
    test r11,r11
    jz .Lrefuse
    mov r11,[r11+0x40]
    test r11,r11
    jz .Lrefuse
    cmp qword ptr [r11+0x78],0
    je .Lrefuse
    mov [rbx+0x1d0],r10
    mov dword ptr [r10+0xd8],1
    jmp .Ldone
.Lrefuse:
    mov [rbx+0x1d0],rcx
    mov dword ptr [r10+0xd8],2
.Ldone:
    pop r11
    pop r10
    popfq
    ret
