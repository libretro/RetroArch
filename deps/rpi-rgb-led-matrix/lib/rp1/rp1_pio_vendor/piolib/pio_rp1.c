// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 Raspberry Pi Ltd.
 * All rights reserved.
 */

#include <errno.h>
#include <fcntl.h>
#include <malloc.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define PIOLIB_INTERNALS

#include "pio_platform.h"

#define pio_encode_delay _pio_encode_delay
#define pio_encode_sideset _pio_encode_sideset
#define pio_encode_sideset_opt _pio_encode_sideset_opt
#define pio_encode_jmp _pio_encode_jmp
#define pio_encode_jmp_not_x _pio_encode_jmp_not_x
#define pio_encode_jmp_x_dec _pio_encode_jmp_x_dec
#define pio_encode_jmp_not_y _pio_encode_jmp_not_y
#define pio_encode_jmp_y_dec _pio_encode_jmp_y_dec
#define pio_encode_jmp_x_ne_y _pio_encode_jmp_x_ne_y
#define pio_encode_jmp_pin _pio_encode_jmp_pin
#define pio_encode_jmp_not_osre _pio_encode_jmp_not_osre
#define pio_encode_wait_gpio _pio_encode_wait_gpio
#define pio_encode_wait_pin _pio_encode_wait_pin
#define pio_encode_wait_irq _pio_encode_wait_irq
#define pio_encode_in _pio_encode_in
#define pio_encode_out _pio_encode_out
#define pio_encode_push _pio_encode_push
#define pio_encode_pull _pio_encode_pull
#define pio_encode_mov _pio_encode_mov
#define pio_encode_mov_not _pio_encode_mov_not
#define pio_encode_mov_reverse _pio_encode_mov_reverse
#define pio_encode_irq_set _pio_encode_irq_set
#define pio_encode_irq_wait _pio_encode_irq_wait
#define pio_encode_irq_clear _pio_encode_irq_clear
#define pio_encode_set _pio_encode_set
#define pio_encode_nop _pio_encode_nop
#include "hardware/pio_instructions.h"
#undef pio_encode_delay
#undef pio_encode_sideset
#undef pio_encode_sideset_opt
#undef pio_encode_jmp
#undef pio_encode_jmp_not_x
#undef pio_encode_jmp_x_dec
#undef pio_encode_jmp_not_y
#undef pio_encode_jmp_y_dec
#undef pio_encode_jmp_x_ne_y
#undef pio_encode_jmp_pin
#undef pio_encode_jmp_not_osre
#undef pio_encode_wait_gpio
#undef pio_encode_wait_pin
#undef pio_encode_wait_irq
#undef pio_encode_in
#undef pio_encode_out
#undef pio_encode_push
#undef pio_encode_pull
#undef pio_encode_mov
#undef pio_encode_mov_not
#undef pio_encode_mov_reverse
#undef pio_encode_irq_set
#undef pio_encode_irq_wait
#undef pio_encode_irq_clear
#undef pio_encode_set
#undef pio_encode_nop

#include "piolib.h"
#include "piolib_priv.h"
#include "hardware/gpio.h"
#include "hardware/pio.h"
#include "hardware/regs/proc_pio.h"
#include "rp1_pio_if.h"

typedef struct rp1_pio_handle {
    struct pio_instance base;
    const char *devname;
    int fd;
} *RP1_PIO;

#define smc_to_rp1(_config, _c) rp1_pio_sm_config *_c = (rp1_pio_sm_config*)_config

#define GPIOS_MASK ((1 << RP1_PIO_GPIO_COUNT) - 1)

STATIC_ASSERT(sizeof(rp1_pio_sm_config) <= sizeof(pio_sm_config));

static inline void check_sm_param(__unused uint sm)
{
    valid_params_if(PIO, sm < RP1_PIO_SM_COUNT);
}

static inline void check_sm_mask(__unused uint mask)
{
    valid_params_if(PIO, mask < (1u << RP1_PIO_SM_COUNT));
}

static pio_sm_config rp1_pio_get_default_sm_config(PIO)
{
    pio_sm_config c = { { 0 } };
    sm_config_set_clkdiv_int_frac(&c, 1, 0);
    sm_config_set_wrap(&c, 0, 31);
    sm_config_set_in_shift(&c, true, false, 32);
    sm_config_set_out_shift(&c, true, false, 32);
    return c;
}

static uint rp1_pio_encode_delay(PIO, uint cycles)
{
    return _pio_encode_delay(cycles);
}

static uint rp1_pio_encode_sideset(PIO, uint sideset_bit_count, uint value)
{
    return _pio_encode_sideset(sideset_bit_count, value);
}

static uint rp1_pio_encode_sideset_opt(PIO, uint sideset_bit_count, uint value)
{
    return _pio_encode_sideset_opt(sideset_bit_count, value);
}

static uint rp1_pio_encode_jmp(PIO, uint addr)
{
    return _pio_encode_jmp(addr);
}

static uint rp1_pio_encode_jmp_not_x(PIO, uint addr)
{
    return _pio_encode_jmp_not_x(addr);
}

static uint rp1_pio_encode_jmp_x_dec(PIO, uint addr)
{
    return _pio_encode_jmp_x_dec(addr);
}

static uint rp1_pio_encode_jmp_not_y(PIO, uint addr)
{
    return _pio_encode_jmp_not_y(addr);
}

static uint rp1_pio_encode_jmp_y_dec(PIO, uint addr)
{
    return _pio_encode_jmp_y_dec(addr);
}

static uint rp1_pio_encode_jmp_x_ne_y(PIO, uint addr)
{
    return _pio_encode_jmp_x_ne_y(addr);
}

static uint rp1_pio_encode_jmp_pin(PIO, uint addr)
{
    return _pio_encode_jmp_pin(addr);
}

