
.macro ISR_NOERRCODE num
    .global isr\num
    isr\num:
        cli
        pushl $0
        pushl $\num
        jmp isr_common_stub
.endm

.macro ISR_ERRCODE num
    .global isr\num
    isr\num:
        cli
        pushl $\num
        jmp isr_common_stub
.endm

.macro IRQ irq_num isr_num
    .global irq\irq_num
    irq\irq_num:
        cli
        pushl $0
        pushl $\isr_num
        jmp irq_common_stub
.endm 

ISR_NOERRCODE 0
ISR_NOERRCODE 1
ISR_NOERRCODE 2
ISR_NOERRCODE 3
ISR_NOERRCODE 4
ISR_NOERRCODE 5
ISR_NOERRCODE 6
ISR_NOERRCODE 7
ISR_ERRCODE   8
ISR_NOERRCODE 9
ISR_ERRCODE   10
ISR_ERRCODE   11
ISR_ERRCODE   12
ISR_ERRCODE   13
ISR_ERRCODE   14
ISR_NOERRCODE 15
ISR_NOERRCODE 16
ISR_NOERRCODE 17
ISR_NOERRCODE 18
ISR_NOERRCODE 19
ISR_NOERRCODE 20
ISR_NOERRCODE 21
ISR_NOERRCODE 22
ISR_NOERRCODE 23
ISR_NOERRCODE 24
ISR_NOERRCODE 25
ISR_NOERRCODE 26
ISR_NOERRCODE 27
ISR_NOERRCODE 28
ISR_NOERRCODE 29
ISR_NOERRCODE 30
ISR_NOERRCODE 31


IRQ 0, 32
IRQ 1, 33
IRQ 2, 34
IRQ 3, 35
IRQ 4, 36
IRQ 5, 37
IRQ 6, 38
IRQ 7, 39
IRQ 8, 40
IRQ 9, 41
IRQ 10, 42
IRQ 11, 43
IRQ 12, 44
IRQ 13, 45
IRQ 14, 46
IRQ 15, 47


isr_common_stub:
    pusha           # Push Stack
    mov %ds, %ax    # Push lower 16 bits of eax == ds
    push %eax       # Save the data segment descriptor

    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs

    call isr_handler
    pop %ebx     # load the original data segment descrip
    
    mov %bx, %ds
    mov %bx, %es
    mov %bx, %fs
    mov %bx, %gs

    popa
    add $8, %esp
    sti
    iret    


irq_common_stub:
    pusha           # Push Stack

    mov %ds, %ax    # Push lower 16 bits of eax == ds
    push %eax       # Save the data segment descriptor
    
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    
    call irq_handler

    pop %ebx     # load the original data segment descrip

    mov %bx, %ds
    mov %bx, %es
    mov %bx, %fs
    mov %bx, %gs

    popa
    add $8, %esp
    sti
    iret    

