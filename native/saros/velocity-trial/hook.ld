ENTRY(canary_entry)
SECTIONS { . = 0x6ce5780; .text : { *(.text.entry) *(.text .text.*) } /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) } }
stock_tsr = 0x984e0b;
game_alloc = 0xe5157;
phase = 0x9154604;
payload = 0x9154608;
/* The original register-preserving SceneColor wrapper also defines its optional readback
   decorator. The velocity pass uses native unarmed-fence polling and never installs it. */
real_enqueue = 0x35d1990;