static uint rp1_pio_encode_jmp_not_osre(PIO, uint addr)
{
    return _pio_encode_jmp_not_osre(addr);
}

static uint rp1_pio_encode_wait_gpio(PIO, bool polarity, uint gpio)
{
    return _pio_encode_wait_gpio(polarity, gpio);
}

static uint rp1_pio_encode_wait_pin(PIO, bool polarity, uint pin)
{
    return _pio_encode_wait_pin(polarity, pin);
}

static uint rp1_pio_encode_wait_irq(PIO, bool polarity, bool relative, uint irq)
{
    return _pio_encode_wait_irq(polarity, relative, irq);
}

static uint rp1_pio_encode_in(PIO, enum pio_src_dest src, uint count)
{
    return _pio_encode_in(src, count);
}

static uint rp1_pio_encode_out(PIO, enum pio_src_dest dest, uint count)
{
    return _pio_encode_out(dest, count);
}

static uint rp1_pio_encode_push(PIO, bool if_full, bool block)
{
    return _pio_encode_push(if_full, block);
}

static uint rp1_pio_encode_pull(PIO, bool if_empty, bool block)
{
    return _pio_encode_pull(if_empty, block);
}

static uint rp1_pio_encode_mov(PIO, enum pio_src_dest dest, enum pio_src_dest src)
{
    return _pio_encode_mov(dest, src);
}

static uint rp1_pio_encode_mov_not(PIO, enum pio_src_dest dest, enum pio_src_dest src)
{
    return _pio_encode_mov_not(dest, src);
}

static uint rp1_pio_encode_mov_reverse(PIO, enum pio_src_dest dest, enum pio_src_dest src)
{
    return _pio_encode_mov_reverse(dest, src);
}

static uint rp1_pio_encode_irq_set(PIO, bool relative, uint irq)
{
    return _pio_encode_irq_set(relative, irq);
}

static uint rp1_pio_encode_irq_wait(PIO, bool relative, uint irq)
{
    return _pio_encode_irq_wait(relative, irq);
}

static uint rp1_pio_encode_irq_clear(PIO, bool relative, uint irq)
{
    return _pio_encode_irq_clear(relative, irq);
}

static uint rp1_pio_encode_set(PIO, enum pio_src_dest dest, uint value)
{
    return _pio_encode_set(dest, value);
}

static uint rp1_pio_encode_nop(PIO)
{
    return _pio_encode_nop();
}

static int rp1_ioctl(PIO pio, int request, void *args)
{
    RP1_PIO rp = (RP1_PIO)pio;
    int err = ioctl(rp->fd, request, args);
    switch (err) {
    case -EREMOTEIO:
    case -ETIMEDOUT:
        pio_panic("Error communicating with RP1");
        break;
    default:
        break;
    }
    return err;
}

static int rp1_pio_sm_config_xfer(PIO pio, uint sm, uint dir, uint buf_size, uint buf_count)
{
    struct rp1_pio_sm_config_xfer_args args = { .sm = sm, .dir = dir, .buf_size = buf_size, .buf_count = buf_count };
    struct rp1_pio_sm_config_xfer32_args args32 = { .sm = sm, .dir = dir, .buf_size = buf_size, .buf_count = buf_count };
    int err;
    check_sm_param(sm);
    if (buf_size > 0xffff || buf_count > 0xffff)
        err = rp1_ioctl(pio, PIO_IOC_SM_CONFIG_XFER32, &args32);
    else
        err = rp1_ioctl(pio, PIO_IOC_SM_CONFIG_XFER, &args);
    return err;
}

static int rp1_pio_sm_xfer_data(PIO pio, uint sm, uint dir, uint data_bytes, void *data)
{
    struct rp1_pio_sm_xfer_data_args args = { .sm = sm, .dir = dir, .data_bytes = data_bytes, .rsvd = 0, .data = data };
    struct rp1_pio_sm_xfer_data32_args args32 = { .sm = sm, .dir = dir, .data_bytes = data_bytes, .data = data };
    int err;
    check_sm_param(sm);
    if (data_bytes > 0xffff)
        err = rp1_ioctl(pio, PIO_IOC_SM_XFER_DATA32, &args32);
    else
        err = rp1_ioctl(pio, PIO_IOC_SM_XFER_DATA, &args);
    return err;
}

static bool rp1_pio_can_add_program_at_offset(PIO pio, const pio_program_t *program, uint offset)
{
    struct rp1_pio_add_program_args args = { .num_instrs = program->length, .origin = program->origin };
    int err;
    valid_params_if(PIO, offset < RP1_PIO_INSTRUCTION_COUNT || offset == PIO_ORIGIN_ANY);
    valid_params_if(PIO, program->length <= RP1_PIO_INSTRUCTION_COUNT);
    valid_params_if(PIO, offset + program->length <= RP1_PIO_INSTRUCTION_COUNT || offset == PIO_ORIGIN_ANY);
    if (program->origin >= 0 && (uint)program->origin != offset)
        return false;
    if (offset != PIO_ORIGIN_ANY)
        args.origin = offset;
    memcpy(args.instrs, program->instructions, program->length * sizeof(uint16_t));
    err = rp1_ioctl(pio, PIO_IOC_CAN_ADD_PROGRAM, &args);
    return (err > 0);
}

static uint rp1_pio_add_program_at_offset(PIO pio, const pio_program_t *program, uint offset)
{
    struct rp1_pio_add_program_args args = { .num_instrs = program->length, .origin = program->origin };
    valid_params_if(PIO, offset < RP1_PIO_INSTRUCTION_COUNT || offset == PIO_ORIGIN_ANY);
    valid_params_if(PIO, program->length <= RP1_PIO_INSTRUCTION_COUNT);
    valid_params_if(PIO, offset + program->length <= RP1_PIO_INSTRUCTION_COUNT || offset == PIO_ORIGIN_ANY);
    if (offset != PIO_ORIGIN_ANY)
        args.origin = offset;
    memcpy(args.instrs, program->instructions, program->length * sizeof(uint16_t));
    return rp1_ioctl(pio, PIO_IOC_ADD_PROGRAM, &args);
}

