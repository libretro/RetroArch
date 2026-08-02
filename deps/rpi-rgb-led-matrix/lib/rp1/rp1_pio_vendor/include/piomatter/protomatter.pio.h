#pragma once

const int protomatter_wrap = 4;
const int protomatter_wrap_target = 0;
const int protomatter_sideset_pin_count = 1;
const bool protomatter_sideset_enable = 1;
const uint16_t protomatter[] = {
    // ; data format (out-shift-right):
    // ; MSB ... LSB
    // ; 0 ddd......ddd: 31-bit delay
    // ; 1 ccc......ccc: 31 bit data count
    // .side_set 1 opt
    // .wrap_target
    // top:
    0x6021, //     out x, 1
    0x605f, //     out y, 31
    0x0025, //     jmp !x do_delay
            // data_loop:
    0x6000, //     out pins, 32
    0x1883, //     jmp y--, data_loop  side 1 ; assert clk bit
            // .wrap
            // do_delay:
    0x6000, //     out pins, 32
            // delay_loop:
    0x0086, //     jmp y--, delay_loop
    0x0000, //     jmp top
    //     ;; fill program out to 32 instructions so nothing else can load
    0xa042, //     nop
    0xa042, //     nop
    0xa042, //     nop
    0xa042, //     nop
    0xa042, //     nop
    0xa042, //     nop
    0xa042, //     nop
    0xa042, //     nop
    0xa042, //     nop
    0xa042, //     nop
    0xa042, //     nop
    0xa042, //     nop
    0xa042, //     nop
    0xa042, //     nop
    0xa042, //     nop
    0xa042, //     nop
    0xa042, //     nop
    0xa042, //     nop
    0xa042, //     nop
    0xa042, //     nop
    0xa042, //     nop
    0xa042, //     nop
    0xa042, //     nop
    0xa042, //     nop
    0xa042, //     nop
};
