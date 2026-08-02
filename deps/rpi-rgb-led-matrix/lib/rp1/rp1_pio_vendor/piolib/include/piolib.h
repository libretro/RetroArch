// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023-24 Raspberry Pi Ltd.
 * All rights reserved.
 */

#ifndef _PIOLIB_H
#define _PIOLIB_H

#ifdef __cplusplus
extern "C" {
#endif

#include "pio_platform.h"
#include "hardware/clocks.h"
#include "hardware/gpio.h"

#ifndef PARAM_ASSERTIONS_ENABLED_PIO
#define PARAM_ASSERTIONS_ENABLED_PIO 0
#endif

#define PIO_ERR(x)((PIO)(uintptr_t)(x))
#define PIO_IS_ERR(x)(((uintptr_t)(x) >= (uintptr_t)-200))
#define PIO_ERR_VAL(x)((int)(uintptr_t)(x))

#define PIO_ORIGIN_ANY          ((uint)(~0))
#define PIO_ORIGIN_INVALID      PIO_ORIGIN_ANY

#define pio0 pio_open_helper(0)

enum pio_fifo_join {
	PIO_FIFO_JOIN_NONE = 0,
	PIO_FIFO_JOIN_TX = 1,
	PIO_FIFO_JOIN_RX = 2,
};

enum pio_mov_status_type {
	STATUS_TX_LESSTHAN = 0,
	STATUS_RX_LESSTHAN = 1
};

enum pio_xfer_dir {
    PIO_DIR_TO_SM,
    PIO_DIR_FROM_SM,
    PIO_DIR_COUNT
};

#ifndef PIOLIB_INTERNALS

enum pio_instr_bits {
    pio_instr_bits_jmp = 0x0000,
    pio_instr_bits_wait = 0x2000,
    pio_instr_bits_in = 0x4000,
    pio_instr_bits_out = 0x6000,
    pio_instr_bits_push = 0x8000,
    pio_instr_bits_pull = 0x8080,
    pio_instr_bits_mov = 0xa000,
    pio_instr_bits_irq = 0xc000,
    pio_instr_bits_set = 0xe000,
};

#ifndef NDEBUG
#define _PIO_INVALID_IN_SRC    0x08u
#define _PIO_INVALID_OUT_DEST 0x10u
#define _PIO_INVALID_SET_DEST 0x20u
#define _PIO_INVALID_MOV_SRC  0x40u
#define _PIO_INVALID_MOV_DEST 0x80u
#else
#define _PIO_INVALID_IN_SRC    0u
#define _PIO_INVALID_OUT_DEST 0u
#define _PIO_INVALID_SET_DEST 0u
#define _PIO_INVALID_MOV_SRC  0u
#define _PIO_INVALID_MOV_DEST 0u
#endif

enum pio_src_dest {
    pio_pins = 0u,
    pio_x = 1u,
    pio_y = 2u,
    pio_null = 3u | _PIO_INVALID_SET_DEST | _PIO_INVALID_MOV_DEST,
    pio_pindirs = 4u | _PIO_INVALID_IN_SRC | _PIO_INVALID_MOV_SRC | _PIO_INVALID_MOV_DEST,
    pio_exec_mov = 4u | _PIO_INVALID_IN_SRC | _PIO_INVALID_OUT_DEST | _PIO_INVALID_SET_DEST | _PIO_INVALID_MOV_SRC,
    pio_status = 5u | _PIO_INVALID_IN_SRC | _PIO_INVALID_OUT_DEST | _PIO_INVALID_SET_DEST | _PIO_INVALID_MOV_DEST,
    pio_pc = 5u | _PIO_INVALID_IN_SRC | _PIO_INVALID_SET_DEST | _PIO_INVALID_MOV_SRC,
    pio_isr = 6u | _PIO_INVALID_SET_DEST,
    pio_osr = 7u | _PIO_INVALID_OUT_DEST | _PIO_INVALID_SET_DEST,
    pio_exec_out = 7u | _PIO_INVALID_IN_SRC | _PIO_INVALID_SET_DEST | _PIO_INVALID_MOV_SRC | _PIO_INVALID_MOV_DEST,
};

#endif

typedef struct pio_program {
    const uint16_t *instructions;
    uint8_t length;
    int8_t origin; // required instruction memory origin or -1
    uint8_t pio_version;
} pio_program_t;

typedef struct {
    uint32_t content[4];
} pio_sm_config;

typedef struct pio_instance *PIO;
typedef const struct pio_chip PIO_CHIP_T;

struct pio_chip {
    const char *name;
    const char *compatible;
    uint16_t instr_count;
    uint16_t sm_count;
    uint16_t fifo_depth;
    void *hw_state;

    PIO (*create_instance)(PIO_CHIP_T *chip, uint index);
    int (*open_instance)(PIO pio);
    void (*close_instance)(PIO pio);

    int (*pio_sm_config_xfer)(PIO pio, uint sm, uint dir, uint buf_size, uint buf_count);
    int (*pio_sm_xfer_data)(PIO pio, uint sm, uint dir, uint data_bytes, void *data);

    bool (*pio_can_add_program_at_offset)(PIO pio, const pio_program_t *program, uint offset);
    uint (*pio_add_program_at_offset)(PIO pio, const pio_program_t *program, uint offset);
    bool (*pio_remove_program)(PIO pio, const pio_program_t *program, uint loaded_offset);
    bool (*pio_clear_instruction_memory)(PIO pio);
    uint (*pio_encode_delay)(PIO pio, uint cycles);
    uint (*pio_encode_sideset)(PIO pio, uint sideset_bit_count, uint value);
    uint (*pio_encode_sideset_opt)(PIO pio, uint sideset_bit_count, uint value);
    uint (*pio_encode_jmp)(PIO pio, uint addr);
    uint (*pio_encode_jmp_not_x)(PIO pio, uint addr);
    uint (*pio_encode_jmp_x_dec)(PIO pio, uint addr);
    uint (*pio_encode_jmp_not_y)(PIO pio, uint addr);
    uint (*pio_encode_jmp_y_dec)(PIO pio, uint addr);
    uint (*pio_encode_jmp_x_ne_y)(PIO pio, uint addr);
    uint (*pio_encode_jmp_pin)(PIO pio, uint addr);
    uint (*pio_encode_jmp_not_osre)(PIO pio, uint addr);
    uint (*pio_encode_wait_gpio)(PIO pio, bool polarity, uint gpio);
    uint (*pio_encode_wait_pin)(PIO pio, bool polarity, uint pin);
    uint (*pio_encode_wait_irq)(PIO pio, bool polarity, bool relative, uint irq);
    uint (*pio_encode_in)(PIO pio, enum pio_src_dest src, uint count);
    uint (*pio_encode_out)(PIO pio, enum pio_src_dest dest, uint count);
    uint (*pio_encode_push)(PIO pio, bool if_full, bool block);
    uint (*pio_encode_pull)(PIO pio, bool if_empty, bool block);
    uint (*pio_encode_mov)(PIO pio, enum pio_src_dest dest, enum pio_src_dest src);
    uint (*pio_encode_mov_not)(PIO pio, enum pio_src_dest dest, enum pio_src_dest src);
    uint (*pio_encode_mov_reverse)(PIO pio, enum pio_src_dest dest, enum pio_src_dest src);
    uint (*pio_encode_irq_set)(PIO pio, bool relative, uint irq);
    uint (*pio_encode_irq_wait)(PIO pio, bool relative, uint irq);
    uint (*pio_encode_irq_clear)(PIO pio, bool relative, uint irq);
    uint (*pio_encode_set)(PIO pio, enum pio_src_dest dest, uint value);
    uint (*pio_encode_nop)(PIO pio);

    bool (*pio_sm_claim)(PIO pio, uint sm);
    bool (*pio_sm_claim_mask)(PIO pio, uint mask);
    int (*pio_sm_claim_unused)(PIO pio, bool required);
    bool (*pio_sm_unclaim)(PIO pio, uint sm);
    bool (*pio_sm_is_claimed)(PIO pio, uint sm);

    void (*pio_sm_init)(PIO pio, uint sm, uint initial_pc, const pio_sm_config *config);
    void (*pio_sm_set_config)(PIO pio, uint sm, const pio_sm_config *config);
    void (*pio_sm_exec)(PIO pio, uint sm, uint instr, bool blocking);
    void (*pio_sm_clear_fifos)(PIO pio, uint sm);
    void (*pio_sm_set_clkdiv_int_frac)(PIO pio, uint sm, uint16_t div_int, uint8_t div_frac);
    void (*pio_sm_set_clkdiv)(PIO pio, uint sm, float div);
    void (*pio_sm_set_pins)(PIO pio, uint sm, uint32_t pin_values);
    void (*pio_sm_set_pins_with_mask)(PIO pio, uint sm, uint32_t pin_values, uint32_t pin_mask);
    void (*pio_sm_set_pindirs_with_mask)(PIO pio, uint sm, uint32_t pin_dirs, uint32_t pin_mask);
    void (*pio_sm_set_consecutive_pindirs)(PIO pio, uint sm, uint pin_base, uint pin_count, bool is_out);
    void (*pio_sm_set_enabled)(PIO pio, uint sm, bool enabled);
    void (*pio_sm_set_enabled_mask)(PIO pio, uint32_t mask, bool enabled);
    void (*pio_sm_restart)(PIO pio, uint sm);
    void (*pio_sm_restart_mask)(PIO pio, uint32_t mask);
    void (*pio_sm_clkdiv_restart)(PIO pio, uint sm);
    void (*pio_sm_clkdiv_restart_mask)(PIO pio, uint32_t mask);
    void (*pio_sm_enable_sync)(PIO pio, uint32_t mask);
    void (*pio_sm_put)(PIO pio, uint sm, uint32_t data, bool blocking);
    uint32_t (*pio_sm_get)(PIO pio, uint sm, bool blocking);
    void (*pio_sm_set_dmactrl)(PIO pio, uint sm, bool is_tx, uint32_t ctrl);
    bool (*pio_sm_is_rx_fifo_empty)(PIO pio, uint sm);
    bool (*pio_sm_is_rx_fifo_full)(PIO pio, uint sm);
    uint (*pio_sm_get_rx_fifo_level)(PIO pio, uint sm);
    bool (*pio_sm_is_tx_fifo_empty)(PIO pio, uint sm);
    bool (*pio_sm_is_tx_fifo_full)(PIO pio, uint sm);
    uint (*pio_sm_get_tx_fifo_level)(PIO pio, uint sm);
    void (*pio_sm_drain_tx_fifo)(PIO pio, uint sm);

    pio_sm_config (*pio_get_default_sm_config)(PIO pio);
    void (*smc_set_out_pins)(PIO pio, pio_sm_config *c, uint out_base, uint out_count);
    void (*smc_set_set_pins)(PIO pio, pio_sm_config *c, uint set_base, uint set_count);
    void (*smc_set_in_pins)(PIO pio, pio_sm_config *c, uint in_base);
    void (*smc_set_sideset_pins)(PIO pio, pio_sm_config *c, uint sideset_base);
    void (*smc_set_sideset)(PIO pio, pio_sm_config *c, uint bit_count, bool optional, bool pindirs);
    void (*smc_set_clkdiv_int_frac)(PIO pio, pio_sm_config *c, uint16_t div_int, uint8_t div_frac);
    void (*smc_set_clkdiv)(PIO pio, pio_sm_config *c, float div);
    void (*smc_set_wrap)(PIO pio, pio_sm_config *c, uint wrap_target, uint wrap);
    void (*smc_set_jmp_pin)(PIO pio, pio_sm_config *c, uint pin);
    void (*smc_set_in_shift)(PIO pio, pio_sm_config *c, bool shift_right, bool autopush, uint push_threshold);
    void (*smc_set_out_shift)(PIO pio, pio_sm_config *c, bool shift_right, bool autopull, uint pull_threshold);
    void (*smc_set_fifo_join)(PIO pio, pio_sm_config *c, enum pio_fifo_join join);
    void (*smc_set_out_special)(PIO pio, pio_sm_config *c, bool sticky, bool has_enable_pin, uint enable_pin_index);
    void (*smc_set_mov_status)(PIO pio, pio_sm_config *c, enum pio_mov_status_type status_sel, uint status_n);

    uint32_t (*clock_get_hz)(PIO pio, enum clock_index clk_index);
    void (*pio_gpio_init)(PIO, uint pin);
    void (*gpio_init)(PIO pio, uint gpio);
    void (*gpio_set_function)(PIO pio, uint gpio, enum gpio_function fn);
    void (*gpio_set_pulls)(PIO pio, uint gpio, bool up, bool down);
    void (*gpio_set_outover)(PIO pio, uint gpio, uint value);
    void (*gpio_set_inover)(PIO pio, uint gpio, uint value);
    void (*gpio_set_oeover)(PIO pio, uint gpio, uint value);
    void (*gpio_set_input_enabled)(PIO pio, uint gpio, bool enabled);
    void (*gpio_set_drive_strength)(PIO pio, uint gpio, enum gpio_drive_strength drive);
};

struct pio_instance {
    const PIO_CHIP_T *chip;
    int in_use;
    bool errors_are_fatal;
    bool error;
};

int pio_init(void);
PIO pio_open(uint idx);
PIO pio_open_by_name(const char *name);
PIO pio_open_helper(uint idx);
void pio_close(PIO pio);
void pio_panic(const char *msg);
int pio_get_index(PIO pio);
void pio_select(PIO pio);
PIO pio_get_current(void);

static inline void pio_error(PIO pio, const char *msg)
{
    pio->error = true;
    if (pio->errors_are_fatal)
        pio_panic(msg);
}

static inline bool pio_get_error(PIO pio)
{
    return pio->error;
}

static inline void pio_clear_error(PIO pio)
{
    pio->error = false;
}

static inline void pio_enable_fatal_errors(PIO pio, bool enable)
{
    pio->errors_are_fatal = enable;
}

static inline uint pio_get_sm_count(PIO pio)
{
    return pio->chip->sm_count;
}

static inline uint pio_get_instruction_count(PIO pio)
{
    return pio->chip->instr_count;
}

static inline uint pio_get_fifo_depth(PIO pio)
{
    return pio->chip->fifo_depth;
}

static inline void check_pio_param(__unused PIO pio)
{
    valid_params_if(PIO, pio_get_index(pio) >= 0);
}

static inline int pio_sm_config_xfer(PIO pio, uint sm, uint dir, uint buf_size, uint buf_count)
{
    check_pio_param(pio);
    return pio->chip->pio_sm_config_xfer(pio, sm, dir, buf_size, buf_count);
}

static inline int pio_sm_xfer_data(PIO pio, uint sm, uint dir, uint data_bytes, void *data)
{
    check_pio_param(pio);
    return pio->chip->pio_sm_xfer_data(pio, sm, dir, data_bytes, data);
}

static inline bool pio_can_add_program(PIO pio, const pio_program_t *program)
{
    check_pio_param(pio);
    return pio->chip->pio_can_add_program_at_offset(pio, program, PIO_ORIGIN_ANY);
}

static inline bool pio_can_add_program_at_offset(PIO pio, const pio_program_t *program, uint offset)
{
    check_pio_param(pio);
    return pio->chip->pio_can_add_program_at_offset(pio, program, offset);
}

static inline uint pio_add_program(PIO pio, const pio_program_t *program)
{
    uint offset;
    check_pio_param(pio);
    offset = pio->chip->pio_add_program_at_offset(pio, program, PIO_ORIGIN_ANY);
    if (offset == PIO_ORIGIN_INVALID)
        pio_error(pio, "No program space");
    return offset;
}

static inline void pio_add_program_at_offset(PIO pio, const pio_program_t *program, uint offset)
{
    check_pio_param(pio);
    if (pio->chip->pio_add_program_at_offset(pio, program, offset) == PIO_ORIGIN_INVALID)
        pio_error(pio, "No program space");
}

static inline void pio_remove_program(PIO pio, const pio_program_t *program, uint loaded_offset)
{
    check_pio_param(pio);
    if (!pio->chip->pio_remove_program(pio, program, loaded_offset))
        pio_error(pio, "Failed to remove program");
}

static inline void pio_clear_instruction_memory(PIO pio)
{
    check_pio_param(pio);
    if (!pio->chip->pio_clear_instruction_memory(pio))
        pio_error(pio, "Failed to clear instruction memory");
}

static inline uint pio_encode_delay(uint cycles)
{
    PIO pio = pio_get_current();
    return pio->chip->pio_encode_delay(pio, cycles);
}

static inline uint pio_encode_sideset(uint sideset_bit_count, uint value)
{
    PIO pio = pio_get_current();
    return pio->chip->pio_encode_sideset(pio, sideset_bit_count, value);
}

static inline uint pio_encode_sideset_opt(uint sideset_bit_count, uint value)
{
    PIO pio = pio_get_current();
    return pio->chip->pio_encode_sideset_opt(pio, sideset_bit_count, value);
}

static inline uint pio_encode_jmp(uint addr)
{
    PIO pio = pio_get_current();
    return pio->chip->pio_encode_jmp(pio, addr);
}

static inline uint pio_encode_jmp_not_x(uint addr)
{
    PIO pio = pio_get_current();
    return pio->chip->pio_encode_jmp_not_x(pio, addr);
}

static inline uint pio_encode_jmp_x_dec(uint addr)
{
    PIO pio = pio_get_current();
    return pio->chip->pio_encode_jmp_x_dec(pio, addr);
}

static inline uint pio_encode_jmp_not_y(uint addr)
{
    PIO pio = pio_get_current();
    return pio->chip->pio_encode_jmp_not_y(pio, addr);
}

static inline uint pio_encode_jmp_y_dec(uint addr)
{
    PIO pio = pio_get_current();
    return pio->chip->pio_encode_jmp_y_dec(pio, addr);
}

static inline uint pio_encode_jmp_x_ne_y(uint addr)
{
    PIO pio = pio_get_current();
    return pio->chip->pio_encode_jmp_x_ne_y(pio, addr);
}

static inline uint pio_encode_jmp_pin(uint addr)
{
    PIO pio = pio_get_current();
    return pio->chip->pio_encode_jmp_pin(pio, addr);
}

static inline uint pio_encode_jmp_not_osre(uint addr)
{
    PIO pio = pio_get_current();
    return pio->chip->pio_encode_jmp_not_osre(pio, addr);
}

static inline uint pio_encode_wait_gpio(bool polarity, uint gpio)
{
    PIO pio = pio_get_current();
    return pio->chip->pio_encode_wait_gpio(pio, polarity, gpio);
}

static inline uint pio_encode_wait_pin(bool polarity, uint pin)
{
    PIO pio = pio_get_current();
    return pio->chip->pio_encode_wait_pin(pio, polarity, pin);
}

static inline uint pio_encode_wait_irq(bool polarity, bool relative, uint irq)
{
    PIO pio = pio_get_current();
    return pio->chip->pio_encode_wait_irq(pio, polarity, relative, irq);
}

static inline uint pio_encode_in(enum pio_src_dest src, uint count)
{
    PIO pio = pio_get_current();
    return pio->chip->pio_encode_in(pio, src, count);
}

static inline uint pio_encode_out(enum pio_src_dest dest, uint count)
{
    PIO pio = pio_get_current();
    return pio->chip->pio_encode_out(pio, dest, count);
}

static inline uint pio_encode_push(bool if_full, bool block)
{
    PIO pio = pio_get_current();
    return pio->chip->pio_encode_push(pio, if_full, block);
}

static inline uint pio_encode_pull(bool if_empty, bool block)
{
    PIO pio = pio_get_current();
    return pio->chip->pio_encode_pull(pio, if_empty, block);
}

static inline uint pio_encode_mov(enum pio_src_dest dest, enum pio_src_dest src)
{
    PIO pio = pio_get_current();
    return pio->chip->pio_encode_mov(pio, dest, src);
}

static inline uint pio_encode_mov_not(enum pio_src_dest dest, enum pio_src_dest src)
{
    PIO pio = pio_get_current();
    return pio->chip->pio_encode_mov_not(pio, dest, src);
}

static inline uint pio_encode_mov_reverse(enum pio_src_dest dest, enum pio_src_dest src)
{
    PIO pio = pio_get_current();
    return pio->chip->pio_encode_mov_reverse(pio, dest, src);
}

static inline uint pio_encode_irq_set(bool relative, uint irq)
{
    PIO pio = pio_get_current();
    return pio->chip->pio_encode_irq_set(pio, relative, irq);
}

static inline uint pio_encode_irq_wait(bool relative, uint irq)
{
    PIO pio = pio_get_current();
    return pio->chip->pio_encode_irq_wait(pio, relative, irq);
}

static inline uint pio_encode_irq_clear(bool relative, uint irq)
{
    PIO pio = pio_get_current();
    return pio->chip->pio_encode_irq_clear(pio, relative, irq);
}

static inline uint pio_encode_set(enum pio_src_dest dest, uint value)
{
    PIO pio = pio_get_current();
    return pio->chip->pio_encode_set(pio, dest, value);
}

static inline uint pio_encode_nop(void)
{
    PIO pio = pio_get_current();
    return pio->chip->pio_encode_nop(pio);
}

static inline void pio_sm_claim(PIO pio, uint sm)
{
    check_pio_param(pio);
    if (!pio->chip->pio_sm_claim(pio, sm))
        pio_error(pio, "Failed to claim SM");
}

static inline void pio_claim_sm_mask(PIO pio, uint mask)
{
    check_pio_param(pio);
    if (!pio->chip->pio_sm_claim_mask(pio, mask))
        pio_error(pio, "Failed to claim masked SMs");
}

static inline void pio_sm_unclaim(PIO pio, uint sm)
{
    check_pio_param(pio);
    pio->chip->pio_sm_unclaim(pio, sm);
}

static inline int pio_claim_unused_sm(PIO pio, bool required)
{
    check_pio_param(pio);
    return pio->chip->pio_sm_claim_unused(pio, required);
}

static inline bool pio_sm_is_claimed(PIO pio, uint sm)
{
    check_pio_param(pio);
    return pio->chip->pio_sm_is_claimed(pio, sm);
}

static inline void pio_sm_init(PIO pio, uint sm, uint initial_pc, const pio_sm_config *config)
{
    check_pio_param(pio);
    pio->chip->pio_sm_init(pio, sm, initial_pc, config);
}

static inline void pio_sm_set_config(PIO pio, uint sm, const pio_sm_config *config)
{
    check_pio_param(pio);
    pio->chip->pio_sm_set_config(pio, sm, config);
}

static inline void pio_sm_exec(PIO pio, uint sm, uint instr)
{
    check_pio_param(pio);
    pio->chip->pio_sm_exec(pio, sm, instr, false);
}

static inline void pio_sm_exec_wait_blocking(PIO pio, uint sm, uint instr)
{
    check_pio_param(pio);
    pio->chip->pio_sm_exec(pio, sm, instr, true);
}

static inline void pio_sm_clear_fifos(PIO pio, uint sm)
{
    check_pio_param(pio);
    pio->chip->pio_sm_clear_fifos(pio, sm);
}

static inline void pio_sm_set_clkdiv_int_frac(PIO pio, uint sm, uint16_t div_int, uint8_t div_frac)
{
    check_pio_param(pio);
    pio->chip->pio_sm_set_clkdiv_int_frac(pio, sm, div_int, div_frac);
}

static inline void pio_sm_set_clkdiv(PIO pio, uint sm, float div)
{
    check_pio_param(pio);
    pio->chip->pio_sm_set_clkdiv(pio, sm, div);
}

static inline void pio_sm_set_pins(PIO pio, uint sm, uint32_t pin_values)
{
    check_pio_param(pio);
    pio->chip->pio_sm_set_pins(pio, sm, pin_values);
}

static inline void pio_sm_set_pins_with_mask(PIO pio, uint sm, uint32_t pin_values, uint32_t pin_mask)
{
    check_pio_param(pio);
    pio->chip->pio_sm_set_pins_with_mask(pio, sm, pin_values, pin_mask);
}

static inline void pio_sm_set_pindirs_with_mask(PIO pio, uint sm, uint32_t pin_dirs, uint32_t pin_mask)
{
    check_pio_param(pio);
    pio->chip->pio_sm_set_pindirs_with_mask(pio, sm, pin_dirs, pin_mask);
}

static inline void pio_sm_set_consecutive_pindirs(PIO pio, uint sm, uint pin_base, uint pin_count, bool is_out)
{
    check_pio_param(pio);
    pio->chip->pio_sm_set_consecutive_pindirs(pio, sm, pin_base, pin_count, is_out);
}

static inline void pio_sm_set_enabled(PIO pio, uint sm, bool enabled)
{
    check_pio_param(pio);
    pio->chip->pio_sm_set_enabled(pio, sm, enabled);
}

static inline void pio_set_sm_mask_enabled(PIO pio, uint32_t mask, bool enabled)
{
    check_pio_param(pio);
    pio->chip->pio_sm_set_enabled_mask(pio, mask, enabled);
}

static inline void pio_sm_restart(PIO pio, uint sm)
{
    check_pio_param(pio);
    pio->chip->pio_sm_restart(pio, sm);
}

static inline void pio_restart_sm_mask(PIO pio, uint32_t mask)
{
    check_pio_param(pio);
    pio->chip->pio_sm_restart_mask(pio, mask);
}

static inline void pio_sm_clkdiv_restart(PIO pio, uint sm)
{
    check_pio_param(pio);
    pio->chip->pio_sm_clkdiv_restart(pio, sm);
}

static inline void pio_clkdiv_restart_sm_mask(PIO pio, uint32_t mask)
{
    check_pio_param(pio);
    pio->chip->pio_sm_clkdiv_restart_mask(pio, mask);
}

static inline void pio_enable_sm_in_sync_mask(PIO pio, uint32_t mask)
{
    check_pio_param(pio);
    pio->chip->pio_sm_enable_sync(pio, mask);
};

static inline void pio_sm_set_dmactrl(PIO pio, uint sm, bool is_tx, uint32_t ctrl)
{
    check_pio_param(pio);
    pio->chip->pio_sm_set_dmactrl(pio, sm, is_tx, ctrl);
};

static inline bool pio_sm_is_rx_fifo_empty(PIO pio, uint sm)
{
    check_pio_param(pio);
    return pio->chip->pio_sm_is_rx_fifo_empty(pio, sm);
}

static inline bool pio_sm_is_rx_fifo_full(PIO pio, uint sm)
{
    check_pio_param(pio);
    return pio->chip->pio_sm_is_rx_fifo_full(pio, sm);
}

static inline uint pio_sm_get_rx_fifo_level(PIO pio, uint sm)
{
    check_pio_param(pio);
    return pio->chip->pio_sm_get_rx_fifo_level(pio, sm);
}

static inline bool pio_sm_is_tx_fifo_empty(PIO pio, uint sm)
{
    check_pio_param(pio);
    return pio->chip->pio_sm_is_tx_fifo_empty(pio, sm);
}

static inline bool pio_sm_is_tx_fifo_full(PIO pio, uint sm)
{
    check_pio_param(pio);
    return pio->chip->pio_sm_is_tx_fifo_full(pio, sm);
}

static inline uint pio_sm_get_tx_fifo_level(PIO pio, uint sm)
{
    check_pio_param(pio);
    return pio->chip->pio_sm_get_tx_fifo_level(pio, sm);
}

static inline void pio_sm_drain_tx_fifo(PIO pio, uint sm)
{
    check_pio_param(pio);
    return pio->chip->pio_sm_drain_tx_fifo(pio, sm);
}

static inline void pio_sm_put(PIO pio, uint sm, uint32_t data)
{
    check_pio_param(pio);
    pio->chip->pio_sm_put(pio, sm, data, false);
}

static inline void pio_sm_put_blocking(PIO pio, uint sm, uint32_t data)
{
    check_pio_param(pio);
    pio->chip->pio_sm_put(pio, sm, data, true);
}

static inline uint32_t pio_sm_get(PIO pio, uint sm)
{
    check_pio_param(pio);
    return pio->chip->pio_sm_get(pio, sm, false);
}

static inline uint32_t pio_sm_get_blocking(PIO pio, uint sm)
{
    check_pio_param(pio);
    return pio->chip->pio_sm_get(pio, sm, true);
}

static inline pio_sm_config pio_get_default_sm_config_for_pio(PIO pio)
{
    check_pio_param(pio);
    return pio->chip->pio_get_default_sm_config(pio);
}

static inline pio_sm_config pio_get_default_sm_config(void)
{
    PIO pio = pio_get_current();
    return pio->chip->pio_get_default_sm_config(pio);
}

static inline void sm_config_set_out_pins(pio_sm_config *c, uint out_base, uint out_count)
{
    PIO pio = pio_get_current();
    pio->chip->smc_set_out_pins(pio, c, out_base, out_count);
}

static inline void sm_config_set_set_pins(pio_sm_config *c, uint set_base, uint set_count)
{
    PIO pio = pio_get_current();
    pio->chip->smc_set_set_pins(pio, c, set_base, set_count);
}

static inline void sm_config_set_in_pins(pio_sm_config *c, uint in_base)
{
    PIO pio = pio_get_current();
    pio->chip->smc_set_in_pins(pio, c, in_base);
}

static inline void sm_config_set_sideset_pins(pio_sm_config *c, uint sideset_base)
{
    PIO pio = pio_get_current();
    pio->chip->smc_set_sideset_pins(pio, c, sideset_base);
}

static inline void sm_config_set_sideset(pio_sm_config *c, uint bit_count, bool optional, bool pindirs)
{
    PIO pio = pio_get_current();
    pio->chip->smc_set_sideset(pio, c, bit_count, optional, pindirs);
}

static inline void sm_config_set_clkdiv_int_frac(pio_sm_config *c, uint16_t div_int, uint8_t div_frac)
{
    PIO pio = pio_get_current();
    pio->chip->smc_set_clkdiv_int_frac(pio, c, div_int, div_frac);
}

static inline void sm_config_set_clkdiv(pio_sm_config *c, float div)
{
    PIO pio = pio_get_current();
    pio->chip->smc_set_clkdiv(pio, c, div);
}

static inline void sm_config_set_wrap(pio_sm_config *c, uint wrap_target, uint wrap)
{
    PIO pio = pio_get_current();
    pio->chip->smc_set_wrap(pio, c, wrap_target, wrap);
}

static inline void sm_config_set_jmp_pin(pio_sm_config *c, uint pin)
{
    PIO pio = pio_get_current();
    pio->chip->smc_set_jmp_pin(pio, c, pin);
}

static inline void sm_config_set_in_shift(pio_sm_config *c, bool shift_right, bool autopush, uint push_threshold)
{
    PIO pio = pio_get_current();
    pio->chip->smc_set_in_shift(pio, c, shift_right, autopush, push_threshold);
}

static inline void sm_config_set_out_shift(pio_sm_config *c, bool shift_right, bool autopull, uint pull_threshold)
{
    PIO pio = pio_get_current();
    pio->chip->smc_set_out_shift(pio, c, shift_right, autopull, pull_threshold);
}

static inline void sm_config_set_fifo_join(pio_sm_config *c, enum pio_fifo_join join)
{
    PIO pio = pio_get_current();
    pio->chip->smc_set_fifo_join(pio, c, join);
}

static inline void sm_config_set_out_special(pio_sm_config *c, bool sticky, bool has_enable_pin, uint enable_pin_index)
{
    PIO pio = pio_get_current();
    pio->chip->smc_set_out_special(pio, c, sticky, has_enable_pin, enable_pin_index);
}

static inline void sm_config_set_mov_status(pio_sm_config *c, enum pio_mov_status_type status_sel, uint status_n)
{
    PIO pio = pio_get_current();
    pio->chip->smc_set_mov_status(pio, c, status_sel, status_n);
}

static inline void pio_gpio_init(PIO pio, uint pin)
{
    check_pio_param(pio);
    pio->chip->pio_gpio_init(pio, pin);
}

static inline uint32_t clock_get_hz(enum clock_index clk_index)
{
    PIO pio = pio_get_current();
    return pio->chip->clock_get_hz(pio, clk_index);
}

static inline void gpio_init(uint gpio)
{
    PIO pio = pio_get_current();
    pio->chip->gpio_init(pio, gpio);
}

static inline void gpio_set_function(uint gpio, enum gpio_function fn)
{
    PIO pio = pio_get_current();
    pio->chip->gpio_set_function(pio, gpio, fn);
}


static inline void gpio_set_pulls(uint gpio, bool up, bool down)
{
    PIO pio = pio_get_current();
    pio->chip->gpio_set_pulls(pio, gpio, up, down);
}

static inline void gpio_set_outover(uint gpio, uint value)
{
    PIO pio = pio_get_current();
    pio->chip->gpio_set_outover(pio, gpio, value);
}

static inline void gpio_set_inover(uint gpio, uint value)
{
    PIO pio = pio_get_current();
    pio->chip->gpio_set_inover(pio, gpio, value);
}

static inline void gpio_set_oeover(uint gpio, uint value)
{
    PIO pio = pio_get_current();
    pio->chip->gpio_set_oeover(pio, gpio, value);
}

static inline void gpio_set_input_enabled(uint gpio, bool enabled)
{
    PIO pio = pio_get_current();
    pio->chip->gpio_set_input_enabled(pio, gpio, enabled);
}

static inline void gpio_set_drive_strength(uint gpio, enum gpio_drive_strength drive)
{
    PIO pio = pio_get_current();
    pio->chip->gpio_set_drive_strength(pio, gpio, drive);
}

static inline void gpio_pull_up(uint gpio) {
    gpio_set_pulls(gpio, true, false);
}

static inline void gpio_pull_down(uint gpio) {
    gpio_set_pulls(gpio, false, true);
}

static inline void gpio_disable_pulls(uint gpio) {
    gpio_set_pulls(gpio, false, false);
}

static inline void stdio_init_all(void)
{
}

void sleep_us(uint64_t us);

static inline void sleep_ms(uint32_t ms) {
    sleep_us((uint64_t)(ms * (uint64_t)1000));
}

#ifdef __cplusplus
}
#endif

#endif