static bool rp1_pio_remove_program(PIO pio, const pio_program_t *program, uint offset)
{
    struct rp1_pio_remove_program_args args = { .num_instrs = program->length, .origin = offset };
    valid_params_if(PIO, offset < RP1_PIO_INSTRUCTION_COUNT);
    valid_params_if(PIO, offset + program->length <= RP1_PIO_INSTRUCTION_COUNT);
    return !rp1_ioctl(pio, PIO_IOC_REMOVE_PROGRAM, &args);
}

static bool rp1_pio_clear_instruction_memory(PIO pio)
{
    return !rp1_ioctl(pio, PIO_IOC_CLEAR_INSTR_MEM, NULL);
}

static bool rp1_pio_sm_claim(PIO pio, uint sm)
{
    struct rp1_pio_sm_claim_args args = { .mask = (1 << sm) };
    check_sm_param(sm);
    return (rp1_ioctl(pio, PIO_IOC_SM_CLAIM, &args) >= 0);
}

static bool rp1_pio_sm_claim_mask(PIO pio, uint mask)
{
    struct rp1_pio_sm_claim_args args = { .mask = mask };
    valid_params_if(PIO, !!mask);
    check_sm_mask(mask);
    return (rp1_ioctl(pio, PIO_IOC_SM_CLAIM, &args) >= 0);
}

static bool rp1_pio_sm_unclaim(PIO pio, uint sm)
{
    struct rp1_pio_sm_claim_args args = { .mask = (1 << sm) };
    check_sm_param(sm);
    return !rp1_ioctl(pio, PIO_IOC_SM_UNCLAIM, &args);
}

static int rp1_pio_sm_claim_unused(PIO pio, bool required)
{
    struct rp1_pio_sm_claim_args args = { .mask = 0 };
    int sm = rp1_ioctl(pio, PIO_IOC_SM_CLAIM, &args);
    if (sm < 0 && required)
        pio_panic("No PIO state machines are available");
    return sm;
}

static bool rp1_pio_sm_is_claimed(PIO pio, uint sm)
{
    struct rp1_pio_sm_claim_args args = { .mask = (1 << sm) };
    check_sm_param(sm);
    int err = rp1_ioctl(pio, PIO_IOC_SM_IS_CLAIMED, &args);
    return (err > 0);
}

static void rp1_pio_sm_init(PIO pio, uint sm, uint initial_pc, const pio_sm_config *config)
{
    smc_to_rp1(config, c);
    struct rp1_pio_sm_init_args args = { .sm = sm, .initial_pc = initial_pc, .config = *c };
    valid_params_if(PIO, initial_pc < RP1_PIO_INSTRUCTION_COUNT);

    (void)rp1_ioctl(pio, PIO_IOC_SM_INIT, &args);
}

static void rp1_pio_sm_set_config(PIO pio, uint sm, const pio_sm_config *config)
{
    smc_to_rp1(config, c);
    struct rp1_pio_sm_init_args args = { .sm = sm, .config = *c };

    check_sm_param(sm);
    (void)rp1_ioctl(pio, PIO_IOC_SM_SET_CONFIG, &args);
}

static void rp1_pio_sm_exec(PIO pio, uint sm, uint instr, bool blocking)
{
    struct rp1_pio_sm_exec_args args = { .sm = sm, .instr = instr, .blocking = blocking };

    check_sm_param(sm);
    (void)rp1_ioctl(pio, PIO_IOC_SM_EXEC, &args);
}

static void rp1_pio_sm_clear_fifos(PIO pio, uint sm)
{
    struct rp1_pio_sm_clear_fifos_args args = { .sm = sm };

    check_sm_param(sm);
    (void)rp1_ioctl(pio, PIO_IOC_SM_CLEAR_FIFOS, &args);
}

static void rp1_pio_calculate_clkdiv_from_float(float div, uint16_t *div_int, uint8_t *div_frac)
{
    valid_params_if(PIO, div >= 1 && div <= 65536);
    *div_int = (uint16_t)div;
    if (*div_int == 0) {
        *div_frac = 0;
    } else {
        *div_frac = (uint8_t)((div - (float)*div_int) * (1u << 8u));
    }
}

static void rp1_pio_sm_set_clkdiv_int_frac(PIO pio, uint sm, uint16_t div_int, uint8_t div_frac)
{
    struct rp1_pio_sm_set_clkdiv_args args = { .sm = sm, .div_int = div_int, .div_frac = div_frac };

    check_sm_param(sm);
    invalid_params_if(PIO, div_int == 0 && div_frac != 0);
    (void)rp1_ioctl(pio, PIO_IOC_SM_SET_CLKDIV, &args);
}

static void rp1_pio_sm_set_clkdiv(PIO pio, uint sm, float div)
{
    uint16_t div_int;
    uint8_t div_frac;

    check_sm_param(sm);
    rp1_pio_calculate_clkdiv_from_float(div, &div_int, &div_frac);
    rp1_pio_sm_set_clkdiv_int_frac(pio, sm, div_int, div_frac);
}

static void rp1_pio_sm_set_pins(PIO pio, uint sm, uint32_t pin_values)
{
    struct rp1_pio_sm_set_pins_args args = { .sm = sm, .values = pin_values, .mask = GPIOS_MASK };

    check_sm_param(sm);
    (void)rp1_ioctl(pio, PIO_IOC_SM_SET_PINS, &args);
}

static void rp1_pio_sm_set_pins_with_mask(PIO pio, uint sm, uint32_t pin_values, uint32_t pin_mask)
{
    struct rp1_pio_sm_set_pins_args args = { .sm = sm, .values = pin_values, .mask = pin_mask };

    check_sm_param(sm);
    (void)rp1_ioctl(pio, PIO_IOC_SM_SET_PINS, &args);
}

