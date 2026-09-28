/* Copyright 2026 Rhett Creighton. Licensed under Apache-2.0. */
ENTRY(blue_m3_reset)
MEMORY {
    FLASH (rx) : ORIGIN = 0x00000000, LENGTH = 4M
    RAM (rwx) : ORIGIN = 0x20000000, LENGTH = 6K
}
SECTIONS {
    .vectors : { KEEP(*(.vectors)) } > FLASH
    .text : { *(.text*) *(.rodata*) } > FLASH
    .ARM.exidx : { *(.ARM.exidx*) } > FLASH
    .data : AT(LOADADDR(.ARM.exidx) + SIZEOF(.ARM.exidx)) {
        _sdata = .; *(.data*) _edata = .;
    } > RAM
    _sidata = LOADADDR(.data);
    .bss (NOLOAD) : { _sbss = .; *(.bss*) *(COMMON) _ebss = .; } > RAM
    _stack_bottom = ORIGIN(RAM) + LENGTH(RAM) - 2048;
    _stack_top = ORIGIN(RAM) + LENGTH(RAM);
    end = _ebss;
    ASSERT(_ebss <= _stack_bottom, "M3 test data overlaps reserved stack")
}
