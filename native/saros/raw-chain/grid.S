.intel_syntax noprefix
.text
.hidden hx_raw_grid
.globl hx_raw_grid
hx_raw_grid:
    pushfq
    push r10
    push r11
    mov r10,[rbp-0x90]
    mov r10,[r10+0x10]
    cmp dword ptr [r10+0xd8],1
    jne .Lfallback
    mov r11d,[r10+0x158]
    dec r11d
    cmp r11d,127
    ja .Lrefuse
    mov r11d,[r10+0x15c]
    dec r11d
    cmp r11d,127
    ja .Lrefuse
    mov r11d,[r10+0x160]
    dec r11d
    cmp r11d,63
    ja .Lrefuse
    inc r11d
    mov [rbx+0x1e0],r11d
    mov r11,[r10+0x158]
    mov [rbx+0x1d8],r11
    jmp .Ldone
.Lrefuse:
    mov dword ptr [r10+0xd8],3
.Lfallback:
    mov dword ptr [rbx+0x1e0],1
.Ldone:
    pop r11
    pop r10
    popfq
    ret