static void rp1_pio_sm_set_pindirs_with_mask(PIO pio, uint sm, uint32_t pin_dirs, uint32_t pin_mask)
{
    struct rp1_pio_sm_set_pindirs_args args = { .sm = sm, .dirs = pin_dirs, .mask = pin_mask };

    check_sm_param(sm);
    valid_params_if(PIO, (pin_dirs & GPIOS_MASK) == pin_dirs);
    valid_params_if(PIO, (pin_mask & pin_mask) == pin_mask);
    (void)rp1_ioctl(pio, PIO_IOC_SM_SET_PINDIRS, &args);
}

static void rp1_pio_sm_set_consecutive_pindirs(PIO pio, uint sm, uint pin_base, uint pin_count, bool is_out)
{
    uint32_t mask = ((1 << pin_count) - 1) << pin_base;
    struct rp1_pio_sm_set_pindirs_args args = { .sm = sm, .dirs = is_out ? mask : 0, .mask = mask };

    check_sm_param(sm);
    valid_params_if(PIO, pin_base < RP1_PIO_GPIO_COUNT &&
                    pin_count < RP1_PIO_GPIO_COUNT &&
                    (pin_base + pin_count) < RP1_PIO_GPIO_COUNT);
    (void)rp1_ioctl(pio, PIO_IOC_SM_SET_PINDIRS, &args);
}

static void rp1_pio_sm_set_enabled(PIO pio, uint sm, bool enabled)
{
    struct rp1_pio_sm_set_enabled_args args = { .mask = (1 << sm), .enable = enabled };
    check_sm_param(sm);
    (void)rp1_ioctl(pio, PIO_IOC_SM_SET_ENABLED, &args);
}

static void rp1_pio_sm_set_enabled_mask(PIO pio, uint32_t mask, bool enabled)
{
    struct rp1_pio_sm_set_enabled_args args = { .mask = (uint16_t)mask, .enable = enabled };
    check_sm_mask(mask);
    (void)rp1_ioctl(pio, PIO_IOC_SM_SET_ENABLED, &args);
}

static void rp1_pio_sm_restart(PIO pio, uint sm)
{
    struct rp1_pio_sm_restart_args args = { .mask = (1 << sm) };
    check_sm_param(sm);
    (void)rp1_ioctl(pio, PIO_IOC_SM_RESTART, &args);
}

static void rp1_pio_sm_restart_mask(PIO pio, uint32_t mask)
{
    struct rp1_pio_sm_restart_args args = { .mask = (uint16_t)mask };
    check_sm_mask(mask);
    (void)rp1_ioctl(pio, PIO_IOC_SM_RESTART, &args);
}

static void rp1_pio_sm_clkdiv_restart(PIO pio, uint sm)
{
    struct rp1_pio_sm_restart_args args = { .mask = (1 << sm) };
    check_sm_param(sm);
    (void)rp1_ioctl(pio, PIO_IOC_SM_CLKDIV_RESTART, &args);
}

static void rp1_pio_sm_clkdiv_restart_mask(PIO pio, uint32_t mask)
{
    struct rp1_pio_sm_restart_args args = { .mask = (uint16_t)mask };

    check_sm_mask(mask);
    (void)rp1_ioctl(pio, PIO_IOC_SM_CLKDIV_RESTART, &args);
}

static void rp1_pio_sm_enable_sync(PIO pio, uint32_t mask)
{
    struct rp1_pio_sm_enable_sync_args args = { .mask = (uint16_t)mask };

    check_sm_mask(mask);
    (void)rp1_ioctl(pio, PIO_IOC_SM_ENABLE_SYNC, &args);
}

static void rp1_pio_sm_put(PIO pio, uint sm, uint32_t data, bool blocking)
{
    struct rp1_pio_sm_put_args args = { .sm = (uint16_t)sm, .blocking = blocking, .data = data };

    check_sm_param(sm);
    (void)rp1_ioctl(pio, PIO_IOC_SM_PUT, &args);
}

static uint32_t rp1_pio_sm_get(PIO pio, uint sm, bool blocking)
{
    struct rp1_pio_sm_get_args args = { .sm = (uint16_t)sm, .blocking = blocking };

    check_sm_param(sm);
    (void)rp1_ioctl(pio, PIO_IOC_SM_GET, &args);
    return args.data;
}

static void rp1_pio_sm_set_dmactrl(PIO pio, uint sm, bool is_tx, uint32_t ctrl)
{
    struct rp1_pio_sm_set_dmactrl_args args = { .sm = sm, .is_tx = is_tx, .ctrl = ctrl };

    check_sm_param(sm);
    (void)rp1_ioctl(pio, PIO_IOC_SM_SET_DMACTRL, &args);
}

static bool rp1_pio_sm_is_rx_fifo_empty(PIO pio, uint sm)
{
    struct rp1_pio_sm_fifo_state_args args = { .sm = sm, .tx = false };

    check_sm_param(sm);
    (void)rp1_ioctl(pio, PIO_IOC_SM_FIFO_STATE, &args);
    return args.empty;
}

static bool rp1_pio_sm_is_rx_fifo_full(PIO pio, uint sm)
{
    struct rp1_pio_sm_fifo_state_args args = { .sm = sm, .tx = false };

    check_sm_param(sm);
    (void)rp1_ioctl(pio, PIO_IOC_SM_FIFO_STATE, &args);
    return args.full;
}

static uint rp1_pio_sm_get_rx_fifo_level(PIO pio, uint sm)
{
    struct rp1_pio_sm_fifo_state_args args = { .sm = sm, .tx = false };

    check_sm_param(sm);
    (void)rp1_ioctl(pio, PIO_IOC_SM_FIFO_STATE, &args);
    return args.level;
}

