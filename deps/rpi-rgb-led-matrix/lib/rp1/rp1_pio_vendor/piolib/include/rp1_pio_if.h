// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023-24 Raspberry Pi Ltd.
 * All rights reserved.
 */
#ifndef _RP1_PIO_IF_H
#define _RP1_PIO_IF_H

#include <sys/ioctl.h>

#define RP1_PIO_INSTRUCTION_COUNT   32
#define RP1_PIO_SM_COUNT            4
#define RP1_PIO_GPIO_COUNT          28
#define RP1_GPIO_FUNC_PIO           7

#define RP1_PIO_ORIGIN_ANY          ((uint16_t)(~0))

#define RP1_PIO_DIR_TO_SM           0
#define RP1_PIO_DIR_FROM_SM         1
#define RP1_PIO_DIR_COUNT           2

typedef struct {
    uint32_t clkdiv;
    uint32_t execctrl;
    uint32_t shiftctrl;
    uint32_t pinctrl;
} rp1_pio_sm_config;

struct rp1_pio_add_program_args {
    uint16_t num_instrs;
    uint16_t origin;
    uint16_t instrs[RP1_PIO_INSTRUCTION_COUNT];
};

struct rp1_pio_remove_program_args {
    uint16_t num_instrs;
    uint16_t origin;
};

struct rp1_pio_sm_claim_args {
    uint16_t mask;
};

struct rp1_pio_sm_init_args {
    uint16_t sm;
    uint16_t initial_pc;
    rp1_pio_sm_config config;
};

struct rp1_pio_sm_set_config_args {
    uint16_t sm;
    uint16_t rsvd;
    rp1_pio_sm_config config;
};

struct rp1_pio_sm_exec_args {
    uint16_t sm;
    uint16_t instr;
    uint8_t blocking;
    uint8_t rsvd;
};

struct rp1_pio_sm_clear_fifos_args {
    uint16_t sm;
};

struct rp1_pio_sm_set_clkdiv_args {
    uint16_t sm;
    uint16_t div_int;
    uint8_t div_frac;
    uint8_t rsvd;
};

struct rp1_pio_sm_set_pins_args {
    uint16_t sm;
    uint16_t rsvd;
    uint32_t values;
    uint32_t mask;
};

struct rp1_pio_sm_set_pindirs_args {
    uint16_t sm;
    uint16_t rsvd;
    uint32_t dirs;
    uint32_t mask;
};

struct rp1_pio_sm_set_enabled_args {
    uint16_t mask;
    uint8_t enable;
    uint8_t rsvd;
};

struct rp1_pio_sm_restart_args {
    uint16_t mask;
};

struct rp1_pio_sm_clkdiv_restart_args {
    uint16_t mask;
};

struct rp1_pio_sm_enable_sync_args {
    uint16_t mask;
};

struct rp1_pio_sm_put_args {
    uint16_t sm;
    uint8_t blocking;
    uint8_t rsvd;
    uint32_t data;
};

struct rp1_pio_sm_get_args {
    uint16_t sm;
    uint8_t blocking;
    uint8_t rsvd;
    uint32_t data; /* OUT */
};

struct rp1_pio_sm_set_dmactrl_args {
    uint16_t sm;
    uint8_t is_tx;
    uint8_t rsvd;
    uint32_t ctrl;
};

struct rp1_pio_sm_fifo_state_args {
	uint16_t sm;
	uint8_t tx;
	uint8_t rsvd;
	uint16_t level; /* OUT */
	uint8_t empty; /* OUT */
	uint8_t full; /* OUT */
};

struct rp1_gpio_init_args {
    uint16_t gpio;
};

struct rp1_gpio_set_function_args {
    uint16_t gpio;
    uint16_t fn;
};

struct rp1_gpio_set_pulls_args {
    uint16_t gpio;
    uint8_t up;
    uint8_t down;
};

struct rp1_gpio_set_args {
    uint16_t gpio;
    uint16_t value;
};

struct rp1_pio_sm_config_xfer_args {
    uint16_t sm;
    uint16_t dir;
    uint16_t buf_size;
    uint16_t buf_count;
};

struct rp1_pio_sm_config_xfer32_args {
    uint16_t sm;
    uint16_t dir;
    uint32_t buf_size;
    uint32_t buf_count;
};

struct rp1_pio_sm_xfer_data_args {
    uint16_t sm;
    uint16_t dir;
    uint16_t data_bytes;
    uint16_t rsvd;
    void *data;
};

struct rp1_pio_sm_xfer_data32_args {
    uint16_t sm;
    uint16_t dir;
    uint32_t data_bytes;
    void *data;
};

struct rp1_access_hw_args {
    uint32_t addr;
    uint32_t len;
    void *data;
};

#define PIO_IOC_MAGIC 102

#define PIO_IOC_SM_CONFIG_XFER _IOW(PIO_IOC_MAGIC, 0, struct rp1_pio_sm_config_xfer_args)
#define PIO_IOC_SM_XFER_DATA _IOW(PIO_IOC_MAGIC, 1, struct rp1_pio_sm_xfer_data_args)
#define PIO_IOC_SM_XFER_DATA32 _IOW(PIO_IOC_MAGIC, 2, struct rp1_pio_sm_xfer_data32_args)
#define PIO_IOC_SM_CONFIG_XFER32 _IOW(PIO_IOC_MAGIC, 3, struct rp1_pio_sm_config_xfer32_args)

