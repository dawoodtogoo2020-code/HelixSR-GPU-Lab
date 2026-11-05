.intel_syntax noprefix
.section .text.entry,"ax",@progbits
.global canary_entry
canary_entry:
 jmp canary_tick