static bool rp1_pio_sm_is_tx_fifo_empty(PIO pio, uint sm)
{
    struct rp1_pio_sm_fifo_state_args args = { .sm = sm, .tx = true };

    check_sm_param(sm);
    (void)rp1_ioctl(pio, PIO_IOC_SM_FIFO_STATE, &args);
    return args.empty;
}

static bool rp1_pio_sm_is_tx_fifo_full(PIO pio, uint sm)
{
    struct rp1_pio_sm_fifo_state_args args = { .sm = sm, .tx = true };

    check_sm_param(sm);
    (void)rp1_ioctl(pio, PIO_IOC_SM_FIFO_STATE, &args);
    return args.full;
}

static uint rp1_pio_sm_get_tx_fifo_level(PIO pio, uint sm)
{
    struct rp1_pio_sm_fifo_state_args args = { .sm = sm, .tx = true };

    check_sm_param(sm);
    (void)rp1_ioctl(pio, PIO_IOC_SM_FIFO_STATE, &args);
    return args.level;
}

static void rp1_pio_sm_drain_tx_fifo(PIO pio, uint sm)
{
    struct rp1_pio_sm_clear_fifos_args args = { .sm = sm };

    check_sm_param(sm);
    (void)rp1_ioctl(pio, PIO_IOC_SM_DRAIN_TX, &args);
}

static void rp1_smc_set_out_pins(PIO, pio_sm_config *config, uint out_base, uint out_count)
{
    smc_to_rp1(config, c);
    valid_params_if(PIO, out_base < RP1_PIO_GPIO_COUNT);
    valid_params_if(PIO, out_count <= RP1_PIO_GPIO_COUNT);
    c->pinctrl = (c->pinctrl & ~(PROC_PIO_SM0_PINCTRL_OUT_BASE_BITS | PROC_PIO_SM0_PINCTRL_OUT_COUNT_BITS)) |
                 (out_base << PROC_PIO_SM0_PINCTRL_OUT_BASE_LSB) |
                 (out_count << PROC_PIO_SM0_PINCTRL_OUT_COUNT_LSB);
}

static void rp1_smc_set_set_pins(PIO, pio_sm_config *config, uint set_base, uint set_count)
{
    smc_to_rp1(config, c);
    valid_params_if(PIO, set_base < RP1_PIO_GPIO_COUNT);
    valid_params_if(PIO, set_count <= 5);
    c->pinctrl = (c->pinctrl & ~(PROC_PIO_SM0_PINCTRL_SET_BASE_BITS | PROC_PIO_SM0_PINCTRL_SET_COUNT_BITS)) |
                 (set_base << PROC_PIO_SM0_PINCTRL_SET_BASE_LSB) |
                 (set_count << PROC_PIO_SM0_PINCTRL_SET_COUNT_LSB);
}


static void rp1_smc_set_in_pins(PIO, pio_sm_config *config, uint in_base)
{
    smc_to_rp1(config, c);
    valid_params_if(PIO, in_base < RP1_PIO_GPIO_COUNT);
    c->pinctrl = (c->pinctrl & ~PROC_PIO_SM0_PINCTRL_IN_BASE_BITS) |
                 (in_base << PROC_PIO_SM0_PINCTRL_IN_BASE_LSB);
}

static void rp1_smc_set_sideset_pins(PIO, pio_sm_config *config, uint sideset_base)
{
    smc_to_rp1(config, c);
    valid_params_if(PIO, sideset_base < RP1_PIO_GPIO_COUNT);
    c->pinctrl = (c->pinctrl & ~PROC_PIO_SM0_PINCTRL_SIDESET_BASE_BITS) |
                 (sideset_base << PROC_PIO_SM0_PINCTRL_SIDESET_BASE_LSB);
}

static void rp1_smc_set_sideset(PIO, pio_sm_config *config, uint bit_count, bool optional, bool pindirs)
{
    smc_to_rp1(config, c);
    valid_params_if(PIO, bit_count <= 5);
    valid_params_if(PIO, !optional || bit_count >= 1);
    c->pinctrl = (c->pinctrl & ~PROC_PIO_SM0_PINCTRL_SIDESET_COUNT_BITS) |
                 (bit_count << PROC_PIO_SM0_PINCTRL_SIDESET_COUNT_LSB);

    c->execctrl = (c->execctrl & ~(PROC_PIO_SM0_EXECCTRL_SIDE_EN_BITS | PROC_PIO_SM0_EXECCTRL_SIDE_PINDIR_BITS)) |
                  (bool_to_bit(optional) << PROC_PIO_SM0_EXECCTRL_SIDE_EN_LSB) |
                  (bool_to_bit(pindirs) << PROC_PIO_SM0_EXECCTRL_SIDE_PINDIR_LSB);
}

static void rp1_smc_set_clkdiv_int_frac(PIO, pio_sm_config *config, uint16_t div_int, uint8_t div_frac)
{
    smc_to_rp1(config, c);
    invalid_params_if(PIO, div_int == 0 && div_frac != 0);
    c->clkdiv =
            (((uint)div_frac) << PROC_PIO_SM0_CLKDIV_FRAC_LSB) |
            (((uint)div_int) << PROC_PIO_SM0_CLKDIV_INT_LSB);
}

static void rp1_smc_set_clkdiv(PIO, pio_sm_config *config, float div)
{
    uint16_t div_int;
    uint8_t div_frac;
    rp1_pio_calculate_clkdiv_from_float(div, &div_int, &div_frac);
    sm_config_set_clkdiv_int_frac(config, div_int, div_frac);
}