#define PIO_IOC_READ_HW _IOW(PIO_IOC_MAGIC, 8, struct rp1_access_hw_args)
#define PIO_IOC_WRITE_HW _IOW(PIO_IOC_MAGIC, 9, struct rp1_access_hw_args)

#define PIO_IOC_CAN_ADD_PROGRAM _IOW(PIO_IOC_MAGIC, 10, struct rp1_pio_add_program_args)
#define PIO_IOC_ADD_PROGRAM _IOW(PIO_IOC_MAGIC, 11, struct rp1_pio_add_program_args)
#define PIO_IOC_REMOVE_PROGRAM _IOW(PIO_IOC_MAGIC, 12, struct rp1_pio_remove_program_args)
#define PIO_IOC_CLEAR_INSTR_MEM _IO(PIO_IOC_MAGIC, 13)

#define PIO_IOC_SM_CLAIM _IOW(PIO_IOC_MAGIC, 20, struct rp1_pio_sm_claim_args)
#define PIO_IOC_SM_UNCLAIM _IOW(PIO_IOC_MAGIC, 21, struct rp1_pio_sm_claim_args)
#define PIO_IOC_SM_IS_CLAIMED _IOW(PIO_IOC_MAGIC, 22, struct rp1_pio_sm_claim_args)

#define PIO_IOC_SM_INIT _IOW(PIO_IOC_MAGIC, 30, struct rp1_pio_sm_init_args)
#define PIO_IOC_SM_SET_CONFIG _IOW(PIO_IOC_MAGIC, 31, struct rp1_pio_sm_set_config_args)
#define PIO_IOC_SM_EXEC _IOW(PIO_IOC_MAGIC, 32, struct rp1_pio_sm_exec_args)
#define PIO_IOC_SM_CLEAR_FIFOS _IOW(PIO_IOC_MAGIC, 33, struct rp1_pio_sm_clear_fifos_args)
#define PIO_IOC_SM_SET_CLKDIV _IOW(PIO_IOC_MAGIC, 34, struct rp1_pio_sm_set_clkdiv_args)
#define PIO_IOC_SM_SET_PINS _IOW(PIO_IOC_MAGIC, 35, struct rp1_pio_sm_set_pins_args)
#define PIO_IOC_SM_SET_PINDIRS _IOW(PIO_IOC_MAGIC, 36, struct rp1_pio_sm_set_pindirs_args)
#define PIO_IOC_SM_SET_ENABLED _IOW(PIO_IOC_MAGIC, 37, struct rp1_pio_sm_set_enabled_args)
#define PIO_IOC_SM_RESTART _IOW(PIO_IOC_MAGIC, 38, struct rp1_pio_sm_restart_args)
#define PIO_IOC_SM_CLKDIV_RESTART _IOW(PIO_IOC_MAGIC, 39, struct rp1_pio_sm_restart_args)
#define PIO_IOC_SM_ENABLE_SYNC _IOW(PIO_IOC_MAGIC, 40, struct rp1_pio_sm_enable_sync_args)
#define PIO_IOC_SM_PUT _IOW(PIO_IOC_MAGIC, 41, struct rp1_pio_sm_put_args)
#define PIO_IOC_SM_GET _IOWR(PIO_IOC_MAGIC, 42, struct rp1_pio_sm_get_args)
#define PIO_IOC_SM_SET_DMACTRL _IOW(PIO_IOC_MAGIC, 43, struct rp1_pio_sm_set_dmactrl_args)
#define PIO_IOC_SM_FIFO_STATE _IOW(PIO_IOC_MAGIC, 44, struct rp1_pio_sm_fifo_state_args)
#define PIO_IOC_SM_DRAIN_TX _IOW(PIO_IOC_MAGIC, 45, struct rp1_pio_sm_clear_fifos_args)

#define PIO_IOC_GPIO_INIT _IOW(PIO_IOC_MAGIC, 50, struct rp1_gpio_init_args)
#define PIO_IOC_GPIO_SET_FUNCTION _IOW(PIO_IOC_MAGIC, 51, struct rp1_gpio_set_function_args)
#define PIO_IOC_GPIO_SET_PULLS _IOW(PIO_IOC_MAGIC, 52, struct rp1_gpio_set_pulls_args)
#define PIO_IOC_GPIO_SET_OUTOVER _IOW(PIO_IOC_MAGIC, 53, struct rp1_gpio_set_args)
#define PIO_IOC_GPIO_SET_INOVER _IOW(PIO_IOC_MAGIC, 54, struct rp1_gpio_set_args)
#define PIO_IOC_GPIO_SET_OEOVER _IOW(PIO_IOC_MAGIC, 55, struct rp1_gpio_set_args)
#define PIO_IOC_GPIO_SET_INPUT_ENABLED _IOW(PIO_IOC_MAGIC, 56, struct rp1_gpio_set_args)
#define PIO_IOC_GPIO_SET_DRIVE_STRENGTH _IOW(PIO_IOC_MAGIC, 57, struct rp1_gpio_set_args)

#endif
