@ strlen.s

/* assembly version of strlen */
.global	mystrlen
mystrlen:
    mov r1, #0
top:
    ldrb r2, [r0, r1]
    cmp r2, #0
    beq done
    add r1, r1, #1
    b top
done:
    mov r0, r1
    mov pc, lr