static void rp1_smc_set_wrap(PIO, pio_sm_config *config, uint wrap_target, uint wrap)
{
    smc_to_rp1(config, c);
    valid_params_if(PIO, wrap < RP1_PIO_INSTRUCTION_COUNT);
    valid_params_if(PIO, wrap_target < RP1_PIO_INSTRUCTION_COUNT);
    c->execctrl = (c->execctrl & ~(PROC_PIO_SM0_EXECCTRL_WRAP_TOP_BITS | PROC_PIO_SM0_EXECCTRL_WRAP_BOTTOM_BITS)) |
                  (wrap_target << PROC_PIO_SM0_EXECCTRL_WRAP_BOTTOM_LSB) |
                  (wrap << PROC_PIO_SM0_EXECCTRL_WRAP_TOP_LSB);
}

static void rp1_smc_set_jmp_pin(PIO, pio_sm_config *config, uint pin)
{
    smc_to_rp1(config, c);
    valid_params_if(PIO, pin < RP1_PIO_GPIO_COUNT);
    c->execctrl = (c->execctrl & ~PROC_PIO_SM0_EXECCTRL_JMP_PIN_BITS) |
                  (pin << PROC_PIO_SM0_EXECCTRL_JMP_PIN_LSB);
}

static void rp1_smc_set_in_shift(PIO, pio_sm_config *config, bool shift_right, bool autopush, uint push_threshold)
{
    smc_to_rp1(config, c);
    valid_params_if(PIO, push_threshold <= 32);
    c->shiftctrl = (c->shiftctrl &
                    ~(PROC_PIO_SM0_SHIFTCTRL_IN_SHIFTDIR_BITS |
                      PROC_PIO_SM0_SHIFTCTRL_AUTOPUSH_BITS |
                      PROC_PIO_SM0_SHIFTCTRL_PUSH_THRESH_BITS)) |
                   (bool_to_bit(shift_right) << PROC_PIO_SM0_SHIFTCTRL_IN_SHIFTDIR_LSB) |
                   (bool_to_bit(autopush) << PROC_PIO_SM0_SHIFTCTRL_AUTOPUSH_LSB) |
                   ((push_threshold & 0x1fu) << PROC_PIO_SM0_SHIFTCTRL_PUSH_THRESH_LSB);
}

static void rp1_smc_set_out_shift(PIO, pio_sm_config *config, bool shift_right, bool autopull, uint pull_threshold)
{
    smc_to_rp1(config, c);
    valid_params_if(PIO, pull_threshold <= 32);
    c->shiftctrl = (c->shiftctrl &
                    ~(PROC_PIO_SM0_SHIFTCTRL_OUT_SHIFTDIR_BITS |
                      PROC_PIO_SM0_SHIFTCTRL_AUTOPULL_BITS |
                      PROC_PIO_SM0_SHIFTCTRL_PULL_THRESH_BITS)) |
                   (bool_to_bit(shift_right) << PROC_PIO_SM0_SHIFTCTRL_OUT_SHIFTDIR_LSB) |
                   (bool_to_bit(autopull) << PROC_PIO_SM0_SHIFTCTRL_AUTOPULL_LSB) |
                   ((pull_threshold & 0x1fu) << PROC_PIO_SM0_SHIFTCTRL_PULL_THRESH_LSB);
}

static void rp1_smc_set_fifo_join(PIO, pio_sm_config *config, enum pio_fifo_join join)
{
    smc_to_rp1(config, c);
    valid_params_if(PIO, join == PIO_FIFO_JOIN_NONE || join == PIO_FIFO_JOIN_TX || join == PIO_FIFO_JOIN_RX);
    c->shiftctrl = (c->shiftctrl & (uint)~(PROC_PIO_SM0_SHIFTCTRL_FJOIN_TX_BITS | PROC_PIO_SM0_SHIFTCTRL_FJOIN_RX_BITS)) |
                   (((uint)join) << PROC_PIO_SM0_SHIFTCTRL_FJOIN_TX_LSB);
}

static void rp1_smc_set_out_special(PIO, pio_sm_config *config, bool sticky, bool has_enable_pin, uint enable_pin_index)
{
    smc_to_rp1(config, c);
    c->execctrl = (c->execctrl &
                   (uint)~(PROC_PIO_SM0_EXECCTRL_OUT_STICKY_BITS | PROC_PIO_SM0_EXECCTRL_INLINE_OUT_EN_BITS |
                     PROC_PIO_SM0_EXECCTRL_OUT_EN_SEL_BITS)) |
                  (bool_to_bit(sticky) << PROC_PIO_SM0_EXECCTRL_OUT_STICKY_LSB) |
                  (bool_to_bit(has_enable_pin) << PROC_PIO_SM0_EXECCTRL_INLINE_OUT_EN_LSB) |
                  ((enable_pin_index << PROC_PIO_SM0_EXECCTRL_OUT_EN_SEL_LSB) & PROC_PIO_SM0_EXECCTRL_OUT_EN_SEL_BITS);
}

static void rp1_smc_set_mov_status(PIO, pio_sm_config *config, enum pio_mov_status_type status_sel, uint status_n)
{
    smc_to_rp1(config, c);
    valid_params_if(PIO, status_sel == STATUS_TX_LESSTHAN || status_sel == STATUS_RX_LESSTHAN);
    c->execctrl = (c->execctrl
                  & ~(PROC_PIO_SM0_EXECCTRL_STATUS_SEL_BITS | PROC_PIO_SM0_EXECCTRL_STATUS_N_BITS))
                  | ((((uint)status_sel) << PROC_PIO_SM0_EXECCTRL_STATUS_SEL_LSB) & PROC_PIO_SM0_EXECCTRL_STATUS_SEL_BITS)
                  | ((status_n << PROC_PIO_SM0_EXECCTRL_STATUS_N_LSB) & PROC_PIO_SM0_EXECCTRL_STATUS_N_BITS);
}

static uint32_t rp1_clock_get_hz(PIO, enum clock_index clk_index)
{
    const uint32_t MHZ = 1000000;

    switch (clk_index) {
    case clk_sys:
        return 200 * MHZ;
    default:
        break;
    }
    return PIO_ORIGIN_ANY;
}

