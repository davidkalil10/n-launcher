// MIT - synthetic module, no game code, imports, SVCs or external relocations.
.section .text
.global nx2link_fixture_image
nx2link_fixture_image:
    .space 0x10
    .ascii "NRO0"
    .word 0
    .word 0x4000
    .word 1                     // first page remains read-only
    .word 0, 0x2000             // header + executable page
    .word 0x2000, 0x1000        // read-only data
    .word 0x3000, 0x1000        // writable data
    .word 0x1000, 0             // BSS supplied as separate heap pages
    .ascii "nx2-link:synthetic-module:v1"
    .org 0x1000
fixture_entry:
    adr x3, fixture_ro
    ldr w1, [x3]
    adr x3, fixture_data
    ldr w2, [x3]
    add w4, w2, #1
    str w4, [x3]
    adr x3, fixture_bss
    ldr w4, [x3]
    add w5, w4, #1
    str w5, [x3]
    eor w0, w0, w1
    add w0, w0, w2
    add w0, w0, w4
    ret
    .org 0x2000
fixture_ro:
    .word 0x1357
    .org 0x3000
fixture_data:
    .word 0x2468
    .org 0x4000
fixture_bss:
