@ add.s

@ declare add_asm as a global function so we can call it
.global	add_asm
.global sub_asm
.global mul_asm

@ here is the definition of add_asm
add_asm:
    add r0, r0, r1      @ add r0 and r1 (first two args) and place result in r0
    mov pc, lr          @ return back to the caller

sub_asm:
    sub r0, r0, r1      @ sub r0 and r1 (first two args) and place result in r0
    mov pc, lr          @ return back to the caller

mul_asm:
    mul r0, r1, r0      @ sub r0 and r1 (first two args) and place result in r0
    mov pc, lr          @ return back to the caller