static void rp1_gpio_init(PIO pio, uint gpio)
{
    struct rp1_gpio_init_args args = { .gpio = gpio };

    valid_params_if(PIO, gpio < RP1_PIO_GPIO_COUNT);
    (void)rp1_ioctl(pio, PIO_IOC_GPIO_INIT, &args);
}

static void rp1_gpio_set_function(PIO pio, uint gpio, enum gpio_function fn)
{
    struct rp1_gpio_set_function_args args = { .gpio = gpio, .fn = fn };

    valid_params_if(PIO, gpio < RP1_PIO_GPIO_COUNT);
    (void)rp1_ioctl(pio, PIO_IOC_GPIO_SET_FUNCTION, &args);
}

static void rp1_gpio_set_pulls(PIO pio, uint gpio, bool up, bool down)
{
    struct rp1_gpio_set_pulls_args args = { .gpio = gpio, .up = up, .down = down };

    valid_params_if(PIO, gpio < RP1_PIO_GPIO_COUNT);
    (void)rp1_ioctl(pio, PIO_IOC_GPIO_SET_PULLS, &args);
}

static void rp1_gpio_set_outover(PIO pio, uint gpio, uint value)
{
    struct rp1_gpio_set_args args = { .gpio = gpio, .value = value };

    valid_params_if(PIO, gpio < RP1_PIO_GPIO_COUNT);
    (void)rp1_ioctl(pio, PIO_IOC_GPIO_SET_OUTOVER, &args);
}

static void rp1_gpio_set_inover(PIO pio, uint gpio, uint value)
{
    struct rp1_gpio_set_args args = { .gpio = gpio, .value = value };

    valid_params_if(PIO, gpio < RP1_PIO_GPIO_COUNT);
    (void)rp1_ioctl(pio, PIO_IOC_GPIO_SET_INOVER, &args);
}

static void rp1_gpio_set_oeover(PIO pio, uint gpio, uint value)
{
    struct rp1_gpio_set_args args = { .gpio = gpio, .value = value };

    valid_params_if(PIO, gpio < RP1_PIO_GPIO_COUNT);
    (void)rp1_ioctl(pio, PIO_IOC_GPIO_SET_OEOVER, &args);
}

static void rp1_gpio_set_input_enabled(PIO pio, uint gpio, bool enabled)
{
    struct rp1_gpio_set_args args = { .gpio = gpio, .value = enabled };

    valid_params_if(PIO, gpio < RP1_PIO_GPIO_COUNT);
    (void)rp1_ioctl(pio, PIO_IOC_GPIO_SET_INPUT_ENABLED, &args);
}

static void rp1_gpio_set_drive_strength(PIO pio, uint gpio, enum gpio_drive_strength drive)
{
    struct rp1_gpio_set_args args = { .gpio = gpio, .value = drive };

    valid_params_if(PIO, gpio < RP1_PIO_GPIO_COUNT);
    (void)rp1_ioctl(pio, PIO_IOC_GPIO_SET_DRIVE_STRENGTH, &args);
}

static void rp1_pio_gpio_init(PIO pio, uint pin)
{
    valid_params_if(PIO, pin < RP1_PIO_GPIO_COUNT);
    rp1_gpio_set_function(pio, pin, RP1_GPIO_FUNC_PIO);
}

PIO rp1_create_instance(PIO_CHIP_T *chip, uint index)
{
    char pathbuf[20];
    RP1_PIO pio = NULL;

    sprintf(pathbuf, "/dev/pio%u", index);

    if (access(pathbuf, F_OK) != 0)
        return NULL;

    pio = calloc(1, sizeof(*pio));
    if (!pio)
        return PIO_ERR(-ENOMEM);

    pio->base.chip = chip;
    pio->fd = -1;
    pio->devname = strdup(pathbuf);

    rp1_pio_clear_instruction_memory(&pio->base);

    return &pio->base;
}

int rp1_open_instance(PIO pio)
{
    RP1_PIO rp = (RP1_PIO)pio;
    int fd;

    fd = open(rp->devname, O_RDWR, O_CLOEXEC);
    if (fd < 0)
        return -errno;
    rp->fd = fd;
    return 0;
}

void rp1_close_instance(PIO pio)
{
    RP1_PIO rp = (RP1_PIO)pio;
    close(rp->fd);
}

static const PIO_CHIP_T rp1_pio_chip = {
    .name = "rp1",
    .compatible = "raspberrypi,rp1-pio",
    .instr_count = RP1_PIO_INSTRUCTION_COUNT,
    .sm_count =  RP1_PIO_SM_COUNT,
    .fifo_depth = 8,

    .create_instance = rp1_create_instance,
    .open_instance = rp1_open_instance,
    .close_instance = rp1_close_instance,

    .pio_sm_config_xfer = rp1_pio_sm_config_xfer,
    .pio_sm_xfer_data = rp1_pio_sm_xfer_data,

    .pio_can_add_program_at_offset = rp1_pio_can_add_program_at_offset,
    .pio_add_program_at_offset = rp1_pio_add_program_at_offset,
    .pio_remove_program = rp1_pio_remove_program,
    .pio_clear_instruction_memory = rp1_pio_clear_instruction_memory,
    .pio_encode_delay = rp1_pio_encode_delay,
    .pio_encode_sideset = rp1_pio_encode_sideset,
    .pio_encode_sideset_opt = rp1_pio_encode_sideset_opt,
    .pio_encode_jmp = rp1_pio_encode_jmp,
    .pio_encode_jmp_not_x = rp1_pio_encode_jmp_not_x,
    .pio_encode_jmp_x_dec = rp1_pio_encode_jmp_x_dec,
    .pio_encode_jmp_not_y = rp1_pio_encode_jmp_not_y,
    .pio_encode_jmp_y_dec = rp1_pio_encode_jmp_y_dec,
    .pio_encode_jmp_x_ne_y = rp1_pio_encode_jmp_x_ne_y,
    .pio_encode_jmp_pin = rp1_pio_encode_jmp_pin,
    .pio_encode_jmp_not_osre = rp1_pio_encode_jmp_not_osre,
    .pio_encode_wait_gpio = rp1_pio_encode_wait_gpio,
    .pio_encode_wait_pin = rp1_pio_encode_wait_pin,
    .pio_encode_wait_irq = rp1_pio_encode_wait_irq,
    .pio_encode_in = rp1_pio_encode_in,
    .pio_encode_out = rp1_pio_encode_out,
    .pio_encode_push = rp1_pio_encode_push,
    .pio_encode_pull = rp1_pio_encode_pull,
    .pio_encode_mov = rp1_pio_encode_mov,
    .pio_encode_mov_not = rp1_pio_encode_mov_not,
    .pio_encode_mov_reverse = rp1_pio_encode_mov_reverse,
    .pio_encode_irq_set = rp1_pio_encode_irq_set,
    .pio_encode_irq_wait = rp1_pio_encode_irq_wait,
    .pio_encode_irq_clear = rp1_pio_encode_irq_clear,
    .pio_encode_set = rp1_pio_encode_set,
    .pio_encode_nop = rp1_pio_encode_nop,

    .pio_sm_claim = rp1_pio_sm_claim,
    .pio_sm_claim_mask = rp1_pio_sm_claim_mask,
    .pio_sm_claim_unused = rp1_pio_sm_claim_unused,
    .pio_sm_unclaim = rp1_pio_sm_unclaim,
    .pio_sm_is_claimed = rp1_pio_sm_is_claimed,

    .pio_sm_init = rp1_pio_sm_init,
    .pio_sm_set_config = rp1_pio_sm_set_config,
    .pio_sm_exec = rp1_pio_sm_exec,
    .pio_sm_clear_fifos = rp1_pio_sm_clear_fifos,
    .pio_sm_set_clkdiv_int_frac = &rp1_pio_sm_set_clkdiv_int_frac,
    .pio_sm_set_clkdiv = rp1_pio_sm_set_clkdiv,
    .pio_sm_set_pins = rp1_pio_sm_set_pins,
    .pio_sm_set_pins_with_mask = rp1_pio_sm_set_pins_with_mask,
    .pio_sm_set_pindirs_with_mask = rp1_pio_sm_set_pindirs_with_mask,
    .pio_sm_set_consecutive_pindirs = rp1_pio_sm_set_consecutive_pindirs,
    .pio_sm_set_enabled = rp1_pio_sm_set_enabled,
    .pio_sm_set_enabled_mask = rp1_pio_sm_set_enabled_mask,
    .pio_sm_restart = rp1_pio_sm_restart,
    .pio_sm_restart_mask = rp1_pio_sm_restart_mask,
    .pio_sm_clkdiv_restart = rp1_pio_sm_clkdiv_restart,
    .pio_sm_clkdiv_restart_mask = rp1_pio_sm_clkdiv_restart_mask,
    .pio_sm_enable_sync = rp1_pio_sm_enable_sync,
    .pio_sm_put = rp1_pio_sm_put,
    .pio_sm_get = rp1_pio_sm_get,
    .pio_sm_set_dmactrl = rp1_pio_sm_set_dmactrl,
    .pio_sm_is_rx_fifo_empty = rp1_pio_sm_is_rx_fifo_empty,
    .pio_sm_is_rx_fifo_full = rp1_pio_sm_is_rx_fifo_full,
    .pio_sm_get_rx_fifo_level = rp1_pio_sm_get_rx_fifo_level,
    .pio_sm_is_tx_fifo_empty = rp1_pio_sm_is_tx_fifo_empty,
    .pio_sm_is_tx_fifo_full = rp1_pio_sm_is_tx_fifo_full,
    .pio_sm_get_tx_fifo_level = rp1_pio_sm_get_tx_fifo_level,
    .pio_sm_drain_tx_fifo = rp1_pio_sm_drain_tx_fifo,

    .pio_get_default_sm_config = rp1_pio_get_default_sm_config,
    .smc_set_out_pins = rp1_smc_set_out_pins,
    .smc_set_set_pins = rp1_smc_set_set_pins,
    .smc_set_in_pins = rp1_smc_set_in_pins,
    .smc_set_sideset_pins = rp1_smc_set_sideset_pins,
    .smc_set_sideset = rp1_smc_set_sideset,
    .smc_set_clkdiv_int_frac = rp1_smc_set_clkdiv_int_frac,
    .smc_set_clkdiv = rp1_smc_set_clkdiv,
    .smc_set_wrap = rp1_smc_set_wrap,
    .smc_set_jmp_pin = rp1_smc_set_jmp_pin,
    .smc_set_in_shift = rp1_smc_set_in_shift,
    .smc_set_out_shift = rp1_smc_set_out_shift,
    .smc_set_fifo_join = rp1_smc_set_fifo_join,
    .smc_set_out_special = rp1_smc_set_out_special,
    .smc_set_mov_status = rp1_smc_set_mov_status,

    .clock_get_hz = rp1_clock_get_hz,

    .pio_gpio_init = rp1_pio_gpio_init,
    .gpio_init = rp1_gpio_init,
    .gpio_set_function = rp1_gpio_set_function,
    .gpio_set_pulls = rp1_gpio_set_pulls,
    .gpio_set_outover = rp1_gpio_set_outover,
    .gpio_set_inover = rp1_gpio_set_inover,
    .gpio_set_oeover = rp1_gpio_set_oeover,
    .gpio_set_input_enabled = rp1_gpio_set_input_enabled,
    .gpio_set_drive_strength = rp1_gpio_set_drive_strength,
};

DECLARE_PIO_CHIP(rp1_pio_chip);
