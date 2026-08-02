/**
 * Copyright (c) 2023 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
// =============================================================================
// Register block : PROC_PIO
// Version        : 1
// Bus type       : ahbl
// Description    : Programmable IO block
// =============================================================================
#ifndef HARDWARE_REGS_PROC_PIO_DEFINED
#define HARDWARE_REGS_PROC_PIO_DEFINED
// =============================================================================
// Register    : PROC_PIO_CTRL
// Description : PIO control register
#define PROC_PIO_CTRL_OFFSET _u(0x00000000)
#define PROC_PIO_CTRL_BITS   _u(0x00000fff)
#define PROC_PIO_CTRL_RESET  _u(0x00000000)
#define PROC_PIO_CTRL_WIDTH  _u(32)
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_CTRL_CLKDIV_RESTART
// Description : Force clock dividers to restart their count and clear
//               fractional
//               accumulators. Restart multiple dividers to synchronise them.
#define PROC_PIO_CTRL_CLKDIV_RESTART_RESET  _u(0x0)
#define PROC_PIO_CTRL_CLKDIV_RESTART_BITS   _u(0x00000f00)
#define PROC_PIO_CTRL_CLKDIV_RESTART_MSB    _u(11)
#define PROC_PIO_CTRL_CLKDIV_RESTART_LSB    _u(8)
#define PROC_PIO_CTRL_CLKDIV_RESTART_ACCESS "SC"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_CTRL_SM_RESTART
// Description : Clear internal SM state which is otherwise difficult to access
//               (e.g. shift counters). Self-clearing.
#define PROC_PIO_CTRL_SM_RESTART_RESET  _u(0x0)
#define PROC_PIO_CTRL_SM_RESTART_BITS   _u(0x000000f0)
#define PROC_PIO_CTRL_SM_RESTART_MSB    _u(7)
#define PROC_PIO_CTRL_SM_RESTART_LSB    _u(4)
#define PROC_PIO_CTRL_SM_RESTART_ACCESS "SC"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_CTRL_SM_ENABLE
// Description : Enable state machine
#define PROC_PIO_CTRL_SM_ENABLE_RESET  _u(0x0)
#define PROC_PIO_CTRL_SM_ENABLE_BITS   _u(0x0000000f)
#define PROC_PIO_CTRL_SM_ENABLE_MSB    _u(3)
#define PROC_PIO_CTRL_SM_ENABLE_LSB    _u(0)
#define PROC_PIO_CTRL_SM_ENABLE_ACCESS "RW"
// =============================================================================
// Register    : PROC_PIO_FSTAT
// Description : FIFO status register
#define PROC_PIO_FSTAT_OFFSET _u(0x00000004)
#define PROC_PIO_FSTAT_BITS   _u(0x0f0f0f0f)
#define PROC_PIO_FSTAT_RESET  _u(0x0f000f00)
#define PROC_PIO_FSTAT_WIDTH  _u(32)
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_FSTAT_TXEMPTY
// Description : State machine TX FIFO is empty
#define PROC_PIO_FSTAT_TXEMPTY_RESET  _u(0xf)
#define PROC_PIO_FSTAT_TXEMPTY_BITS   _u(0x0f000000)
#define PROC_PIO_FSTAT_TXEMPTY_MSB    _u(27)
#define PROC_PIO_FSTAT_TXEMPTY_LSB    _u(24)
#define PROC_PIO_FSTAT_TXEMPTY_ACCESS "RO"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_FSTAT_TXFULL
// Description : State machine TX FIFO is full
#define PROC_PIO_FSTAT_TXFULL_RESET  _u(0x0)
#define PROC_PIO_FSTAT_TXFULL_BITS   _u(0x000f0000)
#define PROC_PIO_FSTAT_TXFULL_MSB    _u(19)
#define PROC_PIO_FSTAT_TXFULL_LSB    _u(16)
#define PROC_PIO_FSTAT_TXFULL_ACCESS "RO"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_FSTAT_RXEMPTY
// Description : State machine RX FIFO is empty
#define PROC_PIO_FSTAT_RXEMPTY_RESET  _u(0xf)
#define PROC_PIO_FSTAT_RXEMPTY_BITS   _u(0x00000f00)
#define PROC_PIO_FSTAT_RXEMPTY_MSB    _u(11)
#define PROC_PIO_FSTAT_RXEMPTY_LSB    _u(8)
#define PROC_PIO_FSTAT_RXEMPTY_ACCESS "RO"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_FSTAT_RXFULL
// Description : State machine RX FIFO is full
#define PROC_PIO_FSTAT_RXFULL_RESET  _u(0x0)
#define PROC_PIO_FSTAT_RXFULL_BITS   _u(0x0000000f)
#define PROC_PIO_FSTAT_RXFULL_MSB    _u(3)
#define PROC_PIO_FSTAT_RXFULL_LSB    _u(0)
#define PROC_PIO_FSTAT_RXFULL_ACCESS "RO"
// =============================================================================
// Register    : PROC_PIO_FDEBUG
// Description : FIFO debug register
#define PROC_PIO_FDEBUG_OFFSET _u(0x00000008)
#define PROC_PIO_FDEBUG_BITS   _u(0x0f0f0f0f)
#define PROC_PIO_FDEBUG_RESET  _u(0x00000000)
#define PROC_PIO_FDEBUG_WIDTH  _u(32)
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_FDEBUG_TXSTALL
// Description : State machine has stalled on empty TX FIFO. Write 1 to clear.
#define PROC_PIO_FDEBUG_TXSTALL_RESET  _u(0x0)
#define PROC_PIO_FDEBUG_TXSTALL_BITS   _u(0x0f000000)
#define PROC_PIO_FDEBUG_TXSTALL_MSB    _u(27)
#define PROC_PIO_FDEBUG_TXSTALL_LSB    _u(24)
#define PROC_PIO_FDEBUG_TXSTALL_ACCESS "WC"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_FDEBUG_TXOVER
// Description : TX FIFO overflow has occurred. Write 1 to clear.
#define PROC_PIO_FDEBUG_TXOVER_RESET  _u(0x0)
#define PROC_PIO_FDEBUG_TXOVER_BITS   _u(0x000f0000)
#define PROC_PIO_FDEBUG_TXOVER_MSB    _u(19)
#define PROC_PIO_FDEBUG_TXOVER_LSB    _u(16)
#define PROC_PIO_FDEBUG_TXOVER_ACCESS "WC"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_FDEBUG_RXUNDER
// Description : RX FIFO underflow has occurred. Write 1 to clear.
#define PROC_PIO_FDEBUG_RXUNDER_RESET  _u(0x0)
#define PROC_PIO_FDEBUG_RXUNDER_BITS   _u(0x00000f00)
#define PROC_PIO_FDEBUG_RXUNDER_MSB    _u(11)
#define PROC_PIO_FDEBUG_RXUNDER_LSB    _u(8)
#define PROC_PIO_FDEBUG_RXUNDER_ACCESS "WC"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_FDEBUG_RXSTALL
// Description : State machine has stalled on full RX FIFO. Write 1 to clear.
#define PROC_PIO_FDEBUG_RXSTALL_RESET  _u(0x0)
#define PROC_PIO_FDEBUG_RXSTALL_BITS   _u(0x0000000f)
#define PROC_PIO_FDEBUG_RXSTALL_MSB    _u(3)
#define PROC_PIO_FDEBUG_RXSTALL_LSB    _u(0)
#define PROC_PIO_FDEBUG_RXSTALL_ACCESS "WC"
// =============================================================================
// Register    : PROC_PIO_FLEVEL
// Description : FIFO levels
//               These count up to 15 only. When counting higher the extra bit
//               is set in flevel2 register and this value saturates
#define PROC_PIO_FLEVEL_OFFSET _u(0x0000000c)
#define PROC_PIO_FLEVEL_BITS   _u(0xffffffff)
#define PROC_PIO_FLEVEL_RESET  _u(0x00000000)
#define PROC_PIO_FLEVEL_WIDTH  _u(32)
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_FLEVEL_RX3
// Description : None
#define PROC_PIO_FLEVEL_RX3_RESET  _u(0x0)
#define PROC_PIO_FLEVEL_RX3_BITS   _u(0xf0000000)
#define PROC_PIO_FLEVEL_RX3_MSB    _u(31)
#define PROC_PIO_FLEVEL_RX3_LSB    _u(28)
#define PROC_PIO_FLEVEL_RX3_ACCESS "RO"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_FLEVEL_TX3
// Description : None
#define PROC_PIO_FLEVEL_TX3_RESET  _u(0x0)
#define PROC_PIO_FLEVEL_TX3_BITS   _u(0x0f000000)
#define PROC_PIO_FLEVEL_TX3_MSB    _u(27)
#define PROC_PIO_FLEVEL_TX3_LSB    _u(24)
#define PROC_PIO_FLEVEL_TX3_ACCESS "RO"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_FLEVEL_RX2
// Description : None
#define PROC_PIO_FLEVEL_RX2_RESET  _u(0x0)
#define PROC_PIO_FLEVEL_RX2_BITS   _u(0x00f00000)
#define PROC_PIO_FLEVEL_RX2_MSB    _u(23)
#define PROC_PIO_FLEVEL_RX2_LSB    _u(20)
#define PROC_PIO_FLEVEL_RX2_ACCESS "RO"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_FLEVEL_TX2
// Description : None
#define PROC_PIO_FLEVEL_TX2_RESET  _u(0x0)
#define PROC_PIO_FLEVEL_TX2_BITS   _u(0x000f0000)
#define PROC_PIO_FLEVEL_TX2_MSB    _u(19)
#define PROC_PIO_FLEVEL_TX2_LSB    _u(16)
#define PROC_PIO_FLEVEL_TX2_ACCESS "RO"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_FLEVEL_RX1
// Description : None
#define PROC_PIO_FLEVEL_RX1_RESET  _u(0x0)
#define PROC_PIO_FLEVEL_RX1_BITS   _u(0x0000f000)
#define PROC_PIO_FLEVEL_RX1_MSB    _u(15)
#define PROC_PIO_FLEVEL_RX1_LSB    _u(12)
#define PROC_PIO_FLEVEL_RX1_ACCESS "RO"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_FLEVEL_TX1
// Description : None
#define PROC_PIO_FLEVEL_TX1_RESET  _u(0x0)
#define PROC_PIO_FLEVEL_TX1_BITS   _u(0x00000f00)
#define PROC_PIO_FLEVEL_TX1_MSB    _u(11)
#define PROC_PIO_FLEVEL_TX1_LSB    _u(8)
#define PROC_PIO_FLEVEL_TX1_ACCESS "RO"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_FLEVEL_RX0
// Description : None
#define PROC_PIO_FLEVEL_RX0_RESET  _u(0x0)
#define PROC_PIO_FLEVEL_RX0_BITS   _u(0x000000f0)
#define PROC_PIO_FLEVEL_RX0_MSB    _u(7)
#define PROC_PIO_FLEVEL_RX0_LSB    _u(4)
#define PROC_PIO_FLEVEL_RX0_ACCESS "RO"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_FLEVEL_TX0
// Description : None
#define PROC_PIO_FLEVEL_TX0_RESET  _u(0x0)
#define PROC_PIO_FLEVEL_TX0_BITS   _u(0x0000000f)
#define PROC_PIO_FLEVEL_TX0_MSB    _u(3)
#define PROC_PIO_FLEVEL_TX0_LSB    _u(0)
#define PROC_PIO_FLEVEL_TX0_ACCESS "RO"
// =============================================================================
// Register    : PROC_PIO_FLEVEL2
// Description : FIFO level extra bits
//               These are only used in double fifo mode, and the fifo has more
//               than 15 elements
#define PROC_PIO_FLEVEL2_OFFSET _u(0x00000010)
#define PROC_PIO_FLEVEL2_BITS   _u(0x11111111)
#define PROC_PIO_FLEVEL2_RESET  _u(0x00000000)
#define PROC_PIO_FLEVEL2_WIDTH  _u(32)
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_FLEVEL2_RX3
// Description : None
#define PROC_PIO_FLEVEL2_RX3_RESET  _u(0x0)
#define PROC_PIO_FLEVEL2_RX3_BITS   _u(0x10000000)
#define PROC_PIO_FLEVEL2_RX3_MSB    _u(28)
#define PROC_PIO_FLEVEL2_RX3_LSB    _u(28)
#define PROC_PIO_FLEVEL2_RX3_ACCESS "RO"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_FLEVEL2_TX3
// Description : None
#define PROC_PIO_FLEVEL2_TX3_RESET  _u(0x0)
#define PROC_PIO_FLEVEL2_TX3_BITS   _u(0x01000000)
#define PROC_PIO_FLEVEL2_TX3_MSB    _u(24)
#define PROC_PIO_FLEVEL2_TX3_LSB    _u(24)
#define PROC_PIO_FLEVEL2_TX3_ACCESS "RO"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_FLEVEL2_RX2
// Description : None
#define PROC_PIO_FLEVEL2_RX2_RESET  _u(0x0)
#define PROC_PIO_FLEVEL2_RX2_BITS   _u(0x00100000)
#define PROC_PIO_FLEVEL2_RX2_MSB    _u(20)
#define PROC_PIO_FLEVEL2_RX2_LSB    _u(20)
#define PROC_PIO_FLEVEL2_RX2_ACCESS "RO"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_FLEVEL2_TX2
// Description : None
#define PROC_PIO_FLEVEL2_TX2_RESET  _u(0x0)
#define PROC_PIO_FLEVEL2_TX2_BITS   _u(0x00010000)
#define PROC_PIO_FLEVEL2_TX2_MSB    _u(16)
#define PROC_PIO_FLEVEL2_TX2_LSB    _u(16)
#define PROC_PIO_FLEVEL2_TX2_ACCESS "RO"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_FLEVEL2_RX1
// Description : None
#define PROC_PIO_FLEVEL2_RX1_RESET  _u(0x0)
#define PROC_PIO_FLEVEL2_RX1_BITS   _u(0x00001000)
#define PROC_PIO_FLEVEL2_RX1_MSB    _u(12)
#define PROC_PIO_FLEVEL2_RX1_LSB    _u(12)
#define PROC_PIO_FLEVEL2_RX1_ACCESS "RO"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_FLEVEL2_TX1
// Description : None
#define PROC_PIO_FLEVEL2_TX1_RESET  _u(0x0)
#define PROC_PIO_FLEVEL2_TX1_BITS   _u(0x00000100)
#define PROC_PIO_FLEVEL2_TX1_MSB    _u(8)
#define PROC_PIO_FLEVEL2_TX1_LSB    _u(8)
#define PROC_PIO_FLEVEL2_TX1_ACCESS "RO"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_FLEVEL2_RX0
// Description : None
#define PROC_PIO_FLEVEL2_RX0_RESET  _u(0x0)
#define PROC_PIO_FLEVEL2_RX0_BITS   _u(0x00000010)
#define PROC_PIO_FLEVEL2_RX0_MSB    _u(4)
#define PROC_PIO_FLEVEL2_RX0_LSB    _u(4)
#define PROC_PIO_FLEVEL2_RX0_ACCESS "RO"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_FLEVEL2_TX0
// Description : None
#define PROC_PIO_FLEVEL2_TX0_RESET  _u(0x0)
#define PROC_PIO_FLEVEL2_TX0_BITS   _u(0x00000001)
#define PROC_PIO_FLEVEL2_TX0_MSB    _u(0)
#define PROC_PIO_FLEVEL2_TX0_LSB    _u(0)
#define PROC_PIO_FLEVEL2_TX0_ACCESS "RO"
// =============================================================================
// Register    : PROC_PIO_TXF0
// Description : Direct write access to the TX FIFO for this state machine. Each
//               write pushes one word to the FIFO.
#define PROC_PIO_TXF0_OFFSET _u(0x00000014)
#define PROC_PIO_TXF0_BITS   _u(0xffffffff)
#define PROC_PIO_TXF0_RESET  _u(0x00000000)
#define PROC_PIO_TXF0_WIDTH  _u(32)
#define PROC_PIO_TXF0_MSB    _u(31)
#define PROC_PIO_TXF0_LSB    _u(0)
#define PROC_PIO_TXF0_ACCESS "WF"
// =============================================================================
// Register    : PROC_PIO_TXF1
// Description : Direct write access to the TX FIFO for this state machine. Each
//               write pushes one word to the FIFO.
#define PROC_PIO_TXF1_OFFSET _u(0x00000018)
#define PROC_PIO_TXF1_BITS   _u(0xffffffff)
#define PROC_PIO_TXF1_RESET  _u(0x00000000)
#define PROC_PIO_TXF1_WIDTH  _u(32)
#define PROC_PIO_TXF1_MSB    _u(31)
#define PROC_PIO_TXF1_LSB    _u(0)
#define PROC_PIO_TXF1_ACCESS "WF"
// =============================================================================
// Register    : PROC_PIO_TXF2
// Description : Direct write access to the TX FIFO for this state machine. Each
//               write pushes one word to the FIFO.
#define PROC_PIO_TXF2_OFFSET _u(0x0000001c)
#define PROC_PIO_TXF2_BITS   _u(0xffffffff)
#define PROC_PIO_TXF2_RESET  _u(0x00000000)
#define PROC_PIO_TXF2_WIDTH  _u(32)
#define PROC_PIO_TXF2_MSB    _u(31)
#define PROC_PIO_TXF2_LSB    _u(0)
#define PROC_PIO_TXF2_ACCESS "WF"
// =============================================================================
// Register    : PROC_PIO_TXF3
// Description : Direct write access to the TX FIFO for this state machine. Each
//               write pushes one word to the FIFO.
#define PROC_PIO_TXF3_OFFSET _u(0x00000020)
#define PROC_PIO_TXF3_BITS   _u(0xffffffff)
#define PROC_PIO_TXF3_RESET  _u(0x00000000)
#define PROC_PIO_TXF3_WIDTH  _u(32)
#define PROC_PIO_TXF3_MSB    _u(31)
#define PROC_PIO_TXF3_LSB    _u(0)
#define PROC_PIO_TXF3_ACCESS "WF"
// =============================================================================
// Register    : PROC_PIO_RXF0
// Description : Direct read access to the RX FIFO for this state machine. Each
//               read pops one word from the FIFO.
#define PROC_PIO_RXF0_OFFSET _u(0x00000024)
#define PROC_PIO_RXF0_BITS   _u(0xffffffff)
#define PROC_PIO_RXF0_RESET  "-"
#define PROC_PIO_RXF0_WIDTH  _u(32)
#define PROC_PIO_RXF0_MSB    _u(31)
#define PROC_PIO_RXF0_LSB    _u(0)
#define PROC_PIO_RXF0_ACCESS "RF"
// =============================================================================
// Register    : PROC_PIO_RXF1
// Description : Direct read access to the RX FIFO for this state machine. Each
//               read pops one word from the FIFO.
#define PROC_PIO_RXF1_OFFSET _u(0x00000028)
#define PROC_PIO_RXF1_BITS   _u(0xffffffff)
#define PROC_PIO_RXF1_RESET  "-"
#define PROC_PIO_RXF1_WIDTH  _u(32)
#define PROC_PIO_RXF1_MSB    _u(31)
#define PROC_PIO_RXF1_LSB    _u(0)
#define PROC_PIO_RXF1_ACCESS "RF"
// =============================================================================
// Register    : PROC_PIO_RXF2
// Description : Direct read access to the RX FIFO for this state machine. Each
//               read pops one word from the FIFO.
#define PROC_PIO_RXF2_OFFSET _u(0x0000002c)
#define PROC_PIO_RXF2_BITS   _u(0xffffffff)
#define PROC_PIO_RXF2_RESET  "-"
#define PROC_PIO_RXF2_WIDTH  _u(32)
#define PROC_PIO_RXF2_MSB    _u(31)
#define PROC_PIO_RXF2_LSB    _u(0)
#define PROC_PIO_RXF2_ACCESS "RF"
// =============================================================================
// Register    : PROC_PIO_RXF3
// Description : Direct read access to the RX FIFO for this state machine. Each
//               read pops one word from the FIFO.
#define PROC_PIO_RXF3_OFFSET _u(0x00000030)
#define PROC_PIO_RXF3_BITS   _u(0xffffffff)
#define PROC_PIO_RXF3_RESET  "-"
#define PROC_PIO_RXF3_WIDTH  _u(32)
#define PROC_PIO_RXF3_MSB    _u(31)
#define PROC_PIO_RXF3_LSB    _u(0)
#define PROC_PIO_RXF3_ACCESS "RF"
// =============================================================================
// Register    : PROC_PIO_IRQ
// Description : Interrupt request register. Write 1 to clear
#define PROC_PIO_IRQ_OFFSET _u(0x00000034)
#define PROC_PIO_IRQ_BITS   _u(0x000000ff)
#define PROC_PIO_IRQ_RESET  _u(0x00000000)
#define PROC_PIO_IRQ_WIDTH  _u(32)
#define PROC_PIO_IRQ_MSB    _u(7)
#define PROC_PIO_IRQ_LSB    _u(0)
#define PROC_PIO_IRQ_ACCESS "WC"
// =============================================================================
// Register    : PROC_PIO_IRQ_FORCE
// Description : Writing a 1 to each of these bits will forcibly assert the
//               corresponding IRQ.
//               Note this is different to the INTF register: writing here
//               affects PIO internal
//               state. INTF just asserts the processor-facing IRQ signal for
//               testing ISRs,
//               and is not visible to the state machines.
#define PROC_PIO_IRQ_FORCE_OFFSET _u(0x00000038)
#define PROC_PIO_IRQ_FORCE_BITS   _u(0x000000ff)
#define PROC_PIO_IRQ_FORCE_RESET  _u(0x00000000)
#define PROC_PIO_IRQ_FORCE_WIDTH  _u(32)
#define PROC_PIO_IRQ_FORCE_MSB    _u(7)
#define PROC_PIO_IRQ_FORCE_LSB    _u(0)
#define PROC_PIO_IRQ_FORCE_ACCESS "WF"
// =============================================================================
// Register    : PROC_PIO_INPUT_SYNC_BYPASS
// Description : There is a 2-flipflop synchronizer on each GPIO input, which
//               protects
//               PIO logic from metastabilities. This increases input delay, and
//               for fast
//               synchronous IO (e.g. SPI) these synchronizers may need to be
//               bypassed.
//               Each bit in this register corresponds to one GPIO.
//               0 -> input is synchronized (default)
//               1 -> synchronizer is bypassed
//               If in doubt, leave this register as all zeroes.
#define PROC_PIO_INPUT_SYNC_BYPASS_OFFSET _u(0x0000003c)
#define PROC_PIO_INPUT_SYNC_BYPASS_BITS   _u(0xffffffff)
#define PROC_PIO_INPUT_SYNC_BYPASS_RESET  _u(0x00000000)
#define PROC_PIO_INPUT_SYNC_BYPASS_WIDTH  _u(32)
#define PROC_PIO_INPUT_SYNC_BYPASS_MSB    _u(31)
#define PROC_PIO_INPUT_SYNC_BYPASS_LSB    _u(0)
#define PROC_PIO_INPUT_SYNC_BYPASS_ACCESS "RW"
// =============================================================================
// Register    : PROC_PIO_DBG_PADOUT
// Description : Read to sample the pad output values PIO is currently driving
//               to the GPIOs.
#define PROC_PIO_DBG_PADOUT_OFFSET _u(0x00000040)
#define PROC_PIO_DBG_PADOUT_BITS   _u(0xffffffff)
#define PROC_PIO_DBG_PADOUT_RESET  _u(0x00000000)
#define PROC_PIO_DBG_PADOUT_WIDTH  _u(32)
#define PROC_PIO_DBG_PADOUT_MSB    _u(31)
#define PROC_PIO_DBG_PADOUT_LSB    _u(0)
#define PROC_PIO_DBG_PADOUT_ACCESS "RO"
// =============================================================================
// Register    : PROC_PIO_DBG_PADOE
// Description : Read to sample the pad output enables (direction) PIO is
//               currently driving to the GPIOs.
#define PROC_PIO_DBG_PADOE_OFFSET _u(0x00000044)
#define PROC_PIO_DBG_PADOE_BITS   _u(0xffffffff)
#define PROC_PIO_DBG_PADOE_RESET  _u(0x00000000)
#define PROC_PIO_DBG_PADOE_WIDTH  _u(32)
#define PROC_PIO_DBG_PADOE_MSB    _u(31)
#define PROC_PIO_DBG_PADOE_LSB    _u(0)
#define PROC_PIO_DBG_PADOE_ACCESS "RO"
// =============================================================================
// Register    : PROC_PIO_DBG_CFGINFO
// Description : The PIO hardware has some free parameters that may vary between
//               chip products.
//               These should be provided in the chip datasheet, but are also
//               exposed here.
#define PROC_PIO_DBG_CFGINFO_OFFSET _u(0x00000048)
#define PROC_PIO_DBG_CFGINFO_BITS   _u(0x003f0f3f)
#define PROC_PIO_DBG_CFGINFO_RESET  _u(0x00000000)
#define PROC_PIO_DBG_CFGINFO_WIDTH  _u(32)
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_DBG_CFGINFO_IMEM_SIZE
// Description : The size of the instruction memory, measured in units of one
//               instruction
#define PROC_PIO_DBG_CFGINFO_IMEM_SIZE_RESET  "-"
#define PROC_PIO_DBG_CFGINFO_IMEM_SIZE_BITS   _u(0x003f0000)
#define PROC_PIO_DBG_CFGINFO_IMEM_SIZE_MSB    _u(21)
#define PROC_PIO_DBG_CFGINFO_IMEM_SIZE_LSB    _u(16)
#define PROC_PIO_DBG_CFGINFO_IMEM_SIZE_ACCESS "RO"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_DBG_CFGINFO_SM_COUNT
// Description : The number of state machines this PIO instance is equipped
//               with.
#define PROC_PIO_DBG_CFGINFO_SM_COUNT_RESET  "-"
#define PROC_PIO_DBG_CFGINFO_SM_COUNT_BITS   _u(0x00000f00)
#define PROC_PIO_DBG_CFGINFO_SM_COUNT_MSB    _u(11)
#define PROC_PIO_DBG_CFGINFO_SM_COUNT_LSB    _u(8)
#define PROC_PIO_DBG_CFGINFO_SM_COUNT_ACCESS "RO"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_DBG_CFGINFO_FIFO_DEPTH
// Description : The depth of the state machine TX/RX FIFOs, measured in words.
//               Joining fifos via SHIFTCTRL_FJOIN gives one FIFO with double
//               this depth.
#define PROC_PIO_DBG_CFGINFO_FIFO_DEPTH_RESET  "-"
#define PROC_PIO_DBG_CFGINFO_FIFO_DEPTH_BITS   _u(0x0000003f)
#define PROC_PIO_DBG_CFGINFO_FIFO_DEPTH_MSB    _u(5)
#define PROC_PIO_DBG_CFGINFO_FIFO_DEPTH_LSB    _u(0)
#define PROC_PIO_DBG_CFGINFO_FIFO_DEPTH_ACCESS "RO"
// =============================================================================
// Register    : PROC_PIO_INSTR_MEM0
// Description : Write-only access to instruction memory location 0
#define PROC_PIO_INSTR_MEM0_OFFSET _u(0x0000004c)
#define PROC_PIO_INSTR_MEM0_BITS   _u(0x0000ffff)
#define PROC_PIO_INSTR_MEM0_RESET  _u(0x00000000)
#define PROC_PIO_INSTR_MEM0_WIDTH  _u(32)
#define PROC_PIO_INSTR_MEM0_MSB    _u(15)
#define PROC_PIO_INSTR_MEM0_LSB    _u(0)
#define PROC_PIO_INSTR_MEM0_ACCESS "WO"
// =============================================================================
// Register    : PROC_PIO_INSTR_MEM1
// Description : Write-only access to instruction memory location 1
#define PROC_PIO_INSTR_MEM1_OFFSET _u(0x00000050)
#define PROC_PIO_INSTR_MEM1_BITS   _u(0x0000ffff)
#define PROC_PIO_INSTR_MEM1_RESET  _u(0x00000000)
#define PROC_PIO_INSTR_MEM1_WIDTH  _u(32)
#define PROC_PIO_INSTR_MEM1_MSB    _u(15)
#define PROC_PIO_INSTR_MEM1_LSB    _u(0)
#define PROC_PIO_INSTR_MEM1_ACCESS "WO"
// =============================================================================
// Register    : PROC_PIO_INSTR_MEM2
// Description : Write-only access to instruction memory location 2
#define PROC_PIO_INSTR_MEM2_OFFSET _u(0x00000054)
#define PROC_PIO_INSTR_MEM2_BITS   _u(0x0000ffff)
#define PROC_PIO_INSTR_MEM2_RESET  _u(0x00000000)
#define PROC_PIO_INSTR_MEM2_WIDTH  _u(32)
#define PROC_PIO_INSTR_MEM2_MSB    _u(15)
#define PROC_PIO_INSTR_MEM2_LSB    _u(0)
#define PROC_PIO_INSTR_MEM2_ACCESS "WO"
// =============================================================================
// Register    : PROC_PIO_INSTR_MEM3
// Description : Write-only access to instruction memory location 3
#define PROC_PIO_INSTR_MEM3_OFFSET _u(0x00000058)
#define PROC_PIO_INSTR_MEM3_BITS   _u(0x0000ffff)
#define PROC_PIO_INSTR_MEM3_RESET  _u(0x00000000)
#define PROC_PIO_INSTR_MEM3_WIDTH  _u(32)
#define PROC_PIO_INSTR_MEM3_MSB    _u(15)
#define PROC_PIO_INSTR_MEM3_LSB    _u(0)
#define PROC_PIO_INSTR_MEM3_ACCESS "WO"
// =============================================================================
// Register    : PROC_PIO_INSTR_MEM4
// Description : Write-only access to instruction memory location 4
#define PROC_PIO_INSTR_MEM4_OFFSET _u(0x0000005c)
#define PROC_PIO_INSTR_MEM4_BITS   _u(0x0000ffff)
#define PROC_PIO_INSTR_MEM4_RESET  _u(0x00000000)
#define PROC_PIO_INSTR_MEM4_WIDTH  _u(32)
#define PROC_PIO_INSTR_MEM4_MSB    _u(15)
#define PROC_PIO_INSTR_MEM4_LSB    _u(0)
#define PROC_PIO_INSTR_MEM4_ACCESS "WO"
// =============================================================================
// Register    : PROC_PIO_INSTR_MEM5
// Description : Write-only access to instruction memory location 5
#define PROC_PIO_INSTR_MEM5_OFFSET _u(0x00000060)
#define PROC_PIO_INSTR_MEM5_BITS   _u(0x0000ffff)
#define PROC_PIO_INSTR_MEM5_RESET  _u(0x00000000)
#define PROC_PIO_INSTR_MEM5_WIDTH  _u(32)
#define PROC_PIO_INSTR_MEM5_MSB    _u(15)
#define PROC_PIO_INSTR_MEM5_LSB    _u(0)
#define PROC_PIO_INSTR_MEM5_ACCESS "WO"
// =============================================================================
// Register    : PROC_PIO_INSTR_MEM6
// Description : Write-only access to instruction memory location 6
#define PROC_PIO_INSTR_MEM6_OFFSET _u(0x00000064)
#define PROC_PIO_INSTR_MEM6_BITS   _u(0x0000ffff)
#define PROC_PIO_INSTR_MEM6_RESET  _u(0x00000000)
#define PROC_PIO_INSTR_MEM6_WIDTH  _u(32)
#define PROC_PIO_INSTR_MEM6_MSB    _u(15)
#define PROC_PIO_INSTR_MEM6_LSB    _u(0)
#define PROC_PIO_INSTR_MEM6_ACCESS "WO"
// =============================================================================
// Register    : PROC_PIO_INSTR_MEM7
// Description : Write-only access to instruction memory location 7
#define PROC_PIO_INSTR_MEM7_OFFSET _u(0x00000068)
#define PROC_PIO_INSTR_MEM7_BITS   _u(0x0000ffff)
#define PROC_PIO_INSTR_MEM7_RESET  _u(0x00000000)
#define PROC_PIO_INSTR_MEM7_WIDTH  _u(32)
#define PROC_PIO_INSTR_MEM7_MSB    _u(15)
#define PROC_PIO_INSTR_MEM7_LSB    _u(0)
#define PROC_PIO_INSTR_MEM7_ACCESS "WO"
// =============================================================================
// Register    : PROC_PIO_INSTR_MEM8
// Description : Write-only access to instruction memory location 8
#define PROC_PIO_INSTR_MEM8_OFFSET _u(0x0000006c)
#define PROC_PIO_INSTR_MEM8_BITS   _u(0x0000ffff)
#define PROC_PIO_INSTR_MEM8_RESET  _u(0x00000000)
#define PROC_PIO_INSTR_MEM8_WIDTH  _u(32)
#define PROC_PIO_INSTR_MEM8_MSB    _u(15)
#define PROC_PIO_INSTR_MEM8_LSB    _u(0)
#define PROC_PIO_INSTR_MEM8_ACCESS "WO"
// =============================================================================
// Register    : PROC_PIO_INSTR_MEM9
// Description : Write-only access to instruction memory location 9
#define PROC_PIO_INSTR_MEM9_OFFSET _u(0x00000070)
#define PROC_PIO_INSTR_MEM9_BITS   _u(0x0000ffff)
#define PROC_PIO_INSTR_MEM9_RESET  _u(0x00000000)
#define PROC_PIO_INSTR_MEM9_WIDTH  _u(32)
#define PROC_PIO_INSTR_MEM9_MSB    _u(15)
#define PROC_PIO_INSTR_MEM9_LSB    _u(0)
#define PROC_PIO_INSTR_MEM9_ACCESS "WO"
// =============================================================================
// Register    : PROC_PIO_INSTR_MEM10
// Description : Write-only access to instruction memory location 10
#define PROC_PIO_INSTR_MEM10_OFFSET _u(0x00000074)
#define PROC_PIO_INSTR_MEM10_BITS   _u(0x0000ffff)
#define PROC_PIO_INSTR_MEM10_RESET  _u(0x00000000)
#define PROC_PIO_INSTR_MEM10_WIDTH  _u(32)
#define PROC_PIO_INSTR_MEM10_MSB    _u(15)
#define PROC_PIO_INSTR_MEM10_LSB    _u(0)
#define PROC_PIO_INSTR_MEM10_ACCESS "WO"
// =============================================================================
// Register    : PROC_PIO_INSTR_MEM11
// Description : Write-only access to instruction memory location 11
#define PROC_PIO_INSTR_MEM11_OFFSET _u(0x00000078)
#define PROC_PIO_INSTR_MEM11_BITS   _u(0x0000ffff)
#define PROC_PIO_INSTR_MEM11_RESET  _u(0x00000000)
#define PROC_PIO_INSTR_MEM11_WIDTH  _u(32)
#define PROC_PIO_INSTR_MEM11_MSB    _u(15)
#define PROC_PIO_INSTR_MEM11_LSB    _u(0)
#define PROC_PIO_INSTR_MEM11_ACCESS "WO"
// =============================================================================
// Register    : PROC_PIO_INSTR_MEM12
// Description : Write-only access to instruction memory location 12
#define PROC_PIO_INSTR_MEM12_OFFSET _u(0x0000007c)
#define PROC_PIO_INSTR_MEM12_BITS   _u(0x0000ffff)
#define PROC_PIO_INSTR_MEM12_RESET  _u(0x00000000)
#define PROC_PIO_INSTR_MEM12_WIDTH  _u(32)
#define PROC_PIO_INSTR_MEM12_MSB    _u(15)
#define PROC_PIO_INSTR_MEM12_LSB    _u(0)
#define PROC_PIO_INSTR_MEM12_ACCESS "WO"
// =============================================================================
// Register    : PROC_PIO_INSTR_MEM13
// Description : Write-only access to instruction memory location 13
#define PROC_PIO_INSTR_MEM13_OFFSET _u(0x00000080)
#define PROC_PIO_INSTR_MEM13_BITS   _u(0x0000ffff)
#define PROC_PIO_INSTR_MEM13_RESET  _u(0x00000000)
#define PROC_PIO_INSTR_MEM13_WIDTH  _u(32)
#define PROC_PIO_INSTR_MEM13_MSB    _u(15)
#define PROC_PIO_INSTR_MEM13_LSB    _u(0)
#define PROC_PIO_INSTR_MEM13_ACCESS "WO"
// =============================================================================
// Register    : PROC_PIO_INSTR_MEM14
// Description : Write-only access to instruction memory location 14
#define PROC_PIO_INSTR_MEM14_OFFSET _u(0x00000084)
#define PROC_PIO_INSTR_MEM14_BITS   _u(0x0000ffff)
#define PROC_PIO_INSTR_MEM14_RESET  _u(0x00000000)
#define PROC_PIO_INSTR_MEM14_WIDTH  _u(32)
#define PROC_PIO_INSTR_MEM14_MSB    _u(15)
#define PROC_PIO_INSTR_MEM14_LSB    _u(0)
#define PROC_PIO_INSTR_MEM14_ACCESS "WO"
// =============================================================================
// Register    : PROC_PIO_INSTR_MEM15
// Description : Write-only access to instruction memory location 15
#define PROC_PIO_INSTR_MEM15_OFFSET _u(0x00000088)
#define PROC_PIO_INSTR_MEM15_BITS   _u(0x0000ffff)
#define PROC_PIO_INSTR_MEM15_RESET  _u(0x00000000)
#define PROC_PIO_INSTR_MEM15_WIDTH  _u(32)
#define PROC_PIO_INSTR_MEM15_MSB    _u(15)
#define PROC_PIO_INSTR_MEM15_LSB    _u(0)
#define PROC_PIO_INSTR_MEM15_ACCESS "WO"
// =============================================================================
// Register    : PROC_PIO_INSTR_MEM16
// Description : Write-only access to instruction memory location 16
#define PROC_PIO_INSTR_MEM16_OFFSET _u(0x0000008c)
#define PROC_PIO_INSTR_MEM16_BITS   _u(0x0000ffff)
#define PROC_PIO_INSTR_MEM16_RESET  _u(0x00000000)
#define PROC_PIO_INSTR_MEM16_WIDTH  _u(32)
#define PROC_PIO_INSTR_MEM16_MSB    _u(15)
#define PROC_PIO_INSTR_MEM16_LSB    _u(0)
#define PROC_PIO_INSTR_MEM16_ACCESS "WO"
// =============================================================================
// Register    : PROC_PIO_INSTR_MEM17
// Description : Write-only access to instruction memory location 17
#define PROC_PIO_INSTR_MEM17_OFFSET _u(0x00000090)
#define PROC_PIO_INSTR_MEM17_BITS   _u(0x0000ffff)
#define PROC_PIO_INSTR_MEM17_RESET  _u(0x00000000)
#define PROC_PIO_INSTR_MEM17_WIDTH  _u(32)
#define PROC_PIO_INSTR_MEM17_MSB    _u(15)
#define PROC_PIO_INSTR_MEM17_LSB    _u(0)
#define PROC_PIO_INSTR_MEM17_ACCESS "WO"
// =============================================================================
// Register    : PROC_PIO_INSTR_MEM18
// Description : Write-only access to instruction memory location 18
#define PROC_PIO_INSTR_MEM18_OFFSET _u(0x00000094)
#define PROC_PIO_INSTR_MEM18_BITS   _u(0x0000ffff)
#define PROC_PIO_INSTR_MEM18_RESET  _u(0x00000000)
#define PROC_PIO_INSTR_MEM18_WIDTH  _u(32)
#define PROC_PIO_INSTR_MEM18_MSB    _u(15)
#define PROC_PIO_INSTR_MEM18_LSB    _u(0)
#define PROC_PIO_INSTR_MEM18_ACCESS "WO"
// =============================================================================
// Register    : PROC_PIO_INSTR_MEM19
// Description : Write-only access to instruction memory location 19
#define PROC_PIO_INSTR_MEM19_OFFSET _u(0x00000098)
#define PROC_PIO_INSTR_MEM19_BITS   _u(0x0000ffff)
#define PROC_PIO_INSTR_MEM19_RESET  _u(0x00000000)
#define PROC_PIO_INSTR_MEM19_WIDTH  _u(32)
#define PROC_PIO_INSTR_MEM19_MSB    _u(15)
#define PROC_PIO_INSTR_MEM19_LSB    _u(0)
#define PROC_PIO_INSTR_MEM19_ACCESS "WO"
// =============================================================================
// Register    : PROC_PIO_INSTR_MEM20
// Description : Write-only access to instruction memory location 20
#define PROC_PIO_INSTR_MEM20_OFFSET _u(0x0000009c)
#define PROC_PIO_INSTR_MEM20_BITS   _u(0x0000ffff)
#define PROC_PIO_INSTR_MEM20_RESET  _u(0x00000000)
#define PROC_PIO_INSTR_MEM20_WIDTH  _u(32)
#define PROC_PIO_INSTR_MEM20_MSB    _u(15)
#define PROC_PIO_INSTR_MEM20_LSB    _u(0)
#define PROC_PIO_INSTR_MEM20_ACCESS "WO"
// =============================================================================
// Register    : PROC_PIO_INSTR_MEM21
// Description : Write-only access to instruction memory location 21
#define PROC_PIO_INSTR_MEM21_OFFSET _u(0x000000a0)
#define PROC_PIO_INSTR_MEM21_BITS   _u(0x0000ffff)
#define PROC_PIO_INSTR_MEM21_RESET  _u(0x00000000)
#define PROC_PIO_INSTR_MEM21_WIDTH  _u(32)
#define PROC_PIO_INSTR_MEM21_MSB    _u(15)
#define PROC_PIO_INSTR_MEM21_LSB    _u(0)
#define PROC_PIO_INSTR_MEM21_ACCESS "WO"
// =============================================================================
// Register    : PROC_PIO_INSTR_MEM22
// Description : Write-only access to instruction memory location 22
#define PROC_PIO_INSTR_MEM22_OFFSET _u(0x000000a4)
#define PROC_PIO_INSTR_MEM22_BITS   _u(0x0000ffff)
#define PROC_PIO_INSTR_MEM22_RESET  _u(0x00000000)
#define PROC_PIO_INSTR_MEM22_WIDTH  _u(32)
#define PROC_PIO_INSTR_MEM22_MSB    _u(15)
#define PROC_PIO_INSTR_MEM22_LSB    _u(0)
#define PROC_PIO_INSTR_MEM22_ACCESS "WO"
// =============================================================================
// Register    : PROC_PIO_INSTR_MEM23
// Description : Write-only access to instruction memory location 23
#define PROC_PIO_INSTR_MEM23_OFFSET _u(0x000000a8)
#define PROC_PIO_INSTR_MEM23_BITS   _u(0x0000ffff)
#define PROC_PIO_INSTR_MEM23_RESET  _u(0x00000000)
#define PROC_PIO_INSTR_MEM23_WIDTH  _u(32)
#define PROC_PIO_INSTR_MEM23_MSB    _u(15)
#define PROC_PIO_INSTR_MEM23_LSB    _u(0)
#define PROC_PIO_INSTR_MEM23_ACCESS "WO"
// =============================================================================
// Register    : PROC_PIO_INSTR_MEM24
// Description : Write-only access to instruction memory location 24
#define PROC_PIO_INSTR_MEM24_OFFSET _u(0x000000ac)
#define PROC_PIO_INSTR_MEM24_BITS   _u(0x0000ffff)
#define PROC_PIO_INSTR_MEM24_RESET  _u(0x00000000)
#define PROC_PIO_INSTR_MEM24_WIDTH  _u(32)
#define PROC_PIO_INSTR_MEM24_MSB    _u(15)
#define PROC_PIO_INSTR_MEM24_LSB    _u(0)
#define PROC_PIO_INSTR_MEM24_ACCESS "WO"
// =============================================================================
// Register    : PROC_PIO_INSTR_MEM25
// Description : Write-only access to instruction memory location 25
#define PROC_PIO_INSTR_MEM25_OFFSET _u(0x000000b0)
#define PROC_PIO_INSTR_MEM25_BITS   _u(0x0000ffff)
#define PROC_PIO_INSTR_MEM25_RESET  _u(0x00000000)
#define PROC_PIO_INSTR_MEM25_WIDTH  _u(32)
#define PROC_PIO_INSTR_MEM25_MSB    _u(15)
#define PROC_PIO_INSTR_MEM25_LSB    _u(0)
#define PROC_PIO_INSTR_MEM25_ACCESS "WO"
// =============================================================================
// Register    : PROC_PIO_INSTR_MEM26
// Description : Write-only access to instruction memory location 26
#define PROC_PIO_INSTR_MEM26_OFFSET _u(0x000000b4)
#define PROC_PIO_INSTR_MEM26_BITS   _u(0x0000ffff)
#define PROC_PIO_INSTR_MEM26_RESET  _u(0x00000000)
#define PROC_PIO_INSTR_MEM26_WIDTH  _u(32)
#define PROC_PIO_INSTR_MEM26_MSB    _u(15)
#define PROC_PIO_INSTR_MEM26_LSB    _u(0)
#define PROC_PIO_INSTR_MEM26_ACCESS "WO"
// =============================================================================
// Register    : PROC_PIO_INSTR_MEM27
// Description : Write-only access to instruction memory location 27
#define PROC_PIO_INSTR_MEM27_OFFSET _u(0x000000b8)
#define PROC_PIO_INSTR_MEM27_BITS   _u(0x0000ffff)
#define PROC_PIO_INSTR_MEM27_RESET  _u(0x00000000)
#define PROC_PIO_INSTR_MEM27_WIDTH  _u(32)
#define PROC_PIO_INSTR_MEM27_MSB    _u(15)
#define PROC_PIO_INSTR_MEM27_LSB    _u(0)
#define PROC_PIO_INSTR_MEM27_ACCESS "WO"
// =============================================================================
// Register    : PROC_PIO_INSTR_MEM28
// Description : Write-only access to instruction memory location 28
#define PROC_PIO_INSTR_MEM28_OFFSET _u(0x000000bc)
#define PROC_PIO_INSTR_MEM28_BITS   _u(0x0000ffff)
#define PROC_PIO_INSTR_MEM28_RESET  _u(0x00000000)
#define PROC_PIO_INSTR_MEM28_WIDTH  _u(32)
#define PROC_PIO_INSTR_MEM28_MSB    _u(15)
#define PROC_PIO_INSTR_MEM28_LSB    _u(0)
#define PROC_PIO_INSTR_MEM28_ACCESS "WO"
// =============================================================================
// Register    : PROC_PIO_INSTR_MEM29
// Description : Write-only access to instruction memory location 29
#define PROC_PIO_INSTR_MEM29_OFFSET _u(0x000000c0)
#define PROC_PIO_INSTR_MEM29_BITS   _u(0x0000ffff)
#define PROC_PIO_INSTR_MEM29_RESET  _u(0x00000000)
#define PROC_PIO_INSTR_MEM29_WIDTH  _u(32)
#define PROC_PIO_INSTR_MEM29_MSB    _u(15)
#define PROC_PIO_INSTR_MEM29_LSB    _u(0)
#define PROC_PIO_INSTR_MEM29_ACCESS "WO"
// =============================================================================
// Register    : PROC_PIO_INSTR_MEM30
// Description : Write-only access to instruction memory location 30
#define PROC_PIO_INSTR_MEM30_OFFSET _u(0x000000c4)
#define PROC_PIO_INSTR_MEM30_BITS   _u(0x0000ffff)
#define PROC_PIO_INSTR_MEM30_RESET  _u(0x00000000)
#define PROC_PIO_INSTR_MEM30_WIDTH  _u(32)
#define PROC_PIO_INSTR_MEM30_MSB    _u(15)
#define PROC_PIO_INSTR_MEM30_LSB    _u(0)
#define PROC_PIO_INSTR_MEM30_ACCESS "WO"
// =============================================================================
// Register    : PROC_PIO_INSTR_MEM31
// Description : Write-only access to instruction memory location 31
#define PROC_PIO_INSTR_MEM31_OFFSET _u(0x000000c8)
#define PROC_PIO_INSTR_MEM31_BITS   _u(0x0000ffff)
#define PROC_PIO_INSTR_MEM31_RESET  _u(0x00000000)
#define PROC_PIO_INSTR_MEM31_WIDTH  _u(32)
#define PROC_PIO_INSTR_MEM31_MSB    _u(15)
#define PROC_PIO_INSTR_MEM31_LSB    _u(0)
#define PROC_PIO_INSTR_MEM31_ACCESS "WO"
// =============================================================================
// Register    : PROC_PIO_SM0_CLKDIV
// Description : Clock divider register for state machine 0
//               Frequency = clock freq / (CLKDIV_INT + CLKDIV_FRAC / 256)
#define PROC_PIO_SM0_CLKDIV_OFFSET _u(0x000000cc)
#define PROC_PIO_SM0_CLKDIV_BITS   _u(0xffffff00)
#define PROC_PIO_SM0_CLKDIV_RESET  _u(0x00010000)
#define PROC_PIO_SM0_CLKDIV_WIDTH  _u(32)
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM0_CLKDIV_INT
// Description : Effective frequency is sysclk/int.
//               Value of 0 is interpreted as max possible value
#define PROC_PIO_SM0_CLKDIV_INT_RESET  _u(0x0001)
#define PROC_PIO_SM0_CLKDIV_INT_BITS   _u(0xffff0000)
#define PROC_PIO_SM0_CLKDIV_INT_MSB    _u(31)
#define PROC_PIO_SM0_CLKDIV_INT_LSB    _u(16)
#define PROC_PIO_SM0_CLKDIV_INT_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM0_CLKDIV_FRAC
// Description : Fractional part of clock divider
#define PROC_PIO_SM0_CLKDIV_FRAC_RESET  _u(0x00)
#define PROC_PIO_SM0_CLKDIV_FRAC_BITS   _u(0x0000ff00)
#define PROC_PIO_SM0_CLKDIV_FRAC_MSB    _u(15)
#define PROC_PIO_SM0_CLKDIV_FRAC_LSB    _u(8)
#define PROC_PIO_SM0_CLKDIV_FRAC_ACCESS "RW"
// =============================================================================
// Register    : PROC_PIO_SM0_EXECCTRL
// Description : Execution/behavioural settings for state machine 0
#define PROC_PIO_SM0_EXECCTRL_OFFSET _u(0x000000d0)
#define PROC_PIO_SM0_EXECCTRL_BITS   _u(0xffffffbf)
#define PROC_PIO_SM0_EXECCTRL_RESET  _u(0x0001f000)
#define PROC_PIO_SM0_EXECCTRL_WIDTH  _u(32)
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM0_EXECCTRL_EXEC_STALLED
// Description : An instruction written to SMx_INSTR is stalled, and latched by
//               the
//               state machine. Will clear once the instruction completes.
#define PROC_PIO_SM0_EXECCTRL_EXEC_STALLED_RESET  _u(0x0)
#define PROC_PIO_SM0_EXECCTRL_EXEC_STALLED_BITS   _u(0x80000000)
#define PROC_PIO_SM0_EXECCTRL_EXEC_STALLED_MSB    _u(31)
#define PROC_PIO_SM0_EXECCTRL_EXEC_STALLED_LSB    _u(31)
#define PROC_PIO_SM0_EXECCTRL_EXEC_STALLED_ACCESS "RO"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM0_EXECCTRL_SIDE_EN
// Description : If 1, the delay MSB is used as side-set enable, rather than a
//               side-set data bit. This allows instructions to perform side-set
//               optionally,
//               rather than on every instruction.
#define PROC_PIO_SM0_EXECCTRL_SIDE_EN_RESET  _u(0x0)
#define PROC_PIO_SM0_EXECCTRL_SIDE_EN_BITS   _u(0x40000000)
#define PROC_PIO_SM0_EXECCTRL_SIDE_EN_MSB    _u(30)
#define PROC_PIO_SM0_EXECCTRL_SIDE_EN_LSB    _u(30)
#define PROC_PIO_SM0_EXECCTRL_SIDE_EN_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM0_EXECCTRL_SIDE_PINDIR
// Description : Side-set data is asserted to pin OEs instead of pin values
#define PROC_PIO_SM0_EXECCTRL_SIDE_PINDIR_RESET  _u(0x0)
#define PROC_PIO_SM0_EXECCTRL_SIDE_PINDIR_BITS   _u(0x20000000)
#define PROC_PIO_SM0_EXECCTRL_SIDE_PINDIR_MSB    _u(29)
#define PROC_PIO_SM0_EXECCTRL_SIDE_PINDIR_LSB    _u(29)
#define PROC_PIO_SM0_EXECCTRL_SIDE_PINDIR_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM0_EXECCTRL_JMP_PIN
// Description : The GPIO number to use as condition for JMP PIN. Unaffected by
//               input mapping.
#define PROC_PIO_SM0_EXECCTRL_JMP_PIN_RESET  _u(0x00)
#define PROC_PIO_SM0_EXECCTRL_JMP_PIN_BITS   _u(0x1f000000)
#define PROC_PIO_SM0_EXECCTRL_JMP_PIN_MSB    _u(28)
#define PROC_PIO_SM0_EXECCTRL_JMP_PIN_LSB    _u(24)
#define PROC_PIO_SM0_EXECCTRL_JMP_PIN_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM0_EXECCTRL_OUT_EN_SEL
// Description : Which data bit to use for inline OUT enable
#define PROC_PIO_SM0_EXECCTRL_OUT_EN_SEL_RESET  _u(0x00)
#define PROC_PIO_SM0_EXECCTRL_OUT_EN_SEL_BITS   _u(0x00f80000)
#define PROC_PIO_SM0_EXECCTRL_OUT_EN_SEL_MSB    _u(23)
#define PROC_PIO_SM0_EXECCTRL_OUT_EN_SEL_LSB    _u(19)
#define PROC_PIO_SM0_EXECCTRL_OUT_EN_SEL_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM0_EXECCTRL_INLINE_OUT_EN
// Description : If 1, use a bit of OUT data as an auxiliary write enable
//               When used in conjunction with OUT_STICKY, writes with an enable
//               of 0 will
//               deassert the latest pin write. This can create useful
//               masking/override behaviour
//               due to the priority ordering of state machine pin writes (SM0 <
//               SM1 < ...)
#define PROC_PIO_SM0_EXECCTRL_INLINE_OUT_EN_RESET  _u(0x0)
#define PROC_PIO_SM0_EXECCTRL_INLINE_OUT_EN_BITS   _u(0x00040000)
#define PROC_PIO_SM0_EXECCTRL_INLINE_OUT_EN_MSB    _u(18)
#define PROC_PIO_SM0_EXECCTRL_INLINE_OUT_EN_LSB    _u(18)
#define PROC_PIO_SM0_EXECCTRL_INLINE_OUT_EN_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM0_EXECCTRL_OUT_STICKY
// Description : Continuously assert the most recent OUT/SET to the pins
#define PROC_PIO_SM0_EXECCTRL_OUT_STICKY_RESET  _u(0x0)
#define PROC_PIO_SM0_EXECCTRL_OUT_STICKY_BITS   _u(0x00020000)
#define PROC_PIO_SM0_EXECCTRL_OUT_STICKY_MSB    _u(17)
#define PROC_PIO_SM0_EXECCTRL_OUT_STICKY_LSB    _u(17)
#define PROC_PIO_SM0_EXECCTRL_OUT_STICKY_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM0_EXECCTRL_WRAP_TOP
// Description : After reaching this address, execution is wrapped to
//               wrap_bottom.
//               If the instruction is a jump, and the jump condition is true,
//               the jump takes priority.
#define PROC_PIO_SM0_EXECCTRL_WRAP_TOP_RESET  _u(0x1f)
#define PROC_PIO_SM0_EXECCTRL_WRAP_TOP_BITS   _u(0x0001f000)
#define PROC_PIO_SM0_EXECCTRL_WRAP_TOP_MSB    _u(16)
#define PROC_PIO_SM0_EXECCTRL_WRAP_TOP_LSB    _u(12)
#define PROC_PIO_SM0_EXECCTRL_WRAP_TOP_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM0_EXECCTRL_WRAP_BOTTOM
// Description : After reaching wrap_top, execution is wrapped to this address.
#define PROC_PIO_SM0_EXECCTRL_WRAP_BOTTOM_RESET  _u(0x00)
#define PROC_PIO_SM0_EXECCTRL_WRAP_BOTTOM_BITS   _u(0x00000f80)
#define PROC_PIO_SM0_EXECCTRL_WRAP_BOTTOM_MSB    _u(11)
#define PROC_PIO_SM0_EXECCTRL_WRAP_BOTTOM_LSB    _u(7)
#define PROC_PIO_SM0_EXECCTRL_WRAP_BOTTOM_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM0_EXECCTRL_STATUS_SEL
// Description : Comparison used for the MOV x, STATUS instruction.
//               0x0 -> All-ones if TX FIFO level < N, otherwise all-zeroes
//               0x1 -> All-ones if RX FIFO level < N, otherwise all-zeroes
#define PROC_PIO_SM0_EXECCTRL_STATUS_SEL_RESET         _u(0x0)
#define PROC_PIO_SM0_EXECCTRL_STATUS_SEL_BITS          _u(0x00000020)
#define PROC_PIO_SM0_EXECCTRL_STATUS_SEL_MSB           _u(5)
#define PROC_PIO_SM0_EXECCTRL_STATUS_SEL_LSB           _u(5)
#define PROC_PIO_SM0_EXECCTRL_STATUS_SEL_ACCESS        "RW"
#define PROC_PIO_SM0_EXECCTRL_STATUS_SEL_VALUE_TXLEVEL _u(0x0)
#define PROC_PIO_SM0_EXECCTRL_STATUS_SEL_VALUE_RXLEVEL _u(0x1)
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM0_EXECCTRL_STATUS_N
// Description : Comparison level for the MOV x, STATUS instruction
#define PROC_PIO_SM0_EXECCTRL_STATUS_N_RESET  _u(0x00)
#define PROC_PIO_SM0_EXECCTRL_STATUS_N_BITS   _u(0x0000001f)
#define PROC_PIO_SM0_EXECCTRL_STATUS_N_MSB    _u(4)
#define PROC_PIO_SM0_EXECCTRL_STATUS_N_LSB    _u(0)
#define PROC_PIO_SM0_EXECCTRL_STATUS_N_ACCESS "RW"
// =============================================================================
// Register    : PROC_PIO_SM0_SHIFTCTRL
// Description : Control behaviour of the input/output shift registers for state
//               machine 0
#define PROC_PIO_SM0_SHIFTCTRL_OFFSET _u(0x000000d4)
#define PROC_PIO_SM0_SHIFTCTRL_BITS   _u(0xffff0000)
#define PROC_PIO_SM0_SHIFTCTRL_RESET  _u(0x000c0000)
#define PROC_PIO_SM0_SHIFTCTRL_WIDTH  _u(32)
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM0_SHIFTCTRL_FJOIN_RX
// Description : When 1, RX FIFO steals the TX FIFO's storage, and becomes twice
//               as deep.
//               TX FIFO is disabled as a result (always reads as both full and
//               empty).
//               FIFOs are flushed when this bit is changed.
#define PROC_PIO_SM0_SHIFTCTRL_FJOIN_RX_RESET  _u(0x0)
#define PROC_PIO_SM0_SHIFTCTRL_FJOIN_RX_BITS   _u(0x80000000)
#define PROC_PIO_SM0_SHIFTCTRL_FJOIN_RX_MSB    _u(31)
#define PROC_PIO_SM0_SHIFTCTRL_FJOIN_RX_LSB    _u(31)
#define PROC_PIO_SM0_SHIFTCTRL_FJOIN_RX_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM0_SHIFTCTRL_FJOIN_TX
// Description : When 1, TX FIFO steals the RX FIFO's storage, and becomes twice
//               as deep.
//               RX FIFO is disabled as a result (always reads as both full and
//               empty).
//               FIFOs are flushed when this bit is changed.
#define PROC_PIO_SM0_SHIFTCTRL_FJOIN_TX_RESET  _u(0x0)
#define PROC_PIO_SM0_SHIFTCTRL_FJOIN_TX_BITS   _u(0x40000000)
#define PROC_PIO_SM0_SHIFTCTRL_FJOIN_TX_MSB    _u(30)
#define PROC_PIO_SM0_SHIFTCTRL_FJOIN_TX_LSB    _u(30)
#define PROC_PIO_SM0_SHIFTCTRL_FJOIN_TX_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM0_SHIFTCTRL_PULL_THRESH
// Description : Number of bits shifted out of TXSR before autopull or
//               conditional pull.
//               Write 0 for value of 32.
#define PROC_PIO_SM0_SHIFTCTRL_PULL_THRESH_RESET  _u(0x00)
#define PROC_PIO_SM0_SHIFTCTRL_PULL_THRESH_BITS   _u(0x3e000000)
#define PROC_PIO_SM0_SHIFTCTRL_PULL_THRESH_MSB    _u(29)
#define PROC_PIO_SM0_SHIFTCTRL_PULL_THRESH_LSB    _u(25)
#define PROC_PIO_SM0_SHIFTCTRL_PULL_THRESH_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM0_SHIFTCTRL_PUSH_THRESH
// Description : Number of bits shifted into RXSR before autopush or conditional
//               push.
//               Write 0 for value of 32.
#define PROC_PIO_SM0_SHIFTCTRL_PUSH_THRESH_RESET  _u(0x00)
#define PROC_PIO_SM0_SHIFTCTRL_PUSH_THRESH_BITS   _u(0x01f00000)
#define PROC_PIO_SM0_SHIFTCTRL_PUSH_THRESH_MSB    _u(24)
#define PROC_PIO_SM0_SHIFTCTRL_PUSH_THRESH_LSB    _u(20)
#define PROC_PIO_SM0_SHIFTCTRL_PUSH_THRESH_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM0_SHIFTCTRL_OUT_SHIFTDIR
// Description : 1 = shift out of output shift register to right. 0 = to left.
#define PROC_PIO_SM0_SHIFTCTRL_OUT_SHIFTDIR_RESET  _u(0x1)
#define PROC_PIO_SM0_SHIFTCTRL_OUT_SHIFTDIR_BITS   _u(0x00080000)
#define PROC_PIO_SM0_SHIFTCTRL_OUT_SHIFTDIR_MSB    _u(19)
#define PROC_PIO_SM0_SHIFTCTRL_OUT_SHIFTDIR_LSB    _u(19)
#define PROC_PIO_SM0_SHIFTCTRL_OUT_SHIFTDIR_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM0_SHIFTCTRL_IN_SHIFTDIR
// Description : 1 = shift input shift register to right (data enters from
//               left). 0 = to left.
#define PROC_PIO_SM0_SHIFTCTRL_IN_SHIFTDIR_RESET  _u(0x1)
#define PROC_PIO_SM0_SHIFTCTRL_IN_SHIFTDIR_BITS   _u(0x00040000)
#define PROC_PIO_SM0_SHIFTCTRL_IN_SHIFTDIR_MSB    _u(18)
#define PROC_PIO_SM0_SHIFTCTRL_IN_SHIFTDIR_LSB    _u(18)
#define PROC_PIO_SM0_SHIFTCTRL_IN_SHIFTDIR_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM0_SHIFTCTRL_AUTOPULL
// Description : Pull automatically when the output shift register is emptied
#define PROC_PIO_SM0_SHIFTCTRL_AUTOPULL_RESET  _u(0x0)
#define PROC_PIO_SM0_SHIFTCTRL_AUTOPULL_BITS   _u(0x00020000)
#define PROC_PIO_SM0_SHIFTCTRL_AUTOPULL_MSB    _u(17)
#define PROC_PIO_SM0_SHIFTCTRL_AUTOPULL_LSB    _u(17)
#define PROC_PIO_SM0_SHIFTCTRL_AUTOPULL_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM0_SHIFTCTRL_AUTOPUSH
// Description : Push automatically when the input shift register is filled
#define PROC_PIO_SM0_SHIFTCTRL_AUTOPUSH_RESET  _u(0x0)
#define PROC_PIO_SM0_SHIFTCTRL_AUTOPUSH_BITS   _u(0x00010000)
#define PROC_PIO_SM0_SHIFTCTRL_AUTOPUSH_MSB    _u(16)
#define PROC_PIO_SM0_SHIFTCTRL_AUTOPUSH_LSB    _u(16)
#define PROC_PIO_SM0_SHIFTCTRL_AUTOPUSH_ACCESS "RW"
// =============================================================================
// Register    : PROC_PIO_SM0_ADDR
// Description : Current instruction address of state machine 0
#define PROC_PIO_SM0_ADDR_OFFSET _u(0x000000d8)
#define PROC_PIO_SM0_ADDR_BITS   _u(0x0000001f)
#define PROC_PIO_SM0_ADDR_RESET  _u(0x00000000)
#define PROC_PIO_SM0_ADDR_WIDTH  _u(32)
#define PROC_PIO_SM0_ADDR_MSB    _u(4)
#define PROC_PIO_SM0_ADDR_LSB    _u(0)
#define PROC_PIO_SM0_ADDR_ACCESS "RO"
// =============================================================================
// Register    : PROC_PIO_SM0_INSTR
// Description : Instruction currently being executed by state machine 0
//               Write to execute an instruction immediately (including jumps)
//               and then resume execution.
#define PROC_PIO_SM0_INSTR_OFFSET _u(0x000000dc)
#define PROC_PIO_SM0_INSTR_BITS   _u(0x0000ffff)
#define PROC_PIO_SM0_INSTR_RESET  "-"
#define PROC_PIO_SM0_INSTR_WIDTH  _u(32)
#define PROC_PIO_SM0_INSTR_MSB    _u(15)
#define PROC_PIO_SM0_INSTR_LSB    _u(0)
#define PROC_PIO_SM0_INSTR_ACCESS "RW"
// =============================================================================
// Register    : PROC_PIO_SM0_PINCTRL
// Description : State machine pin control
#define PROC_PIO_SM0_PINCTRL_OFFSET _u(0x000000e0)
#define PROC_PIO_SM0_PINCTRL_BITS   _u(0xffffffff)
#define PROC_PIO_SM0_PINCTRL_RESET  _u(0x14000000)
#define PROC_PIO_SM0_PINCTRL_WIDTH  _u(32)
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM0_PINCTRL_SIDESET_COUNT
// Description : The number of delay bits co-opted for side-set. Inclusive of
//               the enable bit, if present.
#define PROC_PIO_SM0_PINCTRL_SIDESET_COUNT_RESET  _u(0x0)
#define PROC_PIO_SM0_PINCTRL_SIDESET_COUNT_BITS   _u(0xe0000000)
#define PROC_PIO_SM0_PINCTRL_SIDESET_COUNT_MSB    _u(31)
#define PROC_PIO_SM0_PINCTRL_SIDESET_COUNT_LSB    _u(29)
#define PROC_PIO_SM0_PINCTRL_SIDESET_COUNT_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM0_PINCTRL_SET_COUNT
// Description : The number of pins asserted by a SET. Max of 5
#define PROC_PIO_SM0_PINCTRL_SET_COUNT_RESET  _u(0x5)
#define PROC_PIO_SM0_PINCTRL_SET_COUNT_BITS   _u(0x1c000000)
#define PROC_PIO_SM0_PINCTRL_SET_COUNT_MSB    _u(28)
#define PROC_PIO_SM0_PINCTRL_SET_COUNT_LSB    _u(26)
#define PROC_PIO_SM0_PINCTRL_SET_COUNT_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM0_PINCTRL_OUT_COUNT
// Description : The number of pins asserted by an OUT. Value of 0 -> 32 pins
#define PROC_PIO_SM0_PINCTRL_OUT_COUNT_RESET  _u(0x00)
#define PROC_PIO_SM0_PINCTRL_OUT_COUNT_BITS   _u(0x03f00000)
#define PROC_PIO_SM0_PINCTRL_OUT_COUNT_MSB    _u(25)
#define PROC_PIO_SM0_PINCTRL_OUT_COUNT_LSB    _u(20)
#define PROC_PIO_SM0_PINCTRL_OUT_COUNT_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM0_PINCTRL_IN_BASE
// Description : The virtual pin corresponding to IN bit 0
#define PROC_PIO_SM0_PINCTRL_IN_BASE_RESET  _u(0x00)
#define PROC_PIO_SM0_PINCTRL_IN_BASE_BITS   _u(0x000f8000)
#define PROC_PIO_SM0_PINCTRL_IN_BASE_MSB    _u(19)
#define PROC_PIO_SM0_PINCTRL_IN_BASE_LSB    _u(15)
#define PROC_PIO_SM0_PINCTRL_IN_BASE_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM0_PINCTRL_SIDESET_BASE
// Description : The virtual pin corresponding to delay field bit 0
#define PROC_PIO_SM0_PINCTRL_SIDESET_BASE_RESET  _u(0x00)
#define PROC_PIO_SM0_PINCTRL_SIDESET_BASE_BITS   _u(0x00007c00)
#define PROC_PIO_SM0_PINCTRL_SIDESET_BASE_MSB    _u(14)
#define PROC_PIO_SM0_PINCTRL_SIDESET_BASE_LSB    _u(10)
#define PROC_PIO_SM0_PINCTRL_SIDESET_BASE_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM0_PINCTRL_SET_BASE
// Description : The virtual pin corresponding to SET bit 0
#define PROC_PIO_SM0_PINCTRL_SET_BASE_RESET  _u(0x00)
#define PROC_PIO_SM0_PINCTRL_SET_BASE_BITS   _u(0x000003e0)
#define PROC_PIO_SM0_PINCTRL_SET_BASE_MSB    _u(9)
#define PROC_PIO_SM0_PINCTRL_SET_BASE_LSB    _u(5)
#define PROC_PIO_SM0_PINCTRL_SET_BASE_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM0_PINCTRL_OUT_BASE
// Description : The virtual pin corresponding to OUT bit 0
#define PROC_PIO_SM0_PINCTRL_OUT_BASE_RESET  _u(0x00)
#define PROC_PIO_SM0_PINCTRL_OUT_BASE_BITS   _u(0x0000001f)
#define PROC_PIO_SM0_PINCTRL_OUT_BASE_MSB    _u(4)
#define PROC_PIO_SM0_PINCTRL_OUT_BASE_LSB    _u(0)
#define PROC_PIO_SM0_PINCTRL_OUT_BASE_ACCESS "RW"
// =============================================================================
// Register    : PROC_PIO_SM0_DMACTRL_TX
// Description : State machine DMA control
#define PROC_PIO_SM0_DMACTRL_TX_OFFSET _u(0x000000e4)
#define PROC_PIO_SM0_DMACTRL_TX_BITS   _u(0xc0000f9f)
#define PROC_PIO_SM0_DMACTRL_TX_RESET  _u(0x00000104)
#define PROC_PIO_SM0_DMACTRL_TX_WIDTH  _u(32)
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM0_DMACTRL_TX_DREQ_EN
// Description : 1 - Assert DREQ to DMA when fewer than fifo_threshold spaces
//               are available
//               0 - Don't assert DREQ
#define PROC_PIO_SM0_DMACTRL_TX_DREQ_EN_RESET  _u(0x0)
#define PROC_PIO_SM0_DMACTRL_TX_DREQ_EN_BITS   _u(0x80000000)
#define PROC_PIO_SM0_DMACTRL_TX_DREQ_EN_MSB    _u(31)
#define PROC_PIO_SM0_DMACTRL_TX_DREQ_EN_LSB    _u(31)
#define PROC_PIO_SM0_DMACTRL_TX_DREQ_EN_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM0_DMACTRL_TX_ACTIVE
// Description : 1 - Assert DREQ to DMA when fewer than fifo_threshold spaces
//               are available
//               0 - Don't assert DREQ
#define PROC_PIO_SM0_DMACTRL_TX_ACTIVE_RESET  "-"
#define PROC_PIO_SM0_DMACTRL_TX_ACTIVE_BITS   _u(0x40000000)
#define PROC_PIO_SM0_DMACTRL_TX_ACTIVE_MSB    _u(30)
#define PROC_PIO_SM0_DMACTRL_TX_ACTIVE_LSB    _u(30)
#define PROC_PIO_SM0_DMACTRL_TX_ACTIVE_ACCESS "RO"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM0_DMACTRL_TX_DWELL_TIME
// Description : Delay in number of bus cycles before successive DREQs are
//               generated.
//               Used to account for system bus latency in write data arriving
//               at the FIFO.
#define PROC_PIO_SM0_DMACTRL_TX_DWELL_TIME_RESET  _u(0x02)
#define PROC_PIO_SM0_DMACTRL_TX_DWELL_TIME_BITS   _u(0x00000f80)
#define PROC_PIO_SM0_DMACTRL_TX_DWELL_TIME_MSB    _u(11)
#define PROC_PIO_SM0_DMACTRL_TX_DWELL_TIME_LSB    _u(7)
#define PROC_PIO_SM0_DMACTRL_TX_DWELL_TIME_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM0_DMACTRL_TX_FIFO_THRESHOLD
// Description : Threshold control. If there are no more than THRESHOLD items in
//               the TX FIFO, DMA dreq and/or the interrupt line is asserted.
#define PROC_PIO_SM0_DMACTRL_TX_FIFO_THRESHOLD_RESET  _u(0x04)
#define PROC_PIO_SM0_DMACTRL_TX_FIFO_THRESHOLD_BITS   _u(0x0000001f)
#define PROC_PIO_SM0_DMACTRL_TX_FIFO_THRESHOLD_MSB    _u(4)
#define PROC_PIO_SM0_DMACTRL_TX_FIFO_THRESHOLD_LSB    _u(0)
#define PROC_PIO_SM0_DMACTRL_TX_FIFO_THRESHOLD_ACCESS "RW"
// =============================================================================
// Register    : PROC_PIO_SM0_DMACTRL_RX
// Description : State machine DMA control
#define PROC_PIO_SM0_DMACTRL_RX_OFFSET _u(0x000000e8)
#define PROC_PIO_SM0_DMACTRL_RX_BITS   _u(0xc0000f9f)
#define PROC_PIO_SM0_DMACTRL_RX_RESET  _u(0x00000104)
#define PROC_PIO_SM0_DMACTRL_RX_WIDTH  _u(32)
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM0_DMACTRL_RX_DREQ_EN
// Description : 1 - Assert DREQ to DMA when fewer than fifo_threshold spaces
//               are available
//               0 - Don't assert DREQ
#define PROC_PIO_SM0_DMACTRL_RX_DREQ_EN_RESET  _u(0x0)
#define PROC_PIO_SM0_DMACTRL_RX_DREQ_EN_BITS   _u(0x80000000)
#define PROC_PIO_SM0_DMACTRL_RX_DREQ_EN_MSB    _u(31)
#define PROC_PIO_SM0_DMACTRL_RX_DREQ_EN_LSB    _u(31)
#define PROC_PIO_SM0_DMACTRL_RX_DREQ_EN_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM0_DMACTRL_RX_ACTIVE
// Description : 1 - Assert DREQ to DMA when fewer than fifo_threshold spaces
//               are available
//               0 - Don't assert DREQ
#define PROC_PIO_SM0_DMACTRL_RX_ACTIVE_RESET  "-"
#define PROC_PIO_SM0_DMACTRL_RX_ACTIVE_BITS   _u(0x40000000)
#define PROC_PIO_SM0_DMACTRL_RX_ACTIVE_MSB    _u(30)
#define PROC_PIO_SM0_DMACTRL_RX_ACTIVE_LSB    _u(30)
#define PROC_PIO_SM0_DMACTRL_RX_ACTIVE_ACCESS "RO"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM0_DMACTRL_RX_DWELL_TIME
// Description : Delay in number of bus cycles before successive DREQs are
//               generated.
//               Used to account for system bus latency in write data arriving
//               at the FIFO.
#define PROC_PIO_SM0_DMACTRL_RX_DWELL_TIME_RESET  _u(0x02)
#define PROC_PIO_SM0_DMACTRL_RX_DWELL_TIME_BITS   _u(0x00000f80)
#define PROC_PIO_SM0_DMACTRL_RX_DWELL_TIME_MSB    _u(11)
#define PROC_PIO_SM0_DMACTRL_RX_DWELL_TIME_LSB    _u(7)
#define PROC_PIO_SM0_DMACTRL_RX_DWELL_TIME_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM0_DMACTRL_RX_FIFO_THRESHOLD
// Description : Threshold control. If there are at least THRESHOLD items in the
//               RX FIFO, DMA dreq and/or the interrupt line is asserted.
#define PROC_PIO_SM0_DMACTRL_RX_FIFO_THRESHOLD_RESET  _u(0x04)
#define PROC_PIO_SM0_DMACTRL_RX_FIFO_THRESHOLD_BITS   _u(0x0000001f)
#define PROC_PIO_SM0_DMACTRL_RX_FIFO_THRESHOLD_MSB    _u(4)
#define PROC_PIO_SM0_DMACTRL_RX_FIFO_THRESHOLD_LSB    _u(0)
#define PROC_PIO_SM0_DMACTRL_RX_FIFO_THRESHOLD_ACCESS "RW"
// =============================================================================
// Register    : PROC_PIO_SM1_CLKDIV
// Description : Clock divider register for state machine 1
//               Frequency = clock freq / (CLKDIV_INT + CLKDIV_FRAC / 256)
#define PROC_PIO_SM1_CLKDIV_OFFSET _u(0x000000ec)
#define PROC_PIO_SM1_CLKDIV_BITS   _u(0xffffff00)
#define PROC_PIO_SM1_CLKDIV_RESET  _u(0x00010000)
#define PROC_PIO_SM1_CLKDIV_WIDTH  _u(32)
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM1_CLKDIV_INT
// Description : Effective frequency is sysclk/int.
//               Value of 0 is interpreted as max possible value
#define PROC_PIO_SM1_CLKDIV_INT_RESET  _u(0x0001)
#define PROC_PIO_SM1_CLKDIV_INT_BITS   _u(0xffff0000)
#define PROC_PIO_SM1_CLKDIV_INT_MSB    _u(31)
#define PROC_PIO_SM1_CLKDIV_INT_LSB    _u(16)
#define PROC_PIO_SM1_CLKDIV_INT_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM1_CLKDIV_FRAC
// Description : Fractional part of clock divider
#define PROC_PIO_SM1_CLKDIV_FRAC_RESET  _u(0x00)
#define PROC_PIO_SM1_CLKDIV_FRAC_BITS   _u(0x0000ff00)
#define PROC_PIO_SM1_CLKDIV_FRAC_MSB    _u(15)
#define PROC_PIO_SM1_CLKDIV_FRAC_LSB    _u(8)
#define PROC_PIO_SM1_CLKDIV_FRAC_ACCESS "RW"
// =============================================================================
// Register    : PROC_PIO_SM1_EXECCTRL
// Description : Execution/behavioural settings for state machine 1
#define PROC_PIO_SM1_EXECCTRL_OFFSET _u(0x000000f0)
#define PROC_PIO_SM1_EXECCTRL_BITS   _u(0xffffffbf)
#define PROC_PIO_SM1_EXECCTRL_RESET  _u(0x0001f000)
#define PROC_PIO_SM1_EXECCTRL_WIDTH  _u(32)
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM1_EXECCTRL_EXEC_STALLED
// Description : An instruction written to SMx_INSTR is stalled, and latched by
//               the
//               state machine. Will clear once the instruction completes.
#define PROC_PIO_SM1_EXECCTRL_EXEC_STALLED_RESET  _u(0x0)
#define PROC_PIO_SM1_EXECCTRL_EXEC_STALLED_BITS   _u(0x80000000)
#define PROC_PIO_SM1_EXECCTRL_EXEC_STALLED_MSB    _u(31)
#define PROC_PIO_SM1_EXECCTRL_EXEC_STALLED_LSB    _u(31)
#define PROC_PIO_SM1_EXECCTRL_EXEC_STALLED_ACCESS "RO"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM1_EXECCTRL_SIDE_EN
// Description : If 1, the delay MSB is used as side-set enable, rather than a
//               side-set data bit. This allows instructions to perform side-set
//               optionally,
//               rather than on every instruction.
#define PROC_PIO_SM1_EXECCTRL_SIDE_EN_RESET  _u(0x0)
#define PROC_PIO_SM1_EXECCTRL_SIDE_EN_BITS   _u(0x40000000)
#define PROC_PIO_SM1_EXECCTRL_SIDE_EN_MSB    _u(30)
#define PROC_PIO_SM1_EXECCTRL_SIDE_EN_LSB    _u(30)
#define PROC_PIO_SM1_EXECCTRL_SIDE_EN_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM1_EXECCTRL_SIDE_PINDIR
// Description : Side-set data is asserted to pin OEs instead of pin values
#define PROC_PIO_SM1_EXECCTRL_SIDE_PINDIR_RESET  _u(0x0)
#define PROC_PIO_SM1_EXECCTRL_SIDE_PINDIR_BITS   _u(0x20000000)
#define PROC_PIO_SM1_EXECCTRL_SIDE_PINDIR_MSB    _u(29)
#define PROC_PIO_SM1_EXECCTRL_SIDE_PINDIR_LSB    _u(29)
#define PROC_PIO_SM1_EXECCTRL_SIDE_PINDIR_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM1_EXECCTRL_JMP_PIN
// Description : The GPIO number to use as condition for JMP PIN. Unaffected by
//               input mapping.
#define PROC_PIO_SM1_EXECCTRL_JMP_PIN_RESET  _u(0x00)
#define PROC_PIO_SM1_EXECCTRL_JMP_PIN_BITS   _u(0x1f000000)
#define PROC_PIO_SM1_EXECCTRL_JMP_PIN_MSB    _u(28)
#define PROC_PIO_SM1_EXECCTRL_JMP_PIN_LSB    _u(24)
#define PROC_PIO_SM1_EXECCTRL_JMP_PIN_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM1_EXECCTRL_OUT_EN_SEL
// Description : Which data bit to use for inline OUT enable
#define PROC_PIO_SM1_EXECCTRL_OUT_EN_SEL_RESET  _u(0x00)
#define PROC_PIO_SM1_EXECCTRL_OUT_EN_SEL_BITS   _u(0x00f80000)
#define PROC_PIO_SM1_EXECCTRL_OUT_EN_SEL_MSB    _u(23)
#define PROC_PIO_SM1_EXECCTRL_OUT_EN_SEL_LSB    _u(19)
#define PROC_PIO_SM1_EXECCTRL_OUT_EN_SEL_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM1_EXECCTRL_INLINE_OUT_EN
// Description : If 1, use a bit of OUT data as an auxiliary write enable
//               When used in conjunction with OUT_STICKY, writes with an enable
//               of 0 will
//               deassert the latest pin write. This can create useful
//               masking/override behaviour
//               due to the priority ordering of state machine pin writes (SM0 <
//               SM1 < ...)
#define PROC_PIO_SM1_EXECCTRL_INLINE_OUT_EN_RESET  _u(0x0)
#define PROC_PIO_SM1_EXECCTRL_INLINE_OUT_EN_BITS   _u(0x00040000)
#define PROC_PIO_SM1_EXECCTRL_INLINE_OUT_EN_MSB    _u(18)
#define PROC_PIO_SM1_EXECCTRL_INLINE_OUT_EN_LSB    _u(18)
#define PROC_PIO_SM1_EXECCTRL_INLINE_OUT_EN_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM1_EXECCTRL_OUT_STICKY
// Description : Continuously assert the most recent OUT/SET to the pins
#define PROC_PIO_SM1_EXECCTRL_OUT_STICKY_RESET  _u(0x0)
#define PROC_PIO_SM1_EXECCTRL_OUT_STICKY_BITS   _u(0x00020000)
#define PROC_PIO_SM1_EXECCTRL_OUT_STICKY_MSB    _u(17)
#define PROC_PIO_SM1_EXECCTRL_OUT_STICKY_LSB    _u(17)
#define PROC_PIO_SM1_EXECCTRL_OUT_STICKY_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM1_EXECCTRL_WRAP_TOP
// Description : After reaching this address, execution is wrapped to
//               wrap_bottom.
//               If the instruction is a jump, and the jump condition is true,
//               the jump takes priority.
#define PROC_PIO_SM1_EXECCTRL_WRAP_TOP_RESET  _u(0x1f)
#define PROC_PIO_SM1_EXECCTRL_WRAP_TOP_BITS   _u(0x0001f000)
#define PROC_PIO_SM1_EXECCTRL_WRAP_TOP_MSB    _u(16)
#define PROC_PIO_SM1_EXECCTRL_WRAP_TOP_LSB    _u(12)
#define PROC_PIO_SM1_EXECCTRL_WRAP_TOP_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM1_EXECCTRL_WRAP_BOTTOM
// Description : After reaching wrap_top, execution is wrapped to this address.
#define PROC_PIO_SM1_EXECCTRL_WRAP_BOTTOM_RESET  _u(0x00)
#define PROC_PIO_SM1_EXECCTRL_WRAP_BOTTOM_BITS   _u(0x00000f80)
#define PROC_PIO_SM1_EXECCTRL_WRAP_BOTTOM_MSB    _u(11)
#define PROC_PIO_SM1_EXECCTRL_WRAP_BOTTOM_LSB    _u(7)
#define PROC_PIO_SM1_EXECCTRL_WRAP_BOTTOM_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM1_EXECCTRL_STATUS_SEL
// Description : Comparison used for the MOV x, STATUS instruction.
//               0x0 -> All-ones if TX FIFO level < N, otherwise all-zeroes
//               0x1 -> All-ones if RX FIFO level < N, otherwise all-zeroes
#define PROC_PIO_SM1_EXECCTRL_STATUS_SEL_RESET         _u(0x0)
#define PROC_PIO_SM1_EXECCTRL_STATUS_SEL_BITS          _u(0x00000020)
#define PROC_PIO_SM1_EXECCTRL_STATUS_SEL_MSB           _u(5)
#define PROC_PIO_SM1_EXECCTRL_STATUS_SEL_LSB           _u(5)
#define PROC_PIO_SM1_EXECCTRL_STATUS_SEL_ACCESS        "RW"
#define PROC_PIO_SM1_EXECCTRL_STATUS_SEL_VALUE_TXLEVEL _u(0x0)
#define PROC_PIO_SM1_EXECCTRL_STATUS_SEL_VALUE_RXLEVEL _u(0x1)
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM1_EXECCTRL_STATUS_N
// Description : Comparison level for the MOV x, STATUS instruction
#define PROC_PIO_SM1_EXECCTRL_STATUS_N_RESET  _u(0x00)
#define PROC_PIO_SM1_EXECCTRL_STATUS_N_BITS   _u(0x0000001f)
#define PROC_PIO_SM1_EXECCTRL_STATUS_N_MSB    _u(4)
#define PROC_PIO_SM1_EXECCTRL_STATUS_N_LSB    _u(0)
#define PROC_PIO_SM1_EXECCTRL_STATUS_N_ACCESS "RW"
// =============================================================================
// Register    : PROC_PIO_SM1_SHIFTCTRL
// Description : Control behaviour of the input/output shift registers for state
//               machine 1
#define PROC_PIO_SM1_SHIFTCTRL_OFFSET _u(0x000000f4)
#define PROC_PIO_SM1_SHIFTCTRL_BITS   _u(0xffff0000)
#define PROC_PIO_SM1_SHIFTCTRL_RESET  _u(0x000c0000)
#define PROC_PIO_SM1_SHIFTCTRL_WIDTH  _u(32)
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM1_SHIFTCTRL_FJOIN_RX
// Description : When 1, RX FIFO steals the TX FIFO's storage, and becomes twice
//               as deep.
//               TX FIFO is disabled as a result (always reads as both full and
//               empty).
//               FIFOs are flushed when this bit is changed.
#define PROC_PIO_SM1_SHIFTCTRL_FJOIN_RX_RESET  _u(0x0)
#define PROC_PIO_SM1_SHIFTCTRL_FJOIN_RX_BITS   _u(0x80000000)
#define PROC_PIO_SM1_SHIFTCTRL_FJOIN_RX_MSB    _u(31)
#define PROC_PIO_SM1_SHIFTCTRL_FJOIN_RX_LSB    _u(31)
#define PROC_PIO_SM1_SHIFTCTRL_FJOIN_RX_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM1_SHIFTCTRL_FJOIN_TX
// Description : When 1, TX FIFO steals the RX FIFO's storage, and becomes twice
//               as deep.
//               RX FIFO is disabled as a result (always reads as both full and
//               empty).
//               FIFOs are flushed when this bit is changed.
#define PROC_PIO_SM1_SHIFTCTRL_FJOIN_TX_RESET  _u(0x0)
#define PROC_PIO_SM1_SHIFTCTRL_FJOIN_TX_BITS   _u(0x40000000)
#define PROC_PIO_SM1_SHIFTCTRL_FJOIN_TX_MSB    _u(30)
#define PROC_PIO_SM1_SHIFTCTRL_FJOIN_TX_LSB    _u(30)
#define PROC_PIO_SM1_SHIFTCTRL_FJOIN_TX_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM1_SHIFTCTRL_PULL_THRESH
// Description : Number of bits shifted out of TXSR before autopull or
//               conditional pull.
//               Write 0 for value of 32.
#define PROC_PIO_SM1_SHIFTCTRL_PULL_THRESH_RESET  _u(0x00)
#define PROC_PIO_SM1_SHIFTCTRL_PULL_THRESH_BITS   _u(0x3e000000)
#define PROC_PIO_SM1_SHIFTCTRL_PULL_THRESH_MSB    _u(29)
#define PROC_PIO_SM1_SHIFTCTRL_PULL_THRESH_LSB    _u(25)
#define PROC_PIO_SM1_SHIFTCTRL_PULL_THRESH_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM1_SHIFTCTRL_PUSH_THRESH
// Description : Number of bits shifted into RXSR before autopush or conditional
//               push.
//               Write 0 for value of 32.
#define PROC_PIO_SM1_SHIFTCTRL_PUSH_THRESH_RESET  _u(0x00)
#define PROC_PIO_SM1_SHIFTCTRL_PUSH_THRESH_BITS   _u(0x01f00000)
#define PROC_PIO_SM1_SHIFTCTRL_PUSH_THRESH_MSB    _u(24)
#define PROC_PIO_SM1_SHIFTCTRL_PUSH_THRESH_LSB    _u(20)
#define PROC_PIO_SM1_SHIFTCTRL_PUSH_THRESH_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM1_SHIFTCTRL_OUT_SHIFTDIR
// Description : 1 = shift out of output shift register to right. 0 = to left.
#define PROC_PIO_SM1_SHIFTCTRL_OUT_SHIFTDIR_RESET  _u(0x1)
#define PROC_PIO_SM1_SHIFTCTRL_OUT_SHIFTDIR_BITS   _u(0x00080000)
#define PROC_PIO_SM1_SHIFTCTRL_OUT_SHIFTDIR_MSB    _u(19)
#define PROC_PIO_SM1_SHIFTCTRL_OUT_SHIFTDIR_LSB    _u(19)
#define PROC_PIO_SM1_SHIFTCTRL_OUT_SHIFTDIR_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM1_SHIFTCTRL_IN_SHIFTDIR
// Description : 1 = shift input shift register to right (data enters from
//               left). 0 = to left.
#define PROC_PIO_SM1_SHIFTCTRL_IN_SHIFTDIR_RESET  _u(0x1)
#define PROC_PIO_SM1_SHIFTCTRL_IN_SHIFTDIR_BITS   _u(0x00040000)
#define PROC_PIO_SM1_SHIFTCTRL_IN_SHIFTDIR_MSB    _u(18)
#define PROC_PIO_SM1_SHIFTCTRL_IN_SHIFTDIR_LSB    _u(18)
#define PROC_PIO_SM1_SHIFTCTRL_IN_SHIFTDIR_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM1_SHIFTCTRL_AUTOPULL
// Description : Pull automatically when the output shift register is emptied
#define PROC_PIO_SM1_SHIFTCTRL_AUTOPULL_RESET  _u(0x0)
#define PROC_PIO_SM1_SHIFTCTRL_AUTOPULL_BITS   _u(0x00020000)
#define PROC_PIO_SM1_SHIFTCTRL_AUTOPULL_MSB    _u(17)
#define PROC_PIO_SM1_SHIFTCTRL_AUTOPULL_LSB    _u(17)
#define PROC_PIO_SM1_SHIFTCTRL_AUTOPULL_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM1_SHIFTCTRL_AUTOPUSH
// Description : Push automatically when the input shift register is filled
#define PROC_PIO_SM1_SHIFTCTRL_AUTOPUSH_RESET  _u(0x0)
#define PROC_PIO_SM1_SHIFTCTRL_AUTOPUSH_BITS   _u(0x00010000)
#define PROC_PIO_SM1_SHIFTCTRL_AUTOPUSH_MSB    _u(16)
#define PROC_PIO_SM1_SHIFTCTRL_AUTOPUSH_LSB    _u(16)
#define PROC_PIO_SM1_SHIFTCTRL_AUTOPUSH_ACCESS "RW"
// =============================================================================
// Register    : PROC_PIO_SM1_ADDR
// Description : Current instruction address of state machine 1
#define PROC_PIO_SM1_ADDR_OFFSET _u(0x000000f8)
#define PROC_PIO_SM1_ADDR_BITS   _u(0x0000001f)
#define PROC_PIO_SM1_ADDR_RESET  _u(0x00000000)
#define PROC_PIO_SM1_ADDR_WIDTH  _u(32)
#define PROC_PIO_SM1_ADDR_MSB    _u(4)
#define PROC_PIO_SM1_ADDR_LSB    _u(0)
#define PROC_PIO_SM1_ADDR_ACCESS "RO"
// =============================================================================
// Register    : PROC_PIO_SM1_INSTR
// Description : Instruction currently being executed by state machine 1
//               Write to execute an instruction immediately (including jumps)
//               and then resume execution.
#define PROC_PIO_SM1_INSTR_OFFSET _u(0x000000fc)
#define PROC_PIO_SM1_INSTR_BITS   _u(0x0000ffff)
#define PROC_PIO_SM1_INSTR_RESET  "-"
#define PROC_PIO_SM1_INSTR_WIDTH  _u(32)
#define PROC_PIO_SM1_INSTR_MSB    _u(15)
#define PROC_PIO_SM1_INSTR_LSB    _u(0)
#define PROC_PIO_SM1_INSTR_ACCESS "RW"
// =============================================================================
// Register    : PROC_PIO_SM1_PINCTRL
// Description : State machine pin control
#define PROC_PIO_SM1_PINCTRL_OFFSET _u(0x00000100)
#define PROC_PIO_SM1_PINCTRL_BITS   _u(0xffffffff)
#define PROC_PIO_SM1_PINCTRL_RESET  _u(0x14000000)
#define PROC_PIO_SM1_PINCTRL_WIDTH  _u(32)
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM1_PINCTRL_SIDESET_COUNT
// Description : The number of delay bits co-opted for side-set. Inclusive of
//               the enable bit, if present.
#define PROC_PIO_SM1_PINCTRL_SIDESET_COUNT_RESET  _u(0x0)
#define PROC_PIO_SM1_PINCTRL_SIDESET_COUNT_BITS   _u(0xe0000000)
#define PROC_PIO_SM1_PINCTRL_SIDESET_COUNT_MSB    _u(31)
#define PROC_PIO_SM1_PINCTRL_SIDESET_COUNT_LSB    _u(29)
#define PROC_PIO_SM1_PINCTRL_SIDESET_COUNT_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM1_PINCTRL_SET_COUNT
// Description : The number of pins asserted by a SET. Max of 5
#define PROC_PIO_SM1_PINCTRL_SET_COUNT_RESET  _u(0x5)
#define PROC_PIO_SM1_PINCTRL_SET_COUNT_BITS   _u(0x1c000000)
#define PROC_PIO_SM1_PINCTRL_SET_COUNT_MSB    _u(28)
#define PROC_PIO_SM1_PINCTRL_SET_COUNT_LSB    _u(26)
#define PROC_PIO_SM1_PINCTRL_SET_COUNT_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM1_PINCTRL_OUT_COUNT
// Description : The number of pins asserted by an OUT. Value of 0 -> 32 pins
#define PROC_PIO_SM1_PINCTRL_OUT_COUNT_RESET  _u(0x00)
#define PROC_PIO_SM1_PINCTRL_OUT_COUNT_BITS   _u(0x03f00000)
#define PROC_PIO_SM1_PINCTRL_OUT_COUNT_MSB    _u(25)
#define PROC_PIO_SM1_PINCTRL_OUT_COUNT_LSB    _u(20)
#define PROC_PIO_SM1_PINCTRL_OUT_COUNT_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM1_PINCTRL_IN_BASE
// Description : The virtual pin corresponding to IN bit 0
#define PROC_PIO_SM1_PINCTRL_IN_BASE_RESET  _u(0x00)
#define PROC_PIO_SM1_PINCTRL_IN_BASE_BITS   _u(0x000f8000)
#define PROC_PIO_SM1_PINCTRL_IN_BASE_MSB    _u(19)
#define PROC_PIO_SM1_PINCTRL_IN_BASE_LSB    _u(15)
#define PROC_PIO_SM1_PINCTRL_IN_BASE_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM1_PINCTRL_SIDESET_BASE
// Description : The virtual pin corresponding to delay field bit 0
#define PROC_PIO_SM1_PINCTRL_SIDESET_BASE_RESET  _u(0x00)
#define PROC_PIO_SM1_PINCTRL_SIDESET_BASE_BITS   _u(0x00007c00)
#define PROC_PIO_SM1_PINCTRL_SIDESET_BASE_MSB    _u(14)
#define PROC_PIO_SM1_PINCTRL_SIDESET_BASE_LSB    _u(10)
#define PROC_PIO_SM1_PINCTRL_SIDESET_BASE_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM1_PINCTRL_SET_BASE
// Description : The virtual pin corresponding to SET bit 0
#define PROC_PIO_SM1_PINCTRL_SET_BASE_RESET  _u(0x00)
#define PROC_PIO_SM1_PINCTRL_SET_BASE_BITS   _u(0x000003e0)
#define PROC_PIO_SM1_PINCTRL_SET_BASE_MSB    _u(9)
#define PROC_PIO_SM1_PINCTRL_SET_BASE_LSB    _u(5)
#define PROC_PIO_SM1_PINCTRL_SET_BASE_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM1_PINCTRL_OUT_BASE
// Description : The virtual pin corresponding to OUT bit 0
#define PROC_PIO_SM1_PINCTRL_OUT_BASE_RESET  _u(0x00)
#define PROC_PIO_SM1_PINCTRL_OUT_BASE_BITS   _u(0x0000001f)
#define PROC_PIO_SM1_PINCTRL_OUT_BASE_MSB    _u(4)
#define PROC_PIO_SM1_PINCTRL_OUT_BASE_LSB    _u(0)
#define PROC_PIO_SM1_PINCTRL_OUT_BASE_ACCESS "RW"
// =============================================================================
// Register    : PROC_PIO_SM1_DMACTRL_TX
// Description : State machine DMA control
#define PROC_PIO_SM1_DMACTRL_TX_OFFSET _u(0x00000104)
#define PROC_PIO_SM1_DMACTRL_TX_BITS   _u(0xc0000f9f)
#define PROC_PIO_SM1_DMACTRL_TX_RESET  _u(0x00000104)
#define PROC_PIO_SM1_DMACTRL_TX_WIDTH  _u(32)
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM1_DMACTRL_TX_DREQ_EN
// Description : 1 - Assert DREQ to DMA when fewer than fifo_threshold spaces
//               are available
//               0 - Don't assert DREQ
#define PROC_PIO_SM1_DMACTRL_TX_DREQ_EN_RESET  _u(0x0)
#define PROC_PIO_SM1_DMACTRL_TX_DREQ_EN_BITS   _u(0x80000000)
#define PROC_PIO_SM1_DMACTRL_TX_DREQ_EN_MSB    _u(31)
#define PROC_PIO_SM1_DMACTRL_TX_DREQ_EN_LSB    _u(31)
#define PROC_PIO_SM1_DMACTRL_TX_DREQ_EN_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM1_DMACTRL_TX_ACTIVE
// Description : 1 - Assert DREQ to DMA when fewer than fifo_threshold spaces
//               are available
//               0 - Don't assert DREQ
#define PROC_PIO_SM1_DMACTRL_TX_ACTIVE_RESET  "-"
#define PROC_PIO_SM1_DMACTRL_TX_ACTIVE_BITS   _u(0x40000000)
#define PROC_PIO_SM1_DMACTRL_TX_ACTIVE_MSB    _u(30)
#define PROC_PIO_SM1_DMACTRL_TX_ACTIVE_LSB    _u(30)
#define PROC_PIO_SM1_DMACTRL_TX_ACTIVE_ACCESS "RO"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM1_DMACTRL_TX_DWELL_TIME
// Description : Delay in number of bus cycles before successive DREQs are
//               generated.
//               Used to account for system bus latency in write data arriving
//               at the FIFO.
#define PROC_PIO_SM1_DMACTRL_TX_DWELL_TIME_RESET  _u(0x02)
#define PROC_PIO_SM1_DMACTRL_TX_DWELL_TIME_BITS   _u(0x00000f80)
#define PROC_PIO_SM1_DMACTRL_TX_DWELL_TIME_MSB    _u(11)
#define PROC_PIO_SM1_DMACTRL_TX_DWELL_TIME_LSB    _u(7)
#define PROC_PIO_SM1_DMACTRL_TX_DWELL_TIME_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM1_DMACTRL_TX_FIFO_THRESHOLD
// Description : Threshold control. If there are no more than THRESHOLD items in
//               the TX FIFO, DMA dreq and/or the interrupt line is asserted.
#define PROC_PIO_SM1_DMACTRL_TX_FIFO_THRESHOLD_RESET  _u(0x04)
#define PROC_PIO_SM1_DMACTRL_TX_FIFO_THRESHOLD_BITS   _u(0x0000001f)
#define PROC_PIO_SM1_DMACTRL_TX_FIFO_THRESHOLD_MSB    _u(4)
#define PROC_PIO_SM1_DMACTRL_TX_FIFO_THRESHOLD_LSB    _u(0)
#define PROC_PIO_SM1_DMACTRL_TX_FIFO_THRESHOLD_ACCESS "RW"
// =============================================================================
// Register    : PROC_PIO_SM1_DMACTRL_RX
// Description : State machine DMA control
#define PROC_PIO_SM1_DMACTRL_RX_OFFSET _u(0x00000108)
#define PROC_PIO_SM1_DMACTRL_RX_BITS   _u(0xc0000f9f)
#define PROC_PIO_SM1_DMACTRL_RX_RESET  _u(0x00000104)
#define PROC_PIO_SM1_DMACTRL_RX_WIDTH  _u(32)
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM1_DMACTRL_RX_DREQ_EN
// Description : 1 - Assert DREQ to DMA when fewer than fifo_threshold spaces
//               are available
//               0 - Don't assert DREQ
#define PROC_PIO_SM1_DMACTRL_RX_DREQ_EN_RESET  _u(0x0)
#define PROC_PIO_SM1_DMACTRL_RX_DREQ_EN_BITS   _u(0x80000000)
#define PROC_PIO_SM1_DMACTRL_RX_DREQ_EN_MSB    _u(31)
#define PROC_PIO_SM1_DMACTRL_RX_DREQ_EN_LSB    _u(31)
#define PROC_PIO_SM1_DMACTRL_RX_DREQ_EN_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM1_DMACTRL_RX_ACTIVE
// Description : 1 - Assert DREQ to DMA when fewer than fifo_threshold spaces
//               are available
//               0 - Don't assert DREQ
#define PROC_PIO_SM1_DMACTRL_RX_ACTIVE_RESET  "-"
#define PROC_PIO_SM1_DMACTRL_RX_ACTIVE_BITS   _u(0x40000000)
#define PROC_PIO_SM1_DMACTRL_RX_ACTIVE_MSB    _u(30)
#define PROC_PIO_SM1_DMACTRL_RX_ACTIVE_LSB    _u(30)
#define PROC_PIO_SM1_DMACTRL_RX_ACTIVE_ACCESS "RO"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM1_DMACTRL_RX_DWELL_TIME
// Description : Delay in number of bus cycles before successive DREQs are
//               generated.
//               Used to account for system bus latency in write data arriving
//               at the FIFO.
#define PROC_PIO_SM1_DMACTRL_RX_DWELL_TIME_RESET  _u(0x02)
#define PROC_PIO_SM1_DMACTRL_RX_DWELL_TIME_BITS   _u(0x00000f80)
#define PROC_PIO_SM1_DMACTRL_RX_DWELL_TIME_MSB    _u(11)
#define PROC_PIO_SM1_DMACTRL_RX_DWELL_TIME_LSB    _u(7)
#define PROC_PIO_SM1_DMACTRL_RX_DWELL_TIME_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM1_DMACTRL_RX_FIFO_THRESHOLD
// Description : Threshold control. If there are at least THRESHOLD items in the
//               RX FIFO, DMA dreq and/or the interrupt line is asserted.
#define PROC_PIO_SM1_DMACTRL_RX_FIFO_THRESHOLD_RESET  _u(0x04)
#define PROC_PIO_SM1_DMACTRL_RX_FIFO_THRESHOLD_BITS   _u(0x0000001f)
#define PROC_PIO_SM1_DMACTRL_RX_FIFO_THRESHOLD_MSB    _u(4)
#define PROC_PIO_SM1_DMACTRL_RX_FIFO_THRESHOLD_LSB    _u(0)
#define PROC_PIO_SM1_DMACTRL_RX_FIFO_THRESHOLD_ACCESS "RW"
// =============================================================================
// Register    : PROC_PIO_SM2_CLKDIV
// Description : Clock divider register for state machine 2
//               Frequency = clock freq / (CLKDIV_INT + CLKDIV_FRAC / 256)
#define PROC_PIO_SM2_CLKDIV_OFFSET _u(0x0000010c)
#define PROC_PIO_SM2_CLKDIV_BITS   _u(0xffffff00)
#define PROC_PIO_SM2_CLKDIV_RESET  _u(0x00010000)
#define PROC_PIO_SM2_CLKDIV_WIDTH  _u(32)
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM2_CLKDIV_INT
// Description : Effective frequency is sysclk/int.
//               Value of 0 is interpreted as max possible value
#define PROC_PIO_SM2_CLKDIV_INT_RESET  _u(0x0001)
#define PROC_PIO_SM2_CLKDIV_INT_BITS   _u(0xffff0000)
#define PROC_PIO_SM2_CLKDIV_INT_MSB    _u(31)
#define PROC_PIO_SM2_CLKDIV_INT_LSB    _u(16)
#define PROC_PIO_SM2_CLKDIV_INT_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM2_CLKDIV_FRAC
// Description : Fractional part of clock divider
#define PROC_PIO_SM2_CLKDIV_FRAC_RESET  _u(0x00)
#define PROC_PIO_SM2_CLKDIV_FRAC_BITS   _u(0x0000ff00)
#define PROC_PIO_SM2_CLKDIV_FRAC_MSB    _u(15)
#define PROC_PIO_SM2_CLKDIV_FRAC_LSB    _u(8)
#define PROC_PIO_SM2_CLKDIV_FRAC_ACCESS "RW"
// =============================================================================
// Register    : PROC_PIO_SM2_EXECCTRL
// Description : Execution/behavioural settings for state machine 2
#define PROC_PIO_SM2_EXECCTRL_OFFSET _u(0x00000110)
#define PROC_PIO_SM2_EXECCTRL_BITS   _u(0xffffffbf)
#define PROC_PIO_SM2_EXECCTRL_RESET  _u(0x0001f000)
#define PROC_PIO_SM2_EXECCTRL_WIDTH  _u(32)
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM2_EXECCTRL_EXEC_STALLED
// Description : An instruction written to SMx_INSTR is stalled, and latched by
//               the
//               state machine. Will clear once the instruction completes.
#define PROC_PIO_SM2_EXECCTRL_EXEC_STALLED_RESET  _u(0x0)
#define PROC_PIO_SM2_EXECCTRL_EXEC_STALLED_BITS   _u(0x80000000)
#define PROC_PIO_SM2_EXECCTRL_EXEC_STALLED_MSB    _u(31)
#define PROC_PIO_SM2_EXECCTRL_EXEC_STALLED_LSB    _u(31)
#define PROC_PIO_SM2_EXECCTRL_EXEC_STALLED_ACCESS "RO"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM2_EXECCTRL_SIDE_EN
// Description : If 1, the delay MSB is used as side-set enable, rather than a
//               side-set data bit. This allows instructions to perform side-set
//               optionally,
//               rather than on every instruction.
#define PROC_PIO_SM2_EXECCTRL_SIDE_EN_RESET  _u(0x0)
#define PROC_PIO_SM2_EXECCTRL_SIDE_EN_BITS   _u(0x40000000)
#define PROC_PIO_SM2_EXECCTRL_SIDE_EN_MSB    _u(30)
#define PROC_PIO_SM2_EXECCTRL_SIDE_EN_LSB    _u(30)
#define PROC_PIO_SM2_EXECCTRL_SIDE_EN_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM2_EXECCTRL_SIDE_PINDIR
// Description : Side-set data is asserted to pin OEs instead of pin values
#define PROC_PIO_SM2_EXECCTRL_SIDE_PINDIR_RESET  _u(0x0)
#define PROC_PIO_SM2_EXECCTRL_SIDE_PINDIR_BITS   _u(0x20000000)
#define PROC_PIO_SM2_EXECCTRL_SIDE_PINDIR_MSB    _u(29)
#define PROC_PIO_SM2_EXECCTRL_SIDE_PINDIR_LSB    _u(29)
#define PROC_PIO_SM2_EXECCTRL_SIDE_PINDIR_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM2_EXECCTRL_JMP_PIN
// Description : The GPIO number to use as condition for JMP PIN. Unaffected by
//               input mapping.
#define PROC_PIO_SM2_EXECCTRL_JMP_PIN_RESET  _u(0x00)
#define PROC_PIO_SM2_EXECCTRL_JMP_PIN_BITS   _u(0x1f000000)
#define PROC_PIO_SM2_EXECCTRL_JMP_PIN_MSB    _u(28)
#define PROC_PIO_SM2_EXECCTRL_JMP_PIN_LSB    _u(24)
#define PROC_PIO_SM2_EXECCTRL_JMP_PIN_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM2_EXECCTRL_OUT_EN_SEL
// Description : Which data bit to use for inline OUT enable
#define PROC_PIO_SM2_EXECCTRL_OUT_EN_SEL_RESET  _u(0x00)
#define PROC_PIO_SM2_EXECCTRL_OUT_EN_SEL_BITS   _u(0x00f80000)
#define PROC_PIO_SM2_EXECCTRL_OUT_EN_SEL_MSB    _u(23)
#define PROC_PIO_SM2_EXECCTRL_OUT_EN_SEL_LSB    _u(19)
#define PROC_PIO_SM2_EXECCTRL_OUT_EN_SEL_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM2_EXECCTRL_INLINE_OUT_EN
// Description : If 1, use a bit of OUT data as an auxiliary write enable
//               When used in conjunction with OUT_STICKY, writes with an enable
//               of 0 will
//               deassert the latest pin write. This can create useful
//               masking/override behaviour
//               due to the priority ordering of state machine pin writes (SM0 <
//               SM1 < ...)
#define PROC_PIO_SM2_EXECCTRL_INLINE_OUT_EN_RESET  _u(0x0)
#define PROC_PIO_SM2_EXECCTRL_INLINE_OUT_EN_BITS   _u(0x00040000)
#define PROC_PIO_SM2_EXECCTRL_INLINE_OUT_EN_MSB    _u(18)
#define PROC_PIO_SM2_EXECCTRL_INLINE_OUT_EN_LSB    _u(18)
#define PROC_PIO_SM2_EXECCTRL_INLINE_OUT_EN_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM2_EXECCTRL_OUT_STICKY
// Description : Continuously assert the most recent OUT/SET to the pins
#define PROC_PIO_SM2_EXECCTRL_OUT_STICKY_RESET  _u(0x0)
#define PROC_PIO_SM2_EXECCTRL_OUT_STICKY_BITS   _u(0x00020000)
#define PROC_PIO_SM2_EXECCTRL_OUT_STICKY_MSB    _u(17)
#define PROC_PIO_SM2_EXECCTRL_OUT_STICKY_LSB    _u(17)
#define PROC_PIO_SM2_EXECCTRL_OUT_STICKY_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM2_EXECCTRL_WRAP_TOP
// Description : After reaching this address, execution is wrapped to
//               wrap_bottom.
//               If the instruction is a jump, and the jump condition is true,
//               the jump takes priority.
#define PROC_PIO_SM2_EXECCTRL_WRAP_TOP_RESET  _u(0x1f)
#define PROC_PIO_SM2_EXECCTRL_WRAP_TOP_BITS   _u(0x0001f000)
#define PROC_PIO_SM2_EXECCTRL_WRAP_TOP_MSB    _u(16)
#define PROC_PIO_SM2_EXECCTRL_WRAP_TOP_LSB    _u(12)
#define PROC_PIO_SM2_EXECCTRL_WRAP_TOP_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM2_EXECCTRL_WRAP_BOTTOM
// Description : After reaching wrap_top, execution is wrapped to this address.
#define PROC_PIO_SM2_EXECCTRL_WRAP_BOTTOM_RESET  _u(0x00)
#define PROC_PIO_SM2_EXECCTRL_WRAP_BOTTOM_BITS   _u(0x00000f80)
#define PROC_PIO_SM2_EXECCTRL_WRAP_BOTTOM_MSB    _u(11)
#define PROC_PIO_SM2_EXECCTRL_WRAP_BOTTOM_LSB    _u(7)
#define PROC_PIO_SM2_EXECCTRL_WRAP_BOTTOM_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM2_EXECCTRL_STATUS_SEL
// Description : Comparison used for the MOV x, STATUS instruction.
//               0x0 -> All-ones if TX FIFO level < N, otherwise all-zeroes
//               0x1 -> All-ones if RX FIFO level < N, otherwise all-zeroes
#define PROC_PIO_SM2_EXECCTRL_STATUS_SEL_RESET         _u(0x0)
#define PROC_PIO_SM2_EXECCTRL_STATUS_SEL_BITS          _u(0x00000020)
#define PROC_PIO_SM2_EXECCTRL_STATUS_SEL_MSB           _u(5)
#define PROC_PIO_SM2_EXECCTRL_STATUS_SEL_LSB           _u(5)
#define PROC_PIO_SM2_EXECCTRL_STATUS_SEL_ACCESS        "RW"
#define PROC_PIO_SM2_EXECCTRL_STATUS_SEL_VALUE_TXLEVEL _u(0x0)
#define PROC_PIO_SM2_EXECCTRL_STATUS_SEL_VALUE_RXLEVEL _u(0x1)
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM2_EXECCTRL_STATUS_N
// Description : Comparison level for the MOV x, STATUS instruction
#define PROC_PIO_SM2_EXECCTRL_STATUS_N_RESET  _u(0x00)
#define PROC_PIO_SM2_EXECCTRL_STATUS_N_BITS   _u(0x0000001f)
#define PROC_PIO_SM2_EXECCTRL_STATUS_N_MSB    _u(4)
#define PROC_PIO_SM2_EXECCTRL_STATUS_N_LSB    _u(0)
#define PROC_PIO_SM2_EXECCTRL_STATUS_N_ACCESS "RW"
// =============================================================================
// Register    : PROC_PIO_SM2_SHIFTCTRL
// Description : Control behaviour of the input/output shift registers for state
//               machine 2
#define PROC_PIO_SM2_SHIFTCTRL_OFFSET _u(0x00000114)
#define PROC_PIO_SM2_SHIFTCTRL_BITS   _u(0xffff0000)
#define PROC_PIO_SM2_SHIFTCTRL_RESET  _u(0x000c0000)
#define PROC_PIO_SM2_SHIFTCTRL_WIDTH  _u(32)
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM2_SHIFTCTRL_FJOIN_RX
// Description : When 1, RX FIFO steals the TX FIFO's storage, and becomes twice
//               as deep.
//               TX FIFO is disabled as a result (always reads as both full and
//               empty).
//               FIFOs are flushed when this bit is changed.
#define PROC_PIO_SM2_SHIFTCTRL_FJOIN_RX_RESET  _u(0x0)
#define PROC_PIO_SM2_SHIFTCTRL_FJOIN_RX_BITS   _u(0x80000000)
#define PROC_PIO_SM2_SHIFTCTRL_FJOIN_RX_MSB    _u(31)
#define PROC_PIO_SM2_SHIFTCTRL_FJOIN_RX_LSB    _u(31)
#define PROC_PIO_SM2_SHIFTCTRL_FJOIN_RX_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM2_SHIFTCTRL_FJOIN_TX
// Description : When 1, TX FIFO steals the RX FIFO's storage, and becomes twice
//               as deep.
//               RX FIFO is disabled as a result (always reads as both full and
//               empty).
//               FIFOs are flushed when this bit is changed.
#define PROC_PIO_SM2_SHIFTCTRL_FJOIN_TX_RESET  _u(0x0)
#define PROC_PIO_SM2_SHIFTCTRL_FJOIN_TX_BITS   _u(0x40000000)
#define PROC_PIO_SM2_SHIFTCTRL_FJOIN_TX_MSB    _u(30)
#define PROC_PIO_SM2_SHIFTCTRL_FJOIN_TX_LSB    _u(30)
#define PROC_PIO_SM2_SHIFTCTRL_FJOIN_TX_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM2_SHIFTCTRL_PULL_THRESH
// Description : Number of bits shifted out of TXSR before autopull or
//               conditional pull.
//               Write 0 for value of 32.
#define PROC_PIO_SM2_SHIFTCTRL_PULL_THRESH_RESET  _u(0x00)
#define PROC_PIO_SM2_SHIFTCTRL_PULL_THRESH_BITS   _u(0x3e000000)
#define PROC_PIO_SM2_SHIFTCTRL_PULL_THRESH_MSB    _u(29)
#define PROC_PIO_SM2_SHIFTCTRL_PULL_THRESH_LSB    _u(25)
#define PROC_PIO_SM2_SHIFTCTRL_PULL_THRESH_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM2_SHIFTCTRL_PUSH_THRESH
// Description : Number of bits shifted into RXSR before autopush or conditional
//               push.
//               Write 0 for value of 32.
#define PROC_PIO_SM2_SHIFTCTRL_PUSH_THRESH_RESET  _u(0x00)
#define PROC_PIO_SM2_SHIFTCTRL_PUSH_THRESH_BITS   _u(0x01f00000)
#define PROC_PIO_SM2_SHIFTCTRL_PUSH_THRESH_MSB    _u(24)
#define PROC_PIO_SM2_SHIFTCTRL_PUSH_THRESH_LSB    _u(20)
#define PROC_PIO_SM2_SHIFTCTRL_PUSH_THRESH_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM2_SHIFTCTRL_OUT_SHIFTDIR
// Description : 1 = shift out of output shift register to right. 0 = to left.
#define PROC_PIO_SM2_SHIFTCTRL_OUT_SHIFTDIR_RESET  _u(0x1)
#define PROC_PIO_SM2_SHIFTCTRL_OUT_SHIFTDIR_BITS   _u(0x00080000)
#define PROC_PIO_SM2_SHIFTCTRL_OUT_SHIFTDIR_MSB    _u(19)
#define PROC_PIO_SM2_SHIFTCTRL_OUT_SHIFTDIR_LSB    _u(19)
#define PROC_PIO_SM2_SHIFTCTRL_OUT_SHIFTDIR_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM2_SHIFTCTRL_IN_SHIFTDIR
// Description : 1 = shift input shift register to right (data enters from
//               left). 0 = to left.
#define PROC_PIO_SM2_SHIFTCTRL_IN_SHIFTDIR_RESET  _u(0x1)
#define PROC_PIO_SM2_SHIFTCTRL_IN_SHIFTDIR_BITS   _u(0x00040000)
#define PROC_PIO_SM2_SHIFTCTRL_IN_SHIFTDIR_MSB    _u(18)
#define PROC_PIO_SM2_SHIFTCTRL_IN_SHIFTDIR_LSB    _u(18)
#define PROC_PIO_SM2_SHIFTCTRL_IN_SHIFTDIR_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM2_SHIFTCTRL_AUTOPULL
// Description : Pull automatically when the output shift register is emptied
#define PROC_PIO_SM2_SHIFTCTRL_AUTOPULL_RESET  _u(0x0)
#define PROC_PIO_SM2_SHIFTCTRL_AUTOPULL_BITS   _u(0x00020000)
#define PROC_PIO_SM2_SHIFTCTRL_AUTOPULL_MSB    _u(17)
#define PROC_PIO_SM2_SHIFTCTRL_AUTOPULL_LSB    _u(17)
#define PROC_PIO_SM2_SHIFTCTRL_AUTOPULL_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM2_SHIFTCTRL_AUTOPUSH
// Description : Push automatically when the input shift register is filled
#define PROC_PIO_SM2_SHIFTCTRL_AUTOPUSH_RESET  _u(0x0)
#define PROC_PIO_SM2_SHIFTCTRL_AUTOPUSH_BITS   _u(0x00010000)
#define PROC_PIO_SM2_SHIFTCTRL_AUTOPUSH_MSB    _u(16)
#define PROC_PIO_SM2_SHIFTCTRL_AUTOPUSH_LSB    _u(16)
#define PROC_PIO_SM2_SHIFTCTRL_AUTOPUSH_ACCESS "RW"
// =============================================================================
// Register    : PROC_PIO_SM2_ADDR
// Description : Current instruction address of state machine 2
#define PROC_PIO_SM2_ADDR_OFFSET _u(0x00000118)
#define PROC_PIO_SM2_ADDR_BITS   _u(0x0000001f)
#define PROC_PIO_SM2_ADDR_RESET  _u(0x00000000)
#define PROC_PIO_SM2_ADDR_WIDTH  _u(32)
#define PROC_PIO_SM2_ADDR_MSB    _u(4)
#define PROC_PIO_SM2_ADDR_LSB    _u(0)
#define PROC_PIO_SM2_ADDR_ACCESS "RO"
// =============================================================================
// Register    : PROC_PIO_SM2_INSTR
// Description : Instruction currently being executed by state machine 2
//               Write to execute an instruction immediately (including jumps)
//               and then resume execution.
#define PROC_PIO_SM2_INSTR_OFFSET _u(0x0000011c)
#define PROC_PIO_SM2_INSTR_BITS   _u(0x0000ffff)
#define PROC_PIO_SM2_INSTR_RESET  "-"
#define PROC_PIO_SM2_INSTR_WIDTH  _u(32)
#define PROC_PIO_SM2_INSTR_MSB    _u(15)
#define PROC_PIO_SM2_INSTR_LSB    _u(0)
#define PROC_PIO_SM2_INSTR_ACCESS "RW"
// =============================================================================
// Register    : PROC_PIO_SM2_PINCTRL
// Description : State machine pin control
#define PROC_PIO_SM2_PINCTRL_OFFSET _u(0x00000120)
#define PROC_PIO_SM2_PINCTRL_BITS   _u(0xffffffff)
#define PROC_PIO_SM2_PINCTRL_RESET  _u(0x14000000)
#define PROC_PIO_SM2_PINCTRL_WIDTH  _u(32)
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM2_PINCTRL_SIDESET_COUNT
// Description : The number of delay bits co-opted for side-set. Inclusive of
//               the enable bit, if present.
#define PROC_PIO_SM2_PINCTRL_SIDESET_COUNT_RESET  _u(0x0)
#define PROC_PIO_SM2_PINCTRL_SIDESET_COUNT_BITS   _u(0xe0000000)
#define PROC_PIO_SM2_PINCTRL_SIDESET_COUNT_MSB    _u(31)
#define PROC_PIO_SM2_PINCTRL_SIDESET_COUNT_LSB    _u(29)
#define PROC_PIO_SM2_PINCTRL_SIDESET_COUNT_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM2_PINCTRL_SET_COUNT
// Description : The number of pins asserted by a SET. Max of 5
#define PROC_PIO_SM2_PINCTRL_SET_COUNT_RESET  _u(0x5)
#define PROC_PIO_SM2_PINCTRL_SET_COUNT_BITS   _u(0x1c000000)
#define PROC_PIO_SM2_PINCTRL_SET_COUNT_MSB    _u(28)
#define PROC_PIO_SM2_PINCTRL_SET_COUNT_LSB    _u(26)
#define PROC_PIO_SM2_PINCTRL_SET_COUNT_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM2_PINCTRL_OUT_COUNT
// Description : The number of pins asserted by an OUT. Value of 0 -> 32 pins
#define PROC_PIO_SM2_PINCTRL_OUT_COUNT_RESET  _u(0x00)
#define PROC_PIO_SM2_PINCTRL_OUT_COUNT_BITS   _u(0x03f00000)
#define PROC_PIO_SM2_PINCTRL_OUT_COUNT_MSB    _u(25)
#define PROC_PIO_SM2_PINCTRL_OUT_COUNT_LSB    _u(20)
#define PROC_PIO_SM2_PINCTRL_OUT_COUNT_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM2_PINCTRL_IN_BASE
// Description : The virtual pin corresponding to IN bit 0
#define PROC_PIO_SM2_PINCTRL_IN_BASE_RESET  _u(0x00)
#define PROC_PIO_SM2_PINCTRL_IN_BASE_BITS   _u(0x000f8000)
#define PROC_PIO_SM2_PINCTRL_IN_BASE_MSB    _u(19)
#define PROC_PIO_SM2_PINCTRL_IN_BASE_LSB    _u(15)
#define PROC_PIO_SM2_PINCTRL_IN_BASE_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM2_PINCTRL_SIDESET_BASE
// Description : The virtual pin corresponding to delay field bit 0
#define PROC_PIO_SM2_PINCTRL_SIDESET_BASE_RESET  _u(0x00)
#define PROC_PIO_SM2_PINCTRL_SIDESET_BASE_BITS   _u(0x00007c00)
#define PROC_PIO_SM2_PINCTRL_SIDESET_BASE_MSB    _u(14)
#define PROC_PIO_SM2_PINCTRL_SIDESET_BASE_LSB    _u(10)
#define PROC_PIO_SM2_PINCTRL_SIDESET_BASE_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM2_PINCTRL_SET_BASE
// Description : The virtual pin corresponding to SET bit 0
#define PROC_PIO_SM2_PINCTRL_SET_BASE_RESET  _u(0x00)
#define PROC_PIO_SM2_PINCTRL_SET_BASE_BITS   _u(0x000003e0)
#define PROC_PIO_SM2_PINCTRL_SET_BASE_MSB    _u(9)
#define PROC_PIO_SM2_PINCTRL_SET_BASE_LSB    _u(5)
#define PROC_PIO_SM2_PINCTRL_SET_BASE_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM2_PINCTRL_OUT_BASE
// Description : The virtual pin corresponding to OUT bit 0
#define PROC_PIO_SM2_PINCTRL_OUT_BASE_RESET  _u(0x00)
#define PROC_PIO_SM2_PINCTRL_OUT_BASE_BITS   _u(0x0000001f)
#define PROC_PIO_SM2_PINCTRL_OUT_BASE_MSB    _u(4)
#define PROC_PIO_SM2_PINCTRL_OUT_BASE_LSB    _u(0)
#define PROC_PIO_SM2_PINCTRL_OUT_BASE_ACCESS "RW"
// =============================================================================
// Register    : PROC_PIO_SM2_DMACTRL_TX
// Description : State machine DMA control
#define PROC_PIO_SM2_DMACTRL_TX_OFFSET _u(0x00000124)
#define PROC_PIO_SM2_DMACTRL_TX_BITS   _u(0xc0000f9f)
#define PROC_PIO_SM2_DMACTRL_TX_RESET  _u(0x00000104)
#define PROC_PIO_SM2_DMACTRL_TX_WIDTH  _u(32)
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM2_DMACTRL_TX_DREQ_EN
// Description : 1 - Assert DREQ to DMA when fewer than fifo_threshold spaces
//               are available
//               0 - Don't assert DREQ
#define PROC_PIO_SM2_DMACTRL_TX_DREQ_EN_RESET  _u(0x0)
#define PROC_PIO_SM2_DMACTRL_TX_DREQ_EN_BITS   _u(0x80000000)
#define PROC_PIO_SM2_DMACTRL_TX_DREQ_EN_MSB    _u(31)
#define PROC_PIO_SM2_DMACTRL_TX_DREQ_EN_LSB    _u(31)
#define PROC_PIO_SM2_DMACTRL_TX_DREQ_EN_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM2_DMACTRL_TX_ACTIVE
// Description : 1 - Assert DREQ to DMA when fewer than fifo_threshold spaces
//               are available
//               0 - Don't assert DREQ
#define PROC_PIO_SM2_DMACTRL_TX_ACTIVE_RESET  "-"
#define PROC_PIO_SM2_DMACTRL_TX_ACTIVE_BITS   _u(0x40000000)
#define PROC_PIO_SM2_DMACTRL_TX_ACTIVE_MSB    _u(30)
#define PROC_PIO_SM2_DMACTRL_TX_ACTIVE_LSB    _u(30)
#define PROC_PIO_SM2_DMACTRL_TX_ACTIVE_ACCESS "RO"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM2_DMACTRL_TX_DWELL_TIME
// Description : Delay in number of bus cycles before successive DREQs are
//               generated.
//               Used to account for system bus latency in write data arriving
//               at the FIFO.
#define PROC_PIO_SM2_DMACTRL_TX_DWELL_TIME_RESET  _u(0x02)
#define PROC_PIO_SM2_DMACTRL_TX_DWELL_TIME_BITS   _u(0x00000f80)
#define PROC_PIO_SM2_DMACTRL_TX_DWELL_TIME_MSB    _u(11)
#define PROC_PIO_SM2_DMACTRL_TX_DWELL_TIME_LSB    _u(7)
#define PROC_PIO_SM2_DMACTRL_TX_DWELL_TIME_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM2_DMACTRL_TX_FIFO_THRESHOLD
// Description : Threshold control. If there are no more than THRESHOLD items in
//               the TX FIFO, DMA dreq and/or the interrupt line is asserted.
#define PROC_PIO_SM2_DMACTRL_TX_FIFO_THRESHOLD_RESET  _u(0x04)
#define PROC_PIO_SM2_DMACTRL_TX_FIFO_THRESHOLD_BITS   _u(0x0000001f)
#define PROC_PIO_SM2_DMACTRL_TX_FIFO_THRESHOLD_MSB    _u(4)
#define PROC_PIO_SM2_DMACTRL_TX_FIFO_THRESHOLD_LSB    _u(0)
#define PROC_PIO_SM2_DMACTRL_TX_FIFO_THRESHOLD_ACCESS "RW"
// =============================================================================
// Register    : PROC_PIO_SM2_DMACTRL_RX
// Description : State machine DMA control
#define PROC_PIO_SM2_DMACTRL_RX_OFFSET _u(0x00000128)
#define PROC_PIO_SM2_DMACTRL_RX_BITS   _u(0xc0000f9f)
#define PROC_PIO_SM2_DMACTRL_RX_RESET  _u(0x00000104)
#define PROC_PIO_SM2_DMACTRL_RX_WIDTH  _u(32)
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM2_DMACTRL_RX_DREQ_EN
// Description : 1 - Assert DREQ to DMA when fewer than fifo_threshold spaces
//               are available
//               0 - Don't assert DREQ
#define PROC_PIO_SM2_DMACTRL_RX_DREQ_EN_RESET  _u(0x0)
#define PROC_PIO_SM2_DMACTRL_RX_DREQ_EN_BITS   _u(0x80000000)
#define PROC_PIO_SM2_DMACTRL_RX_DREQ_EN_MSB    _u(31)
#define PROC_PIO_SM2_DMACTRL_RX_DREQ_EN_LSB    _u(31)
#define PROC_PIO_SM2_DMACTRL_RX_DREQ_EN_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM2_DMACTRL_RX_ACTIVE
// Description : 1 - Assert DREQ to DMA when fewer than fifo_threshold spaces
//               are available
//               0 - Don't assert DREQ
#define PROC_PIO_SM2_DMACTRL_RX_ACTIVE_RESET  "-"
#define PROC_PIO_SM2_DMACTRL_RX_ACTIVE_BITS   _u(0x40000000)
#define PROC_PIO_SM2_DMACTRL_RX_ACTIVE_MSB    _u(30)
#define PROC_PIO_SM2_DMACTRL_RX_ACTIVE_LSB    _u(30)
#define PROC_PIO_SM2_DMACTRL_RX_ACTIVE_ACCESS "RO"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM2_DMACTRL_RX_DWELL_TIME
// Description : Delay in number of bus cycles before successive DREQs are
//               generated.
//               Used to account for system bus latency in write data arriving
//               at the FIFO.
#define PROC_PIO_SM2_DMACTRL_RX_DWELL_TIME_RESET  _u(0x02)
#define PROC_PIO_SM2_DMACTRL_RX_DWELL_TIME_BITS   _u(0x00000f80)
#define PROC_PIO_SM2_DMACTRL_RX_DWELL_TIME_MSB    _u(11)
#define PROC_PIO_SM2_DMACTRL_RX_DWELL_TIME_LSB    _u(7)
#define PROC_PIO_SM2_DMACTRL_RX_DWELL_TIME_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM2_DMACTRL_RX_FIFO_THRESHOLD
// Description : Threshold control. If there are at least THRESHOLD items in the
//               RX FIFO, DMA dreq and/or the interrupt line is asserted.
#define PROC_PIO_SM2_DMACTRL_RX_FIFO_THRESHOLD_RESET  _u(0x04)
#define PROC_PIO_SM2_DMACTRL_RX_FIFO_THRESHOLD_BITS   _u(0x0000001f)
#define PROC_PIO_SM2_DMACTRL_RX_FIFO_THRESHOLD_MSB    _u(4)
#define PROC_PIO_SM2_DMACTRL_RX_FIFO_THRESHOLD_LSB    _u(0)
#define PROC_PIO_SM2_DMACTRL_RX_FIFO_THRESHOLD_ACCESS "RW"
// =============================================================================
// Register    : PROC_PIO_SM3_CLKDIV
// Description : Clock divider register for state machine 3
//               Frequency = clock freq / (CLKDIV_INT + CLKDIV_FRAC / 256)
#define PROC_PIO_SM3_CLKDIV_OFFSET _u(0x0000012c)
#define PROC_PIO_SM3_CLKDIV_BITS   _u(0xffffff00)
#define PROC_PIO_SM3_CLKDIV_RESET  _u(0x00010000)
#define PROC_PIO_SM3_CLKDIV_WIDTH  _u(32)
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM3_CLKDIV_INT
// Description : Effective frequency is sysclk/int.
//               Value of 0 is interpreted as max possible value
#define PROC_PIO_SM3_CLKDIV_INT_RESET  _u(0x0001)
#define PROC_PIO_SM3_CLKDIV_INT_BITS   _u(0xffff0000)
#define PROC_PIO_SM3_CLKDIV_INT_MSB    _u(31)
#define PROC_PIO_SM3_CLKDIV_INT_LSB    _u(16)
#define PROC_PIO_SM3_CLKDIV_INT_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM3_CLKDIV_FRAC
// Description : Fractional part of clock divider
#define PROC_PIO_SM3_CLKDIV_FRAC_RESET  _u(0x00)
#define PROC_PIO_SM3_CLKDIV_FRAC_BITS   _u(0x0000ff00)
#define PROC_PIO_SM3_CLKDIV_FRAC_MSB    _u(15)
#define PROC_PIO_SM3_CLKDIV_FRAC_LSB    _u(8)
#define PROC_PIO_SM3_CLKDIV_FRAC_ACCESS "RW"
// =============================================================================
// Register    : PROC_PIO_SM3_EXECCTRL
// Description : Execution/behavioural settings for state machine 3
#define PROC_PIO_SM3_EXECCTRL_OFFSET _u(0x00000130)
#define PROC_PIO_SM3_EXECCTRL_BITS   _u(0xffffffbf)
#define PROC_PIO_SM3_EXECCTRL_RESET  _u(0x0001f000)
#define PROC_PIO_SM3_EXECCTRL_WIDTH  _u(32)
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM3_EXECCTRL_EXEC_STALLED
// Description : An instruction written to SMx_INSTR is stalled, and latched by
//               the
//               state machine. Will clear once the instruction completes.
#define PROC_PIO_SM3_EXECCTRL_EXEC_STALLED_RESET  _u(0x0)
#define PROC_PIO_SM3_EXECCTRL_EXEC_STALLED_BITS   _u(0x80000000)
#define PROC_PIO_SM3_EXECCTRL_EXEC_STALLED_MSB    _u(31)
#define PROC_PIO_SM3_EXECCTRL_EXEC_STALLED_LSB    _u(31)
#define PROC_PIO_SM3_EXECCTRL_EXEC_STALLED_ACCESS "RO"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM3_EXECCTRL_SIDE_EN
// Description : If 1, the delay MSB is used as side-set enable, rather than a
//               side-set data bit. This allows instructions to perform side-set
//               optionally,
//               rather than on every instruction.
#define PROC_PIO_SM3_EXECCTRL_SIDE_EN_RESET  _u(0x0)
#define PROC_PIO_SM3_EXECCTRL_SIDE_EN_BITS   _u(0x40000000)
#define PROC_PIO_SM3_EXECCTRL_SIDE_EN_MSB    _u(30)
#define PROC_PIO_SM3_EXECCTRL_SIDE_EN_LSB    _u(30)
#define PROC_PIO_SM3_EXECCTRL_SIDE_EN_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM3_EXECCTRL_SIDE_PINDIR
// Description : Side-set data is asserted to pin OEs instead of pin values
#define PROC_PIO_SM3_EXECCTRL_SIDE_PINDIR_RESET  _u(0x0)
#define PROC_PIO_SM3_EXECCTRL_SIDE_PINDIR_BITS   _u(0x20000000)
#define PROC_PIO_SM3_EXECCTRL_SIDE_PINDIR_MSB    _u(29)
#define PROC_PIO_SM3_EXECCTRL_SIDE_PINDIR_LSB    _u(29)
#define PROC_PIO_SM3_EXECCTRL_SIDE_PINDIR_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM3_EXECCTRL_JMP_PIN
// Description : The GPIO number to use as condition for JMP PIN. Unaffected by
//               input mapping.
#define PROC_PIO_SM3_EXECCTRL_JMP_PIN_RESET  _u(0x00)
#define PROC_PIO_SM3_EXECCTRL_JMP_PIN_BITS   _u(0x1f000000)
#define PROC_PIO_SM3_EXECCTRL_JMP_PIN_MSB    _u(28)
#define PROC_PIO_SM3_EXECCTRL_JMP_PIN_LSB    _u(24)
#define PROC_PIO_SM3_EXECCTRL_JMP_PIN_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM3_EXECCTRL_OUT_EN_SEL
// Description : Which data bit to use for inline OUT enable
#define PROC_PIO_SM3_EXECCTRL_OUT_EN_SEL_RESET  _u(0x00)
#define PROC_PIO_SM3_EXECCTRL_OUT_EN_SEL_BITS   _u(0x00f80000)
#define PROC_PIO_SM3_EXECCTRL_OUT_EN_SEL_MSB    _u(23)
#define PROC_PIO_SM3_EXECCTRL_OUT_EN_SEL_LSB    _u(19)
#define PROC_PIO_SM3_EXECCTRL_OUT_EN_SEL_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM3_EXECCTRL_INLINE_OUT_EN
// Description : If 1, use a bit of OUT data as an auxiliary write enable
//               When used in conjunction with OUT_STICKY, writes with an enable
//               of 0 will
//               deassert the latest pin write. This can create useful
//               masking/override behaviour
//               due to the priority ordering of state machine pin writes (SM0 <
//               SM1 < ...)
#define PROC_PIO_SM3_EXECCTRL_INLINE_OUT_EN_RESET  _u(0x0)
#define PROC_PIO_SM3_EXECCTRL_INLINE_OUT_EN_BITS   _u(0x00040000)
#define PROC_PIO_SM3_EXECCTRL_INLINE_OUT_EN_MSB    _u(18)
#define PROC_PIO_SM3_EXECCTRL_INLINE_OUT_EN_LSB    _u(18)
#define PROC_PIO_SM3_EXECCTRL_INLINE_OUT_EN_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM3_EXECCTRL_OUT_STICKY
// Description : Continuously assert the most recent OUT/SET to the pins
#define PROC_PIO_SM3_EXECCTRL_OUT_STICKY_RESET  _u(0x0)
#define PROC_PIO_SM3_EXECCTRL_OUT_STICKY_BITS   _u(0x00020000)
#define PROC_PIO_SM3_EXECCTRL_OUT_STICKY_MSB    _u(17)
#define PROC_PIO_SM3_EXECCTRL_OUT_STICKY_LSB    _u(17)
#define PROC_PIO_SM3_EXECCTRL_OUT_STICKY_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM3_EXECCTRL_WRAP_TOP
// Description : After reaching this address, execution is wrapped to
//               wrap_bottom.
//               If the instruction is a jump, and the jump condition is true,
//               the jump takes priority.
#define PROC_PIO_SM3_EXECCTRL_WRAP_TOP_RESET  _u(0x1f)
#define PROC_PIO_SM3_EXECCTRL_WRAP_TOP_BITS   _u(0x0001f000)
#define PROC_PIO_SM3_EXECCTRL_WRAP_TOP_MSB    _u(16)
#define PROC_PIO_SM3_EXECCTRL_WRAP_TOP_LSB    _u(12)
#define PROC_PIO_SM3_EXECCTRL_WRAP_TOP_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM3_EXECCTRL_WRAP_BOTTOM
// Description : After reaching wrap_top, execution is wrapped to this address.
#define PROC_PIO_SM3_EXECCTRL_WRAP_BOTTOM_RESET  _u(0x00)
#define PROC_PIO_SM3_EXECCTRL_WRAP_BOTTOM_BITS   _u(0x00000f80)
#define PROC_PIO_SM3_EXECCTRL_WRAP_BOTTOM_MSB    _u(11)
#define PROC_PIO_SM3_EXECCTRL_WRAP_BOTTOM_LSB    _u(7)
#define PROC_PIO_SM3_EXECCTRL_WRAP_BOTTOM_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM3_EXECCTRL_STATUS_SEL
// Description : Comparison used for the MOV x, STATUS instruction.
//               0x0 -> All-ones if TX FIFO level < N, otherwise all-zeroes
//               0x1 -> All-ones if RX FIFO level < N, otherwise all-zeroes
#define PROC_PIO_SM3_EXECCTRL_STATUS_SEL_RESET         _u(0x0)
#define PROC_PIO_SM3_EXECCTRL_STATUS_SEL_BITS          _u(0x00000020)
#define PROC_PIO_SM3_EXECCTRL_STATUS_SEL_MSB           _u(5)
#define PROC_PIO_SM3_EXECCTRL_STATUS_SEL_LSB           _u(5)
#define PROC_PIO_SM3_EXECCTRL_STATUS_SEL_ACCESS        "RW"
#define PROC_PIO_SM3_EXECCTRL_STATUS_SEL_VALUE_TXLEVEL _u(0x0)
#define PROC_PIO_SM3_EXECCTRL_STATUS_SEL_VALUE_RXLEVEL _u(0x1)
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM3_EXECCTRL_STATUS_N
// Description : Comparison level for the MOV x, STATUS instruction
#define PROC_PIO_SM3_EXECCTRL_STATUS_N_RESET  _u(0x00)
#define PROC_PIO_SM3_EXECCTRL_STATUS_N_BITS   _u(0x0000001f)
#define PROC_PIO_SM3_EXECCTRL_STATUS_N_MSB    _u(4)
#define PROC_PIO_SM3_EXECCTRL_STATUS_N_LSB    _u(0)
#define PROC_PIO_SM3_EXECCTRL_STATUS_N_ACCESS "RW"
// =============================================================================
// Register    : PROC_PIO_SM3_SHIFTCTRL
// Description : Control behaviour of the input/output shift registers for state
//               machine 3
#define PROC_PIO_SM3_SHIFTCTRL_OFFSET _u(0x00000134)
#define PROC_PIO_SM3_SHIFTCTRL_BITS   _u(0xffff0000)
#define PROC_PIO_SM3_SHIFTCTRL_RESET  _u(0x000c0000)
#define PROC_PIO_SM3_SHIFTCTRL_WIDTH  _u(32)
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM3_SHIFTCTRL_FJOIN_RX
// Description : When 1, RX FIFO steals the TX FIFO's storage, and becomes twice
//               as deep.
//               TX FIFO is disabled as a result (always reads as both full and
//               empty).
//               FIFOs are flushed when this bit is changed.
#define PROC_PIO_SM3_SHIFTCTRL_FJOIN_RX_RESET  _u(0x0)
#define PROC_PIO_SM3_SHIFTCTRL_FJOIN_RX_BITS   _u(0x80000000)
#define PROC_PIO_SM3_SHIFTCTRL_FJOIN_RX_MSB    _u(31)
#define PROC_PIO_SM3_SHIFTCTRL_FJOIN_RX_LSB    _u(31)
#define PROC_PIO_SM3_SHIFTCTRL_FJOIN_RX_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM3_SHIFTCTRL_FJOIN_TX
// Description : When 1, TX FIFO steals the RX FIFO's storage, and becomes twice
//               as deep.
//               RX FIFO is disabled as a result (always reads as both full and
//               empty).
//               FIFOs are flushed when this bit is changed.
#define PROC_PIO_SM3_SHIFTCTRL_FJOIN_TX_RESET  _u(0x0)
#define PROC_PIO_SM3_SHIFTCTRL_FJOIN_TX_BITS   _u(0x40000000)
#define PROC_PIO_SM3_SHIFTCTRL_FJOIN_TX_MSB    _u(30)
#define PROC_PIO_SM3_SHIFTCTRL_FJOIN_TX_LSB    _u(30)
#define PROC_PIO_SM3_SHIFTCTRL_FJOIN_TX_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM3_SHIFTCTRL_PULL_THRESH
// Description : Number of bits shifted out of TXSR before autopull or
//               conditional pull.
//               Write 0 for value of 32.
#define PROC_PIO_SM3_SHIFTCTRL_PULL_THRESH_RESET  _u(0x00)
#define PROC_PIO_SM3_SHIFTCTRL_PULL_THRESH_BITS   _u(0x3e000000)
#define PROC_PIO_SM3_SHIFTCTRL_PULL_THRESH_MSB    _u(29)
#define PROC_PIO_SM3_SHIFTCTRL_PULL_THRESH_LSB    _u(25)
#define PROC_PIO_SM3_SHIFTCTRL_PULL_THRESH_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM3_SHIFTCTRL_PUSH_THRESH
// Description : Number of bits shifted into RXSR before autopush or conditional
//               push.
//               Write 0 for value of 32.
#define PROC_PIO_SM3_SHIFTCTRL_PUSH_THRESH_RESET  _u(0x00)
#define PROC_PIO_SM3_SHIFTCTRL_PUSH_THRESH_BITS   _u(0x01f00000)
#define PROC_PIO_SM3_SHIFTCTRL_PUSH_THRESH_MSB    _u(24)
#define PROC_PIO_SM3_SHIFTCTRL_PUSH_THRESH_LSB    _u(20)
#define PROC_PIO_SM3_SHIFTCTRL_PUSH_THRESH_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM3_SHIFTCTRL_OUT_SHIFTDIR
// Description : 1 = shift out of output shift register to right. 0 = to left.
#define PROC_PIO_SM3_SHIFTCTRL_OUT_SHIFTDIR_RESET  _u(0x1)
#define PROC_PIO_SM3_SHIFTCTRL_OUT_SHIFTDIR_BITS   _u(0x00080000)
#define PROC_PIO_SM3_SHIFTCTRL_OUT_SHIFTDIR_MSB    _u(19)
#define PROC_PIO_SM3_SHIFTCTRL_OUT_SHIFTDIR_LSB    _u(19)
#define PROC_PIO_SM3_SHIFTCTRL_OUT_SHIFTDIR_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM3_SHIFTCTRL_IN_SHIFTDIR
// Description : 1 = shift input shift register to right (data enters from
//               left). 0 = to left.
#define PROC_PIO_SM3_SHIFTCTRL_IN_SHIFTDIR_RESET  _u(0x1)
#define PROC_PIO_SM3_SHIFTCTRL_IN_SHIFTDIR_BITS   _u(0x00040000)
#define PROC_PIO_SM3_SHIFTCTRL_IN_SHIFTDIR_MSB    _u(18)
#define PROC_PIO_SM3_SHIFTCTRL_IN_SHIFTDIR_LSB    _u(18)
#define PROC_PIO_SM3_SHIFTCTRL_IN_SHIFTDIR_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM3_SHIFTCTRL_AUTOPULL
// Description : Pull automatically when the output shift register is emptied
#define PROC_PIO_SM3_SHIFTCTRL_AUTOPULL_RESET  _u(0x0)
#define PROC_PIO_SM3_SHIFTCTRL_AUTOPULL_BITS   _u(0x00020000)
#define PROC_PIO_SM3_SHIFTCTRL_AUTOPULL_MSB    _u(17)
#define PROC_PIO_SM3_SHIFTCTRL_AUTOPULL_LSB    _u(17)
#define PROC_PIO_SM3_SHIFTCTRL_AUTOPULL_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM3_SHIFTCTRL_AUTOPUSH
// Description : Push automatically when the input shift register is filled
#define PROC_PIO_SM3_SHIFTCTRL_AUTOPUSH_RESET  _u(0x0)
#define PROC_PIO_SM3_SHIFTCTRL_AUTOPUSH_BITS   _u(0x00010000)
#define PROC_PIO_SM3_SHIFTCTRL_AUTOPUSH_MSB    _u(16)
#define PROC_PIO_SM3_SHIFTCTRL_AUTOPUSH_LSB    _u(16)
#define PROC_PIO_SM3_SHIFTCTRL_AUTOPUSH_ACCESS "RW"
// =============================================================================
// Register    : PROC_PIO_SM3_ADDR
// Description : Current instruction address of state machine 3
#define PROC_PIO_SM3_ADDR_OFFSET _u(0x00000138)
#define PROC_PIO_SM3_ADDR_BITS   _u(0x0000001f)
#define PROC_PIO_SM3_ADDR_RESET  _u(0x00000000)
#define PROC_PIO_SM3_ADDR_WIDTH  _u(32)
#define PROC_PIO_SM3_ADDR_MSB    _u(4)
#define PROC_PIO_SM3_ADDR_LSB    _u(0)
#define PROC_PIO_SM3_ADDR_ACCESS "RO"
// =============================================================================
// Register    : PROC_PIO_SM3_INSTR
// Description : Instruction currently being executed by state machine 3
//               Write to execute an instruction immediately (including jumps)
//               and then resume execution.
#define PROC_PIO_SM3_INSTR_OFFSET _u(0x0000013c)
#define PROC_PIO_SM3_INSTR_BITS   _u(0x0000ffff)
#define PROC_PIO_SM3_INSTR_RESET  "-"
#define PROC_PIO_SM3_INSTR_WIDTH  _u(32)
#define PROC_PIO_SM3_INSTR_MSB    _u(15)
#define PROC_PIO_SM3_INSTR_LSB    _u(0)
#define PROC_PIO_SM3_INSTR_ACCESS "RW"
// =============================================================================
// Register    : PROC_PIO_SM3_PINCTRL
// Description : State machine pin control
#define PROC_PIO_SM3_PINCTRL_OFFSET _u(0x00000140)
#define PROC_PIO_SM3_PINCTRL_BITS   _u(0xffffffff)
#define PROC_PIO_SM3_PINCTRL_RESET  _u(0x14000000)
#define PROC_PIO_SM3_PINCTRL_WIDTH  _u(32)
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM3_PINCTRL_SIDESET_COUNT
// Description : The number of delay bits co-opted for side-set. Inclusive of
//               the enable bit, if present.
#define PROC_PIO_SM3_PINCTRL_SIDESET_COUNT_RESET  _u(0x0)
#define PROC_PIO_SM3_PINCTRL_SIDESET_COUNT_BITS   _u(0xe0000000)
#define PROC_PIO_SM3_PINCTRL_SIDESET_COUNT_MSB    _u(31)
#define PROC_PIO_SM3_PINCTRL_SIDESET_COUNT_LSB    _u(29)
#define PROC_PIO_SM3_PINCTRL_SIDESET_COUNT_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM3_PINCTRL_SET_COUNT
// Description : The number of pins asserted by a SET. Max of 5
#define PROC_PIO_SM3_PINCTRL_SET_COUNT_RESET  _u(0x5)
#define PROC_PIO_SM3_PINCTRL_SET_COUNT_BITS   _u(0x1c000000)
#define PROC_PIO_SM3_PINCTRL_SET_COUNT_MSB    _u(28)
#define PROC_PIO_SM3_PINCTRL_SET_COUNT_LSB    _u(26)
#define PROC_PIO_SM3_PINCTRL_SET_COUNT_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM3_PINCTRL_OUT_COUNT
// Description : The number of pins asserted by an OUT. Value of 0 -> 32 pins
#define PROC_PIO_SM3_PINCTRL_OUT_COUNT_RESET  _u(0x00)
#define PROC_PIO_SM3_PINCTRL_OUT_COUNT_BITS   _u(0x03f00000)
#define PROC_PIO_SM3_PINCTRL_OUT_COUNT_MSB    _u(25)
#define PROC_PIO_SM3_PINCTRL_OUT_COUNT_LSB    _u(20)
#define PROC_PIO_SM3_PINCTRL_OUT_COUNT_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM3_PINCTRL_IN_BASE
// Description : The virtual pin corresponding to IN bit 0
#define PROC_PIO_SM3_PINCTRL_IN_BASE_RESET  _u(0x00)
#define PROC_PIO_SM3_PINCTRL_IN_BASE_BITS   _u(0x000f8000)
#define PROC_PIO_SM3_PINCTRL_IN_BASE_MSB    _u(19)
#define PROC_PIO_SM3_PINCTRL_IN_BASE_LSB    _u(15)
#define PROC_PIO_SM3_PINCTRL_IN_BASE_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM3_PINCTRL_SIDESET_BASE
// Description : The virtual pin corresponding to delay field bit 0
#define PROC_PIO_SM3_PINCTRL_SIDESET_BASE_RESET  _u(0x00)
#define PROC_PIO_SM3_PINCTRL_SIDESET_BASE_BITS   _u(0x00007c00)
#define PROC_PIO_SM3_PINCTRL_SIDESET_BASE_MSB    _u(14)
#define PROC_PIO_SM3_PINCTRL_SIDESET_BASE_LSB    _u(10)
#define PROC_PIO_SM3_PINCTRL_SIDESET_BASE_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM3_PINCTRL_SET_BASE
// Description : The virtual pin corresponding to SET bit 0
#define PROC_PIO_SM3_PINCTRL_SET_BASE_RESET  _u(0x00)
#define PROC_PIO_SM3_PINCTRL_SET_BASE_BITS   _u(0x000003e0)
#define PROC_PIO_SM3_PINCTRL_SET_BASE_MSB    _u(9)
#define PROC_PIO_SM3_PINCTRL_SET_BASE_LSB    _u(5)
#define PROC_PIO_SM3_PINCTRL_SET_BASE_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM3_PINCTRL_OUT_BASE
// Description : The virtual pin corresponding to OUT bit 0
#define PROC_PIO_SM3_PINCTRL_OUT_BASE_RESET  _u(0x00)
#define PROC_PIO_SM3_PINCTRL_OUT_BASE_BITS   _u(0x0000001f)
#define PROC_PIO_SM3_PINCTRL_OUT_BASE_MSB    _u(4)
#define PROC_PIO_SM3_PINCTRL_OUT_BASE_LSB    _u(0)
#define PROC_PIO_SM3_PINCTRL_OUT_BASE_ACCESS "RW"
// =============================================================================
// Register    : PROC_PIO_SM3_DMACTRL_TX
// Description : State machine DMA control
#define PROC_PIO_SM3_DMACTRL_TX_OFFSET _u(0x00000144)
#define PROC_PIO_SM3_DMACTRL_TX_BITS   _u(0xc0000f9f)
#define PROC_PIO_SM3_DMACTRL_TX_RESET  _u(0x00000104)
#define PROC_PIO_SM3_DMACTRL_TX_WIDTH  _u(32)
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM3_DMACTRL_TX_DREQ_EN
// Description : 1 - Assert DREQ to DMA when fewer than fifo_threshold spaces
//               are available
//               0 - Don't assert DREQ
#define PROC_PIO_SM3_DMACTRL_TX_DREQ_EN_RESET  _u(0x0)
#define PROC_PIO_SM3_DMACTRL_TX_DREQ_EN_BITS   _u(0x80000000)
#define PROC_PIO_SM3_DMACTRL_TX_DREQ_EN_MSB    _u(31)
#define PROC_PIO_SM3_DMACTRL_TX_DREQ_EN_LSB    _u(31)
#define PROC_PIO_SM3_DMACTRL_TX_DREQ_EN_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM3_DMACTRL_TX_ACTIVE
// Description : 1 - Assert DREQ to DMA when fewer than fifo_threshold spaces
//               are available
//               0 - Don't assert DREQ
#define PROC_PIO_SM3_DMACTRL_TX_ACTIVE_RESET  "-"
#define PROC_PIO_SM3_DMACTRL_TX_ACTIVE_BITS   _u(0x40000000)
#define PROC_PIO_SM3_DMACTRL_TX_ACTIVE_MSB    _u(30)
#define PROC_PIO_SM3_DMACTRL_TX_ACTIVE_LSB    _u(30)
#define PROC_PIO_SM3_DMACTRL_TX_ACTIVE_ACCESS "RO"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM3_DMACTRL_TX_DWELL_TIME
// Description : Delay in number of bus cycles before successive DREQs are
//               generated.
//               Used to account for system bus latency in write data arriving
//               at the FIFO.
#define PROC_PIO_SM3_DMACTRL_TX_DWELL_TIME_RESET  _u(0x02)
#define PROC_PIO_SM3_DMACTRL_TX_DWELL_TIME_BITS   _u(0x00000f80)
#define PROC_PIO_SM3_DMACTRL_TX_DWELL_TIME_MSB    _u(11)
#define PROC_PIO_SM3_DMACTRL_TX_DWELL_TIME_LSB    _u(7)
#define PROC_PIO_SM3_DMACTRL_TX_DWELL_TIME_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM3_DMACTRL_TX_FIFO_THRESHOLD
// Description : Threshold control. If there are no more than THRESHOLD items in
//               the TX FIFO, DMA dreq and/or the interrupt line is asserted.
#define PROC_PIO_SM3_DMACTRL_TX_FIFO_THRESHOLD_RESET  _u(0x04)
#define PROC_PIO_SM3_DMACTRL_TX_FIFO_THRESHOLD_BITS   _u(0x0000001f)
#define PROC_PIO_SM3_DMACTRL_TX_FIFO_THRESHOLD_MSB    _u(4)
#define PROC_PIO_SM3_DMACTRL_TX_FIFO_THRESHOLD_LSB    _u(0)
#define PROC_PIO_SM3_DMACTRL_TX_FIFO_THRESHOLD_ACCESS "RW"
// =============================================================================
// Register    : PROC_PIO_SM3_DMACTRL_RX
// Description : State machine DMA control
#define PROC_PIO_SM3_DMACTRL_RX_OFFSET _u(0x00000148)
#define PROC_PIO_SM3_DMACTRL_RX_BITS   _u(0xc0000f9f)
#define PROC_PIO_SM3_DMACTRL_RX_RESET  _u(0x00000104)
#define PROC_PIO_SM3_DMACTRL_RX_WIDTH  _u(32)
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM3_DMACTRL_RX_DREQ_EN
// Description : 1 - Assert DREQ to DMA when fewer than fifo_threshold spaces
//               are available
//               0 - Don't assert DREQ
#define PROC_PIO_SM3_DMACTRL_RX_DREQ_EN_RESET  _u(0x0)
#define PROC_PIO_SM3_DMACTRL_RX_DREQ_EN_BITS   _u(0x80000000)
#define PROC_PIO_SM3_DMACTRL_RX_DREQ_EN_MSB    _u(31)
#define PROC_PIO_SM3_DMACTRL_RX_DREQ_EN_LSB    _u(31)
#define PROC_PIO_SM3_DMACTRL_RX_DREQ_EN_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM3_DMACTRL_RX_ACTIVE
// Description : 1 - Assert DREQ to DMA when fewer than fifo_threshold spaces
//               are available
//               0 - Don't assert DREQ
#define PROC_PIO_SM3_DMACTRL_RX_ACTIVE_RESET  "-"
#define PROC_PIO_SM3_DMACTRL_RX_ACTIVE_BITS   _u(0x40000000)
#define PROC_PIO_SM3_DMACTRL_RX_ACTIVE_MSB    _u(30)
#define PROC_PIO_SM3_DMACTRL_RX_ACTIVE_LSB    _u(30)
#define PROC_PIO_SM3_DMACTRL_RX_ACTIVE_ACCESS "RO"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM3_DMACTRL_RX_DWELL_TIME
// Description : Delay in number of bus cycles before successive DREQs are
//               generated.
//               Used to account for system bus latency in write data arriving
//               at the FIFO.
#define PROC_PIO_SM3_DMACTRL_RX_DWELL_TIME_RESET  _u(0x02)
#define PROC_PIO_SM3_DMACTRL_RX_DWELL_TIME_BITS   _u(0x00000f80)
#define PROC_PIO_SM3_DMACTRL_RX_DWELL_TIME_MSB    _u(11)
#define PROC_PIO_SM3_DMACTRL_RX_DWELL_TIME_LSB    _u(7)
#define PROC_PIO_SM3_DMACTRL_RX_DWELL_TIME_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_SM3_DMACTRL_RX_FIFO_THRESHOLD
// Description : Threshold control. If there are at least THRESHOLD items in the
//               RX FIFO, DMA dreq and/or the interrupt line is asserted.
#define PROC_PIO_SM3_DMACTRL_RX_FIFO_THRESHOLD_RESET  _u(0x04)
#define PROC_PIO_SM3_DMACTRL_RX_FIFO_THRESHOLD_BITS   _u(0x0000001f)
#define PROC_PIO_SM3_DMACTRL_RX_FIFO_THRESHOLD_MSB    _u(4)
#define PROC_PIO_SM3_DMACTRL_RX_FIFO_THRESHOLD_LSB    _u(0)
#define PROC_PIO_SM3_DMACTRL_RX_FIFO_THRESHOLD_ACCESS "RW"
// =============================================================================
// Register    : PROC_PIO_INTR
// Description : Raw Interrupts
#define PROC_PIO_INTR_OFFSET _u(0x0000014c)
#define PROC_PIO_INTR_BITS   _u(0x00000fff)
#define PROC_PIO_INTR_RESET  _u(0x00000000)
#define PROC_PIO_INTR_WIDTH  _u(32)
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_INTR_SM3
// Description : None
#define PROC_PIO_INTR_SM3_RESET  _u(0x0)
#define PROC_PIO_INTR_SM3_BITS   _u(0x00000800)
#define PROC_PIO_INTR_SM3_MSB    _u(11)
#define PROC_PIO_INTR_SM3_LSB    _u(11)
#define PROC_PIO_INTR_SM3_ACCESS "RO"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_INTR_SM2
// Description : None
#define PROC_PIO_INTR_SM2_RESET  _u(0x0)
#define PROC_PIO_INTR_SM2_BITS   _u(0x00000400)
#define PROC_PIO_INTR_SM2_MSB    _u(10)
#define PROC_PIO_INTR_SM2_LSB    _u(10)
#define PROC_PIO_INTR_SM2_ACCESS "RO"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_INTR_SM1
// Description : None
#define PROC_PIO_INTR_SM1_RESET  _u(0x0)
#define PROC_PIO_INTR_SM1_BITS   _u(0x00000200)
#define PROC_PIO_INTR_SM1_MSB    _u(9)
#define PROC_PIO_INTR_SM1_LSB    _u(9)
#define PROC_PIO_INTR_SM1_ACCESS "RO"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_INTR_SM0
// Description : None
#define PROC_PIO_INTR_SM0_RESET  _u(0x0)
#define PROC_PIO_INTR_SM0_BITS   _u(0x00000100)
#define PROC_PIO_INTR_SM0_MSB    _u(8)
#define PROC_PIO_INTR_SM0_LSB    _u(8)
#define PROC_PIO_INTR_SM0_ACCESS "RO"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_INTR_SM3_TXNFULL
// Description : None
#define PROC_PIO_INTR_SM3_TXNFULL_RESET  _u(0x0)
#define PROC_PIO_INTR_SM3_TXNFULL_BITS   _u(0x00000080)
#define PROC_PIO_INTR_SM3_TXNFULL_MSB    _u(7)
#define PROC_PIO_INTR_SM3_TXNFULL_LSB    _u(7)
#define PROC_PIO_INTR_SM3_TXNFULL_ACCESS "RO"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_INTR_SM2_TXNFULL
// Description : None
#define PROC_PIO_INTR_SM2_TXNFULL_RESET  _u(0x0)
#define PROC_PIO_INTR_SM2_TXNFULL_BITS   _u(0x00000040)
#define PROC_PIO_INTR_SM2_TXNFULL_MSB    _u(6)
#define PROC_PIO_INTR_SM2_TXNFULL_LSB    _u(6)
#define PROC_PIO_INTR_SM2_TXNFULL_ACCESS "RO"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_INTR_SM1_TXNFULL
// Description : None
#define PROC_PIO_INTR_SM1_TXNFULL_RESET  _u(0x0)
#define PROC_PIO_INTR_SM1_TXNFULL_BITS   _u(0x00000020)
#define PROC_PIO_INTR_SM1_TXNFULL_MSB    _u(5)
#define PROC_PIO_INTR_SM1_TXNFULL_LSB    _u(5)
#define PROC_PIO_INTR_SM1_TXNFULL_ACCESS "RO"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_INTR_SM0_TXNFULL
// Description : None
#define PROC_PIO_INTR_SM0_TXNFULL_RESET  _u(0x0)
#define PROC_PIO_INTR_SM0_TXNFULL_BITS   _u(0x00000010)
#define PROC_PIO_INTR_SM0_TXNFULL_MSB    _u(4)
#define PROC_PIO_INTR_SM0_TXNFULL_LSB    _u(4)
#define PROC_PIO_INTR_SM0_TXNFULL_ACCESS "RO"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_INTR_SM3_RXNEMPTY
// Description : None
#define PROC_PIO_INTR_SM3_RXNEMPTY_RESET  _u(0x0)
#define PROC_PIO_INTR_SM3_RXNEMPTY_BITS   _u(0x00000008)
#define PROC_PIO_INTR_SM3_RXNEMPTY_MSB    _u(3)
#define PROC_PIO_INTR_SM3_RXNEMPTY_LSB    _u(3)
#define PROC_PIO_INTR_SM3_RXNEMPTY_ACCESS "RO"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_INTR_SM2_RXNEMPTY
// Description : None
#define PROC_PIO_INTR_SM2_RXNEMPTY_RESET  _u(0x0)
#define PROC_PIO_INTR_SM2_RXNEMPTY_BITS   _u(0x00000004)
#define PROC_PIO_INTR_SM2_RXNEMPTY_MSB    _u(2)
#define PROC_PIO_INTR_SM2_RXNEMPTY_LSB    _u(2)
#define PROC_PIO_INTR_SM2_RXNEMPTY_ACCESS "RO"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_INTR_SM1_RXNEMPTY
// Description : None
#define PROC_PIO_INTR_SM1_RXNEMPTY_RESET  _u(0x0)
#define PROC_PIO_INTR_SM1_RXNEMPTY_BITS   _u(0x00000002)
#define PROC_PIO_INTR_SM1_RXNEMPTY_MSB    _u(1)
#define PROC_PIO_INTR_SM1_RXNEMPTY_LSB    _u(1)
#define PROC_PIO_INTR_SM1_RXNEMPTY_ACCESS "RO"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_INTR_SM0_RXNEMPTY
// Description : None
#define PROC_PIO_INTR_SM0_RXNEMPTY_RESET  _u(0x0)
#define PROC_PIO_INTR_SM0_RXNEMPTY_BITS   _u(0x00000001)
#define PROC_PIO_INTR_SM0_RXNEMPTY_MSB    _u(0)
#define PROC_PIO_INTR_SM0_RXNEMPTY_LSB    _u(0)
#define PROC_PIO_INTR_SM0_RXNEMPTY_ACCESS "RO"
// =============================================================================
// Register    : PROC_PIO_IRQ0_INTE
// Description : Interrupt Enable for irq0
#define PROC_PIO_IRQ0_INTE_OFFSET _u(0x00000150)
#define PROC_PIO_IRQ0_INTE_BITS   _u(0x00000fff)
#define PROC_PIO_IRQ0_INTE_RESET  _u(0x00000000)
#define PROC_PIO_IRQ0_INTE_WIDTH  _u(32)
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_IRQ0_INTE_SM3
// Description : None
#define PROC_PIO_IRQ0_INTE_SM3_RESET  _u(0x0)
#define PROC_PIO_IRQ0_INTE_SM3_BITS   _u(0x00000800)
#define PROC_PIO_IRQ0_INTE_SM3_MSB    _u(11)
#define PROC_PIO_IRQ0_INTE_SM3_LSB    _u(11)
#define PROC_PIO_IRQ0_INTE_SM3_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_IRQ0_INTE_SM2
// Description : None
#define PROC_PIO_IRQ0_INTE_SM2_RESET  _u(0x0)
#define PROC_PIO_IRQ0_INTE_SM2_BITS   _u(0x00000400)
#define PROC_PIO_IRQ0_INTE_SM2_MSB    _u(10)
#define PROC_PIO_IRQ0_INTE_SM2_LSB    _u(10)
#define PROC_PIO_IRQ0_INTE_SM2_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_IRQ0_INTE_SM1
// Description : None
#define PROC_PIO_IRQ0_INTE_SM1_RESET  _u(0x0)
#define PROC_PIO_IRQ0_INTE_SM1_BITS   _u(0x00000200)
#define PROC_PIO_IRQ0_INTE_SM1_MSB    _u(9)
#define PROC_PIO_IRQ0_INTE_SM1_LSB    _u(9)
#define PROC_PIO_IRQ0_INTE_SM1_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_IRQ0_INTE_SM0
// Description : None
#define PROC_PIO_IRQ0_INTE_SM0_RESET  _u(0x0)
#define PROC_PIO_IRQ0_INTE_SM0_BITS   _u(0x00000100)
#define PROC_PIO_IRQ0_INTE_SM0_MSB    _u(8)
#define PROC_PIO_IRQ0_INTE_SM0_LSB    _u(8)
#define PROC_PIO_IRQ0_INTE_SM0_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_IRQ0_INTE_SM3_TXNFULL
// Description : None
#define PROC_PIO_IRQ0_INTE_SM3_TXNFULL_RESET  _u(0x0)
#define PROC_PIO_IRQ0_INTE_SM3_TXNFULL_BITS   _u(0x00000080)
#define PROC_PIO_IRQ0_INTE_SM3_TXNFULL_MSB    _u(7)
#define PROC_PIO_IRQ0_INTE_SM3_TXNFULL_LSB    _u(7)
#define PROC_PIO_IRQ0_INTE_SM3_TXNFULL_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_IRQ0_INTE_SM2_TXNFULL
// Description : None
#define PROC_PIO_IRQ0_INTE_SM2_TXNFULL_RESET  _u(0x0)
#define PROC_PIO_IRQ0_INTE_SM2_TXNFULL_BITS   _u(0x00000040)
#define PROC_PIO_IRQ0_INTE_SM2_TXNFULL_MSB    _u(6)
#define PROC_PIO_IRQ0_INTE_SM2_TXNFULL_LSB    _u(6)
#define PROC_PIO_IRQ0_INTE_SM2_TXNFULL_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_IRQ0_INTE_SM1_TXNFULL
// Description : None
#define PROC_PIO_IRQ0_INTE_SM1_TXNFULL_RESET  _u(0x0)
#define PROC_PIO_IRQ0_INTE_SM1_TXNFULL_BITS   _u(0x00000020)
#define PROC_PIO_IRQ0_INTE_SM1_TXNFULL_MSB    _u(5)
#define PROC_PIO_IRQ0_INTE_SM1_TXNFULL_LSB    _u(5)
#define PROC_PIO_IRQ0_INTE_SM1_TXNFULL_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_IRQ0_INTE_SM0_TXNFULL
// Description : None
#define PROC_PIO_IRQ0_INTE_SM0_TXNFULL_RESET  _u(0x0)
#define PROC_PIO_IRQ0_INTE_SM0_TXNFULL_BITS   _u(0x00000010)
#define PROC_PIO_IRQ0_INTE_SM0_TXNFULL_MSB    _u(4)
#define PROC_PIO_IRQ0_INTE_SM0_TXNFULL_LSB    _u(4)
#define PROC_PIO_IRQ0_INTE_SM0_TXNFULL_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_IRQ0_INTE_SM3_RXNEMPTY
// Description : None
#define PROC_PIO_IRQ0_INTE_SM3_RXNEMPTY_RESET  _u(0x0)
#define PROC_PIO_IRQ0_INTE_SM3_RXNEMPTY_BITS   _u(0x00000008)
#define PROC_PIO_IRQ0_INTE_SM3_RXNEMPTY_MSB    _u(3)
#define PROC_PIO_IRQ0_INTE_SM3_RXNEMPTY_LSB    _u(3)
#define PROC_PIO_IRQ0_INTE_SM3_RXNEMPTY_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_IRQ0_INTE_SM2_RXNEMPTY
// Description : None
#define PROC_PIO_IRQ0_INTE_SM2_RXNEMPTY_RESET  _u(0x0)
#define PROC_PIO_IRQ0_INTE_SM2_RXNEMPTY_BITS   _u(0x00000004)
#define PROC_PIO_IRQ0_INTE_SM2_RXNEMPTY_MSB    _u(2)
#define PROC_PIO_IRQ0_INTE_SM2_RXNEMPTY_LSB    _u(2)
#define PROC_PIO_IRQ0_INTE_SM2_RXNEMPTY_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_IRQ0_INTE_SM1_RXNEMPTY
// Description : None
#define PROC_PIO_IRQ0_INTE_SM1_RXNEMPTY_RESET  _u(0x0)
#define PROC_PIO_IRQ0_INTE_SM1_RXNEMPTY_BITS   _u(0x00000002)
#define PROC_PIO_IRQ0_INTE_SM1_RXNEMPTY_MSB    _u(1)
#define PROC_PIO_IRQ0_INTE_SM1_RXNEMPTY_LSB    _u(1)
#define PROC_PIO_IRQ0_INTE_SM1_RXNEMPTY_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_IRQ0_INTE_SM0_RXNEMPTY
// Description : None
#define PROC_PIO_IRQ0_INTE_SM0_RXNEMPTY_RESET  _u(0x0)
#define PROC_PIO_IRQ0_INTE_SM0_RXNEMPTY_BITS   _u(0x00000001)
#define PROC_PIO_IRQ0_INTE_SM0_RXNEMPTY_MSB    _u(0)
#define PROC_PIO_IRQ0_INTE_SM0_RXNEMPTY_LSB    _u(0)
#define PROC_PIO_IRQ0_INTE_SM0_RXNEMPTY_ACCESS "RW"
// =============================================================================
// Register    : PROC_PIO_IRQ0_INTF
// Description : Interrupt Force for irq0
#define PROC_PIO_IRQ0_INTF_OFFSET _u(0x00000154)
#define PROC_PIO_IRQ0_INTF_BITS   _u(0x00000fff)
#define PROC_PIO_IRQ0_INTF_RESET  _u(0x00000000)
#define PROC_PIO_IRQ0_INTF_WIDTH  _u(32)
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_IRQ0_INTF_SM3
// Description : None
#define PROC_PIO_IRQ0_INTF_SM3_RESET  _u(0x0)
#define PROC_PIO_IRQ0_INTF_SM3_BITS   _u(0x00000800)
#define PROC_PIO_IRQ0_INTF_SM3_MSB    _u(11)
#define PROC_PIO_IRQ0_INTF_SM3_LSB    _u(11)
#define PROC_PIO_IRQ0_INTF_SM3_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_IRQ0_INTF_SM2
// Description : None
#define PROC_PIO_IRQ0_INTF_SM2_RESET  _u(0x0)
#define PROC_PIO_IRQ0_INTF_SM2_BITS   _u(0x00000400)
#define PROC_PIO_IRQ0_INTF_SM2_MSB    _u(10)
#define PROC_PIO_IRQ0_INTF_SM2_LSB    _u(10)
#define PROC_PIO_IRQ0_INTF_SM2_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_IRQ0_INTF_SM1
// Description : None
#define PROC_PIO_IRQ0_INTF_SM1_RESET  _u(0x0)
#define PROC_PIO_IRQ0_INTF_SM1_BITS   _u(0x00000200)
#define PROC_PIO_IRQ0_INTF_SM1_MSB    _u(9)
#define PROC_PIO_IRQ0_INTF_SM1_LSB    _u(9)
#define PROC_PIO_IRQ0_INTF_SM1_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_IRQ0_INTF_SM0
// Description : None
#define PROC_PIO_IRQ0_INTF_SM0_RESET  _u(0x0)
#define PROC_PIO_IRQ0_INTF_SM0_BITS   _u(0x00000100)
#define PROC_PIO_IRQ0_INTF_SM0_MSB    _u(8)
#define PROC_PIO_IRQ0_INTF_SM0_LSB    _u(8)
#define PROC_PIO_IRQ0_INTF_SM0_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_IRQ0_INTF_SM3_TXNFULL
// Description : None
#define PROC_PIO_IRQ0_INTF_SM3_TXNFULL_RESET  _u(0x0)
#define PROC_PIO_IRQ0_INTF_SM3_TXNFULL_BITS   _u(0x00000080)
#define PROC_PIO_IRQ0_INTF_SM3_TXNFULL_MSB    _u(7)
#define PROC_PIO_IRQ0_INTF_SM3_TXNFULL_LSB    _u(7)
#define PROC_PIO_IRQ0_INTF_SM3_TXNFULL_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_IRQ0_INTF_SM2_TXNFULL
// Description : None
#define PROC_PIO_IRQ0_INTF_SM2_TXNFULL_RESET  _u(0x0)
#define PROC_PIO_IRQ0_INTF_SM2_TXNFULL_BITS   _u(0x00000040)
#define PROC_PIO_IRQ0_INTF_SM2_TXNFULL_MSB    _u(6)
#define PROC_PIO_IRQ0_INTF_SM2_TXNFULL_LSB    _u(6)
#define PROC_PIO_IRQ0_INTF_SM2_TXNFULL_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_IRQ0_INTF_SM1_TXNFULL
// Description : None
#define PROC_PIO_IRQ0_INTF_SM1_TXNFULL_RESET  _u(0x0)
#define PROC_PIO_IRQ0_INTF_SM1_TXNFULL_BITS   _u(0x00000020)
#define PROC_PIO_IRQ0_INTF_SM1_TXNFULL_MSB    _u(5)
#define PROC_PIO_IRQ0_INTF_SM1_TXNFULL_LSB    _u(5)
#define PROC_PIO_IRQ0_INTF_SM1_TXNFULL_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_IRQ0_INTF_SM0_TXNFULL
// Description : None
#define PROC_PIO_IRQ0_INTF_SM0_TXNFULL_RESET  _u(0x0)
#define PROC_PIO_IRQ0_INTF_SM0_TXNFULL_BITS   _u(0x00000010)
#define PROC_PIO_IRQ0_INTF_SM0_TXNFULL_MSB    _u(4)
#define PROC_PIO_IRQ0_INTF_SM0_TXNFULL_LSB    _u(4)
#define PROC_PIO_IRQ0_INTF_SM0_TXNFULL_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_IRQ0_INTF_SM3_RXNEMPTY
// Description : None
#define PROC_PIO_IRQ0_INTF_SM3_RXNEMPTY_RESET  _u(0x0)
#define PROC_PIO_IRQ0_INTF_SM3_RXNEMPTY_BITS   _u(0x00000008)
#define PROC_PIO_IRQ0_INTF_SM3_RXNEMPTY_MSB    _u(3)
#define PROC_PIO_IRQ0_INTF_SM3_RXNEMPTY_LSB    _u(3)
#define PROC_PIO_IRQ0_INTF_SM3_RXNEMPTY_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_IRQ0_INTF_SM2_RXNEMPTY
// Description : None
#define PROC_PIO_IRQ0_INTF_SM2_RXNEMPTY_RESET  _u(0x0)
#define PROC_PIO_IRQ0_INTF_SM2_RXNEMPTY_BITS   _u(0x00000004)
#define PROC_PIO_IRQ0_INTF_SM2_RXNEMPTY_MSB    _u(2)
#define PROC_PIO_IRQ0_INTF_SM2_RXNEMPTY_LSB    _u(2)
#define PROC_PIO_IRQ0_INTF_SM2_RXNEMPTY_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_IRQ0_INTF_SM1_RXNEMPTY
// Description : None
#define PROC_PIO_IRQ0_INTF_SM1_RXNEMPTY_RESET  _u(0x0)
#define PROC_PIO_IRQ0_INTF_SM1_RXNEMPTY_BITS   _u(0x00000002)
#define PROC_PIO_IRQ0_INTF_SM1_RXNEMPTY_MSB    _u(1)
#define PROC_PIO_IRQ0_INTF_SM1_RXNEMPTY_LSB    _u(1)
#define PROC_PIO_IRQ0_INTF_SM1_RXNEMPTY_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_IRQ0_INTF_SM0_RXNEMPTY
// Description : None
#define PROC_PIO_IRQ0_INTF_SM0_RXNEMPTY_RESET  _u(0x0)
#define PROC_PIO_IRQ0_INTF_SM0_RXNEMPTY_BITS   _u(0x00000001)
#define PROC_PIO_IRQ0_INTF_SM0_RXNEMPTY_MSB    _u(0)
#define PROC_PIO_IRQ0_INTF_SM0_RXNEMPTY_LSB    _u(0)
#define PROC_PIO_IRQ0_INTF_SM0_RXNEMPTY_ACCESS "RW"
// =============================================================================
// Register    : PROC_PIO_IRQ0_INTS
// Description : Interrupt status after masking & forcing for irq0
#define PROC_PIO_IRQ0_INTS_OFFSET _u(0x00000158)
#define PROC_PIO_IRQ0_INTS_BITS   _u(0x00000fff)
#define PROC_PIO_IRQ0_INTS_RESET  _u(0x00000000)
#define PROC_PIO_IRQ0_INTS_WIDTH  _u(32)
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_IRQ0_INTS_SM3
// Description : None
#define PROC_PIO_IRQ0_INTS_SM3_RESET  _u(0x0)
#define PROC_PIO_IRQ0_INTS_SM3_BITS   _u(0x00000800)
#define PROC_PIO_IRQ0_INTS_SM3_MSB    _u(11)
#define PROC_PIO_IRQ0_INTS_SM3_LSB    _u(11)
#define PROC_PIO_IRQ0_INTS_SM3_ACCESS "RO"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_IRQ0_INTS_SM2
// Description : None
#define PROC_PIO_IRQ0_INTS_SM2_RESET  _u(0x0)
#define PROC_PIO_IRQ0_INTS_SM2_BITS   _u(0x00000400)
#define PROC_PIO_IRQ0_INTS_SM2_MSB    _u(10)
#define PROC_PIO_IRQ0_INTS_SM2_LSB    _u(10)
#define PROC_PIO_IRQ0_INTS_SM2_ACCESS "RO"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_IRQ0_INTS_SM1
// Description : None
#define PROC_PIO_IRQ0_INTS_SM1_RESET  _u(0x0)
#define PROC_PIO_IRQ0_INTS_SM1_BITS   _u(0x00000200)
#define PROC_PIO_IRQ0_INTS_SM1_MSB    _u(9)
#define PROC_PIO_IRQ0_INTS_SM1_LSB    _u(9)
#define PROC_PIO_IRQ0_INTS_SM1_ACCESS "RO"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_IRQ0_INTS_SM0
// Description : None
#define PROC_PIO_IRQ0_INTS_SM0_RESET  _u(0x0)
#define PROC_PIO_IRQ0_INTS_SM0_BITS   _u(0x00000100)
#define PROC_PIO_IRQ0_INTS_SM0_MSB    _u(8)
#define PROC_PIO_IRQ0_INTS_SM0_LSB    _u(8)
#define PROC_PIO_IRQ0_INTS_SM0_ACCESS "RO"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_IRQ0_INTS_SM3_TXNFULL
// Description : None
#define PROC_PIO_IRQ0_INTS_SM3_TXNFULL_RESET  _u(0x0)
#define PROC_PIO_IRQ0_INTS_SM3_TXNFULL_BITS   _u(0x00000080)
#define PROC_PIO_IRQ0_INTS_SM3_TXNFULL_MSB    _u(7)
#define PROC_PIO_IRQ0_INTS_SM3_TXNFULL_LSB    _u(7)
#define PROC_PIO_IRQ0_INTS_SM3_TXNFULL_ACCESS "RO"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_IRQ0_INTS_SM2_TXNFULL
// Description : None
#define PROC_PIO_IRQ0_INTS_SM2_TXNFULL_RESET  _u(0x0)
#define PROC_PIO_IRQ0_INTS_SM2_TXNFULL_BITS   _u(0x00000040)
#define PROC_PIO_IRQ0_INTS_SM2_TXNFULL_MSB    _u(6)
#define PROC_PIO_IRQ0_INTS_SM2_TXNFULL_LSB    _u(6)
#define PROC_PIO_IRQ0_INTS_SM2_TXNFULL_ACCESS "RO"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_IRQ0_INTS_SM1_TXNFULL
// Description : None
#define PROC_PIO_IRQ0_INTS_SM1_TXNFULL_RESET  _u(0x0)
#define PROC_PIO_IRQ0_INTS_SM1_TXNFULL_BITS   _u(0x00000020)
#define PROC_PIO_IRQ0_INTS_SM1_TXNFULL_MSB    _u(5)
#define PROC_PIO_IRQ0_INTS_SM1_TXNFULL_LSB    _u(5)
#define PROC_PIO_IRQ0_INTS_SM1_TXNFULL_ACCESS "RO"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_IRQ0_INTS_SM0_TXNFULL
// Description : None
#define PROC_PIO_IRQ0_INTS_SM0_TXNFULL_RESET  _u(0x0)
#define PROC_PIO_IRQ0_INTS_SM0_TXNFULL_BITS   _u(0x00000010)
#define PROC_PIO_IRQ0_INTS_SM0_TXNFULL_MSB    _u(4)
#define PROC_PIO_IRQ0_INTS_SM0_TXNFULL_LSB    _u(4)
#define PROC_PIO_IRQ0_INTS_SM0_TXNFULL_ACCESS "RO"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_IRQ0_INTS_SM3_RXNEMPTY
// Description : None
#define PROC_PIO_IRQ0_INTS_SM3_RXNEMPTY_RESET  _u(0x0)
#define PROC_PIO_IRQ0_INTS_SM3_RXNEMPTY_BITS   _u(0x00000008)
#define PROC_PIO_IRQ0_INTS_SM3_RXNEMPTY_MSB    _u(3)
#define PROC_PIO_IRQ0_INTS_SM3_RXNEMPTY_LSB    _u(3)
#define PROC_PIO_IRQ0_INTS_SM3_RXNEMPTY_ACCESS "RO"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_IRQ0_INTS_SM2_RXNEMPTY
// Description : None
#define PROC_PIO_IRQ0_INTS_SM2_RXNEMPTY_RESET  _u(0x0)
#define PROC_PIO_IRQ0_INTS_SM2_RXNEMPTY_BITS   _u(0x00000004)
#define PROC_PIO_IRQ0_INTS_SM2_RXNEMPTY_MSB    _u(2)
#define PROC_PIO_IRQ0_INTS_SM2_RXNEMPTY_LSB    _u(2)
#define PROC_PIO_IRQ0_INTS_SM2_RXNEMPTY_ACCESS "RO"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_IRQ0_INTS_SM1_RXNEMPTY
// Description : None
#define PROC_PIO_IRQ0_INTS_SM1_RXNEMPTY_RESET  _u(0x0)
#define PROC_PIO_IRQ0_INTS_SM1_RXNEMPTY_BITS   _u(0x00000002)
#define PROC_PIO_IRQ0_INTS_SM1_RXNEMPTY_MSB    _u(1)
#define PROC_PIO_IRQ0_INTS_SM1_RXNEMPTY_LSB    _u(1)
#define PROC_PIO_IRQ0_INTS_SM1_RXNEMPTY_ACCESS "RO"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_IRQ0_INTS_SM0_RXNEMPTY
// Description : None
#define PROC_PIO_IRQ0_INTS_SM0_RXNEMPTY_RESET  _u(0x0)
#define PROC_PIO_IRQ0_INTS_SM0_RXNEMPTY_BITS   _u(0x00000001)
#define PROC_PIO_IRQ0_INTS_SM0_RXNEMPTY_MSB    _u(0)
#define PROC_PIO_IRQ0_INTS_SM0_RXNEMPTY_LSB    _u(0)
#define PROC_PIO_IRQ0_INTS_SM0_RXNEMPTY_ACCESS "RO"
// =============================================================================
// Register    : PROC_PIO_IRQ1_INTE
// Description : Interrupt Enable for irq1
#define PROC_PIO_IRQ1_INTE_OFFSET _u(0x0000015c)
#define PROC_PIO_IRQ1_INTE_BITS   _u(0x00000fff)
#define PROC_PIO_IRQ1_INTE_RESET  _u(0x00000000)
#define PROC_PIO_IRQ1_INTE_WIDTH  _u(32)
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_IRQ1_INTE_SM3
// Description : None
#define PROC_PIO_IRQ1_INTE_SM3_RESET  _u(0x0)
#define PROC_PIO_IRQ1_INTE_SM3_BITS   _u(0x00000800)
#define PROC_PIO_IRQ1_INTE_SM3_MSB    _u(11)
#define PROC_PIO_IRQ1_INTE_SM3_LSB    _u(11)
#define PROC_PIO_IRQ1_INTE_SM3_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_IRQ1_INTE_SM2
// Description : None
#define PROC_PIO_IRQ1_INTE_SM2_RESET  _u(0x0)
#define PROC_PIO_IRQ1_INTE_SM2_BITS   _u(0x00000400)
#define PROC_PIO_IRQ1_INTE_SM2_MSB    _u(10)
#define PROC_PIO_IRQ1_INTE_SM2_LSB    _u(10)
#define PROC_PIO_IRQ1_INTE_SM2_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_IRQ1_INTE_SM1
// Description : None
#define PROC_PIO_IRQ1_INTE_SM1_RESET  _u(0x0)
#define PROC_PIO_IRQ1_INTE_SM1_BITS   _u(0x00000200)
#define PROC_PIO_IRQ1_INTE_SM1_MSB    _u(9)
#define PROC_PIO_IRQ1_INTE_SM1_LSB    _u(9)
#define PROC_PIO_IRQ1_INTE_SM1_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_IRQ1_INTE_SM0
// Description : None
#define PROC_PIO_IRQ1_INTE_SM0_RESET  _u(0x0)
#define PROC_PIO_IRQ1_INTE_SM0_BITS   _u(0x00000100)
#define PROC_PIO_IRQ1_INTE_SM0_MSB    _u(8)
#define PROC_PIO_IRQ1_INTE_SM0_LSB    _u(8)
#define PROC_PIO_IRQ1_INTE_SM0_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_IRQ1_INTE_SM3_TXNFULL
// Description : None
#define PROC_PIO_IRQ1_INTE_SM3_TXNFULL_RESET  _u(0x0)
#define PROC_PIO_IRQ1_INTE_SM3_TXNFULL_BITS   _u(0x00000080)
#define PROC_PIO_IRQ1_INTE_SM3_TXNFULL_MSB    _u(7)
#define PROC_PIO_IRQ1_INTE_SM3_TXNFULL_LSB    _u(7)
#define PROC_PIO_IRQ1_INTE_SM3_TXNFULL_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_IRQ1_INTE_SM2_TXNFULL
// Description : None
#define PROC_PIO_IRQ1_INTE_SM2_TXNFULL_RESET  _u(0x0)
#define PROC_PIO_IRQ1_INTE_SM2_TXNFULL_BITS   _u(0x00000040)
#define PROC_PIO_IRQ1_INTE_SM2_TXNFULL_MSB    _u(6)
#define PROC_PIO_IRQ1_INTE_SM2_TXNFULL_LSB    _u(6)
#define PROC_PIO_IRQ1_INTE_SM2_TXNFULL_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_IRQ1_INTE_SM1_TXNFULL
// Description : None
#define PROC_PIO_IRQ1_INTE_SM1_TXNFULL_RESET  _u(0x0)
#define PROC_PIO_IRQ1_INTE_SM1_TXNFULL_BITS   _u(0x00000020)
#define PROC_PIO_IRQ1_INTE_SM1_TXNFULL_MSB    _u(5)
#define PROC_PIO_IRQ1_INTE_SM1_TXNFULL_LSB    _u(5)
#define PROC_PIO_IRQ1_INTE_SM1_TXNFULL_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_IRQ1_INTE_SM0_TXNFULL
// Description : None
#define PROC_PIO_IRQ1_INTE_SM0_TXNFULL_RESET  _u(0x0)
#define PROC_PIO_IRQ1_INTE_SM0_TXNFULL_BITS   _u(0x00000010)
#define PROC_PIO_IRQ1_INTE_SM0_TXNFULL_MSB    _u(4)
#define PROC_PIO_IRQ1_INTE_SM0_TXNFULL_LSB    _u(4)
#define PROC_PIO_IRQ1_INTE_SM0_TXNFULL_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_IRQ1_INTE_SM3_RXNEMPTY
// Description : None
#define PROC_PIO_IRQ1_INTE_SM3_RXNEMPTY_RESET  _u(0x0)
#define PROC_PIO_IRQ1_INTE_SM3_RXNEMPTY_BITS   _u(0x00000008)
#define PROC_PIO_IRQ1_INTE_SM3_RXNEMPTY_MSB    _u(3)
#define PROC_PIO_IRQ1_INTE_SM3_RXNEMPTY_LSB    _u(3)
#define PROC_PIO_IRQ1_INTE_SM3_RXNEMPTY_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_IRQ1_INTE_SM2_RXNEMPTY
// Description : None
#define PROC_PIO_IRQ1_INTE_SM2_RXNEMPTY_RESET  _u(0x0)
#define PROC_PIO_IRQ1_INTE_SM2_RXNEMPTY_BITS   _u(0x00000004)
#define PROC_PIO_IRQ1_INTE_SM2_RXNEMPTY_MSB    _u(2)
#define PROC_PIO_IRQ1_INTE_SM2_RXNEMPTY_LSB    _u(2)
#define PROC_PIO_IRQ1_INTE_SM2_RXNEMPTY_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_IRQ1_INTE_SM1_RXNEMPTY
// Description : None
#define PROC_PIO_IRQ1_INTE_SM1_RXNEMPTY_RESET  _u(0x0)
#define PROC_PIO_IRQ1_INTE_SM1_RXNEMPTY_BITS   _u(0x00000002)
#define PROC_PIO_IRQ1_INTE_SM1_RXNEMPTY_MSB    _u(1)
#define PROC_PIO_IRQ1_INTE_SM1_RXNEMPTY_LSB    _u(1)
#define PROC_PIO_IRQ1_INTE_SM1_RXNEMPTY_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_IRQ1_INTE_SM0_RXNEMPTY
// Description : None
#define PROC_PIO_IRQ1_INTE_SM0_RXNEMPTY_RESET  _u(0x0)
#define PROC_PIO_IRQ1_INTE_SM0_RXNEMPTY_BITS   _u(0x00000001)
#define PROC_PIO_IRQ1_INTE_SM0_RXNEMPTY_MSB    _u(0)
#define PROC_PIO_IRQ1_INTE_SM0_RXNEMPTY_LSB    _u(0)
#define PROC_PIO_IRQ1_INTE_SM0_RXNEMPTY_ACCESS "RW"
// =============================================================================
// Register    : PROC_PIO_IRQ1_INTF
// Description : Interrupt Force for irq1
#define PROC_PIO_IRQ1_INTF_OFFSET _u(0x00000160)
#define PROC_PIO_IRQ1_INTF_BITS   _u(0x00000fff)
#define PROC_PIO_IRQ1_INTF_RESET  _u(0x00000000)
#define PROC_PIO_IRQ1_INTF_WIDTH  _u(32)
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_IRQ1_INTF_SM3
// Description : None
#define PROC_PIO_IRQ1_INTF_SM3_RESET  _u(0x0)
#define PROC_PIO_IRQ1_INTF_SM3_BITS   _u(0x00000800)
#define PROC_PIO_IRQ1_INTF_SM3_MSB    _u(11)
#define PROC_PIO_IRQ1_INTF_SM3_LSB    _u(11)
#define PROC_PIO_IRQ1_INTF_SM3_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_IRQ1_INTF_SM2
// Description : None
#define PROC_PIO_IRQ1_INTF_SM2_RESET  _u(0x0)
#define PROC_PIO_IRQ1_INTF_SM2_BITS   _u(0x00000400)
#define PROC_PIO_IRQ1_INTF_SM2_MSB    _u(10)
#define PROC_PIO_IRQ1_INTF_SM2_LSB    _u(10)
#define PROC_PIO_IRQ1_INTF_SM2_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_IRQ1_INTF_SM1
// Description : None
#define PROC_PIO_IRQ1_INTF_SM1_RESET  _u(0x0)
#define PROC_PIO_IRQ1_INTF_SM1_BITS   _u(0x00000200)
#define PROC_PIO_IRQ1_INTF_SM1_MSB    _u(9)
#define PROC_PIO_IRQ1_INTF_SM1_LSB    _u(9)
#define PROC_PIO_IRQ1_INTF_SM1_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_IRQ1_INTF_SM0
// Description : None
#define PROC_PIO_IRQ1_INTF_SM0_RESET  _u(0x0)
#define PROC_PIO_IRQ1_INTF_SM0_BITS   _u(0x00000100)
#define PROC_PIO_IRQ1_INTF_SM0_MSB    _u(8)
#define PROC_PIO_IRQ1_INTF_SM0_LSB    _u(8)
#define PROC_PIO_IRQ1_INTF_SM0_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_IRQ1_INTF_SM3_TXNFULL
// Description : None
#define PROC_PIO_IRQ1_INTF_SM3_TXNFULL_RESET  _u(0x0)
#define PROC_PIO_IRQ1_INTF_SM3_TXNFULL_BITS   _u(0x00000080)
#define PROC_PIO_IRQ1_INTF_SM3_TXNFULL_MSB    _u(7)
#define PROC_PIO_IRQ1_INTF_SM3_TXNFULL_LSB    _u(7)
#define PROC_PIO_IRQ1_INTF_SM3_TXNFULL_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_IRQ1_INTF_SM2_TXNFULL
// Description : None
#define PROC_PIO_IRQ1_INTF_SM2_TXNFULL_RESET  _u(0x0)
#define PROC_PIO_IRQ1_INTF_SM2_TXNFULL_BITS   _u(0x00000040)
#define PROC_PIO_IRQ1_INTF_SM2_TXNFULL_MSB    _u(6)
#define PROC_PIO_IRQ1_INTF_SM2_TXNFULL_LSB    _u(6)
#define PROC_PIO_IRQ1_INTF_SM2_TXNFULL_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_IRQ1_INTF_SM1_TXNFULL
// Description : None
#define PROC_PIO_IRQ1_INTF_SM1_TXNFULL_RESET  _u(0x0)
#define PROC_PIO_IRQ1_INTF_SM1_TXNFULL_BITS   _u(0x00000020)
#define PROC_PIO_IRQ1_INTF_SM1_TXNFULL_MSB    _u(5)
#define PROC_PIO_IRQ1_INTF_SM1_TXNFULL_LSB    _u(5)
#define PROC_PIO_IRQ1_INTF_SM1_TXNFULL_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_IRQ1_INTF_SM0_TXNFULL
// Description : None
#define PROC_PIO_IRQ1_INTF_SM0_TXNFULL_RESET  _u(0x0)
#define PROC_PIO_IRQ1_INTF_SM0_TXNFULL_BITS   _u(0x00000010)
#define PROC_PIO_IRQ1_INTF_SM0_TXNFULL_MSB    _u(4)
#define PROC_PIO_IRQ1_INTF_SM0_TXNFULL_LSB    _u(4)
#define PROC_PIO_IRQ1_INTF_SM0_TXNFULL_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_IRQ1_INTF_SM3_RXNEMPTY
// Description : None
#define PROC_PIO_IRQ1_INTF_SM3_RXNEMPTY_RESET  _u(0x0)
#define PROC_PIO_IRQ1_INTF_SM3_RXNEMPTY_BITS   _u(0x00000008)
#define PROC_PIO_IRQ1_INTF_SM3_RXNEMPTY_MSB    _u(3)
#define PROC_PIO_IRQ1_INTF_SM3_RXNEMPTY_LSB    _u(3)
#define PROC_PIO_IRQ1_INTF_SM3_RXNEMPTY_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_IRQ1_INTF_SM2_RXNEMPTY
// Description : None
#define PROC_PIO_IRQ1_INTF_SM2_RXNEMPTY_RESET  _u(0x0)
#define PROC_PIO_IRQ1_INTF_SM2_RXNEMPTY_BITS   _u(0x00000004)
#define PROC_PIO_IRQ1_INTF_SM2_RXNEMPTY_MSB    _u(2)
#define PROC_PIO_IRQ1_INTF_SM2_RXNEMPTY_LSB    _u(2)
#define PROC_PIO_IRQ1_INTF_SM2_RXNEMPTY_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_IRQ1_INTF_SM1_RXNEMPTY
// Description : None
#define PROC_PIO_IRQ1_INTF_SM1_RXNEMPTY_RESET  _u(0x0)
#define PROC_PIO_IRQ1_INTF_SM1_RXNEMPTY_BITS   _u(0x00000002)
#define PROC_PIO_IRQ1_INTF_SM1_RXNEMPTY_MSB    _u(1)
#define PROC_PIO_IRQ1_INTF_SM1_RXNEMPTY_LSB    _u(1)
#define PROC_PIO_IRQ1_INTF_SM1_RXNEMPTY_ACCESS "RW"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_IRQ1_INTF_SM0_RXNEMPTY
// Description : None
#define PROC_PIO_IRQ1_INTF_SM0_RXNEMPTY_RESET  _u(0x0)
#define PROC_PIO_IRQ1_INTF_SM0_RXNEMPTY_BITS   _u(0x00000001)
#define PROC_PIO_IRQ1_INTF_SM0_RXNEMPTY_MSB    _u(0)
#define PROC_PIO_IRQ1_INTF_SM0_RXNEMPTY_LSB    _u(0)
#define PROC_PIO_IRQ1_INTF_SM0_RXNEMPTY_ACCESS "RW"
// =============================================================================
// Register    : PROC_PIO_IRQ1_INTS
// Description : Interrupt status after masking & forcing for irq1
#define PROC_PIO_IRQ1_INTS_OFFSET _u(0x00000164)
#define PROC_PIO_IRQ1_INTS_BITS   _u(0x00000fff)
#define PROC_PIO_IRQ1_INTS_RESET  _u(0x00000000)
#define PROC_PIO_IRQ1_INTS_WIDTH  _u(32)
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_IRQ1_INTS_SM3
// Description : None
#define PROC_PIO_IRQ1_INTS_SM3_RESET  _u(0x0)
#define PROC_PIO_IRQ1_INTS_SM3_BITS   _u(0x00000800)
#define PROC_PIO_IRQ1_INTS_SM3_MSB    _u(11)
#define PROC_PIO_IRQ1_INTS_SM3_LSB    _u(11)
#define PROC_PIO_IRQ1_INTS_SM3_ACCESS "RO"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_IRQ1_INTS_SM2
// Description : None
#define PROC_PIO_IRQ1_INTS_SM2_RESET  _u(0x0)
#define PROC_PIO_IRQ1_INTS_SM2_BITS   _u(0x00000400)
#define PROC_PIO_IRQ1_INTS_SM2_MSB    _u(10)
#define PROC_PIO_IRQ1_INTS_SM2_LSB    _u(10)
#define PROC_PIO_IRQ1_INTS_SM2_ACCESS "RO"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_IRQ1_INTS_SM1
// Description : None
#define PROC_PIO_IRQ1_INTS_SM1_RESET  _u(0x0)
#define PROC_PIO_IRQ1_INTS_SM1_BITS   _u(0x00000200)
#define PROC_PIO_IRQ1_INTS_SM1_MSB    _u(9)
#define PROC_PIO_IRQ1_INTS_SM1_LSB    _u(9)
#define PROC_PIO_IRQ1_INTS_SM1_ACCESS "RO"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_IRQ1_INTS_SM0
// Description : None
#define PROC_PIO_IRQ1_INTS_SM0_RESET  _u(0x0)
#define PROC_PIO_IRQ1_INTS_SM0_BITS   _u(0x00000100)
#define PROC_PIO_IRQ1_INTS_SM0_MSB    _u(8)
#define PROC_PIO_IRQ1_INTS_SM0_LSB    _u(8)
#define PROC_PIO_IRQ1_INTS_SM0_ACCESS "RO"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_IRQ1_INTS_SM3_TXNFULL
// Description : None
#define PROC_PIO_IRQ1_INTS_SM3_TXNFULL_RESET  _u(0x0)
#define PROC_PIO_IRQ1_INTS_SM3_TXNFULL_BITS   _u(0x00000080)
#define PROC_PIO_IRQ1_INTS_SM3_TXNFULL_MSB    _u(7)
#define PROC_PIO_IRQ1_INTS_SM3_TXNFULL_LSB    _u(7)
#define PROC_PIO_IRQ1_INTS_SM3_TXNFULL_ACCESS "RO"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_IRQ1_INTS_SM2_TXNFULL
// Description : None
#define PROC_PIO_IRQ1_INTS_SM2_TXNFULL_RESET  _u(0x0)
#define PROC_PIO_IRQ1_INTS_SM2_TXNFULL_BITS   _u(0x00000040)
#define PROC_PIO_IRQ1_INTS_SM2_TXNFULL_MSB    _u(6)
#define PROC_PIO_IRQ1_INTS_SM2_TXNFULL_LSB    _u(6)
#define PROC_PIO_IRQ1_INTS_SM2_TXNFULL_ACCESS "RO"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_IRQ1_INTS_SM1_TXNFULL
// Description : None
#define PROC_PIO_IRQ1_INTS_SM1_TXNFULL_RESET  _u(0x0)
#define PROC_PIO_IRQ1_INTS_SM1_TXNFULL_BITS   _u(0x00000020)
#define PROC_PIO_IRQ1_INTS_SM1_TXNFULL_MSB    _u(5)
#define PROC_PIO_IRQ1_INTS_SM1_TXNFULL_LSB    _u(5)
#define PROC_PIO_IRQ1_INTS_SM1_TXNFULL_ACCESS "RO"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_IRQ1_INTS_SM0_TXNFULL
// Description : None
#define PROC_PIO_IRQ1_INTS_SM0_TXNFULL_RESET  _u(0x0)
#define PROC_PIO_IRQ1_INTS_SM0_TXNFULL_BITS   _u(0x00000010)
#define PROC_PIO_IRQ1_INTS_SM0_TXNFULL_MSB    _u(4)
#define PROC_PIO_IRQ1_INTS_SM0_TXNFULL_LSB    _u(4)
#define PROC_PIO_IRQ1_INTS_SM0_TXNFULL_ACCESS "RO"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_IRQ1_INTS_SM3_RXNEMPTY
// Description : None
#define PROC_PIO_IRQ1_INTS_SM3_RXNEMPTY_RESET  _u(0x0)
#define PROC_PIO_IRQ1_INTS_SM3_RXNEMPTY_BITS   _u(0x00000008)
#define PROC_PIO_IRQ1_INTS_SM3_RXNEMPTY_MSB    _u(3)
#define PROC_PIO_IRQ1_INTS_SM3_RXNEMPTY_LSB    _u(3)
#define PROC_PIO_IRQ1_INTS_SM3_RXNEMPTY_ACCESS "RO"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_IRQ1_INTS_SM2_RXNEMPTY
// Description : None
#define PROC_PIO_IRQ1_INTS_SM2_RXNEMPTY_RESET  _u(0x0)
#define PROC_PIO_IRQ1_INTS_SM2_RXNEMPTY_BITS   _u(0x00000004)
#define PROC_PIO_IRQ1_INTS_SM2_RXNEMPTY_MSB    _u(2)
#define PROC_PIO_IRQ1_INTS_SM2_RXNEMPTY_LSB    _u(2)
#define PROC_PIO_IRQ1_INTS_SM2_RXNEMPTY_ACCESS "RO"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_IRQ1_INTS_SM1_RXNEMPTY
// Description : None
#define PROC_PIO_IRQ1_INTS_SM1_RXNEMPTY_RESET  _u(0x0)
#define PROC_PIO_IRQ1_INTS_SM1_RXNEMPTY_BITS   _u(0x00000002)
#define PROC_PIO_IRQ1_INTS_SM1_RXNEMPTY_MSB    _u(1)
#define PROC_PIO_IRQ1_INTS_SM1_RXNEMPTY_LSB    _u(1)
#define PROC_PIO_IRQ1_INTS_SM1_RXNEMPTY_ACCESS "RO"
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_IRQ1_INTS_SM0_RXNEMPTY
// Description : None
#define PROC_PIO_IRQ1_INTS_SM0_RXNEMPTY_RESET  _u(0x0)
#define PROC_PIO_IRQ1_INTS_SM0_RXNEMPTY_BITS   _u(0x00000001)
#define PROC_PIO_IRQ1_INTS_SM0_RXNEMPTY_MSB    _u(0)
#define PROC_PIO_IRQ1_INTS_SM0_RXNEMPTY_LSB    _u(0)
#define PROC_PIO_IRQ1_INTS_SM0_RXNEMPTY_ACCESS "RO"
// =============================================================================
// Register    : PROC_PIO_BLOCK_ID
// Description : Block Identifier
//               Hexadecimal representation of "pio2"
#define PROC_PIO_BLOCK_ID_OFFSET _u(0x00000168)
#define PROC_PIO_BLOCK_ID_BITS   _u(0xffffffff)
#define PROC_PIO_BLOCK_ID_RESET  _u(0x70696f32)
#define PROC_PIO_BLOCK_ID_WIDTH  _u(32)
#define PROC_PIO_BLOCK_ID_MSB    _u(31)
#define PROC_PIO_BLOCK_ID_LSB    _u(0)
#define PROC_PIO_BLOCK_ID_ACCESS "RO"
// =============================================================================
// Register    : PROC_PIO_INSTANCE_ID
// Description : Block Instance Identifier
#define PROC_PIO_INSTANCE_ID_OFFSET _u(0x0000016c)
#define PROC_PIO_INSTANCE_ID_BITS   _u(0x0000000f)
#define PROC_PIO_INSTANCE_ID_RESET  _u(0x00000000)
#define PROC_PIO_INSTANCE_ID_WIDTH  _u(32)
#define PROC_PIO_INSTANCE_ID_MSB    _u(3)
#define PROC_PIO_INSTANCE_ID_LSB    _u(0)
#define PROC_PIO_INSTANCE_ID_ACCESS "RO"
// =============================================================================
// Register    : PROC_PIO_RSTSEQ_AUTO
// Description : None
#define PROC_PIO_RSTSEQ_AUTO_OFFSET _u(0x00000170)
#define PROC_PIO_RSTSEQ_AUTO_BITS   _u(0x00000001)
#define PROC_PIO_RSTSEQ_AUTO_RESET  _u(0x00000001)
#define PROC_PIO_RSTSEQ_AUTO_WIDTH  _u(32)
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_RSTSEQ_AUTO_BUSADAPTER
// Description : 1 = reset is controlled by the sequencer
//               0 = reset is controlled by rstseq_ctrl
#define PROC_PIO_RSTSEQ_AUTO_BUSADAPTER_RESET  _u(0x1)
#define PROC_PIO_RSTSEQ_AUTO_BUSADAPTER_BITS   _u(0x00000001)
#define PROC_PIO_RSTSEQ_AUTO_BUSADAPTER_MSB    _u(0)
#define PROC_PIO_RSTSEQ_AUTO_BUSADAPTER_LSB    _u(0)
#define PROC_PIO_RSTSEQ_AUTO_BUSADAPTER_ACCESS "RW"
// =============================================================================
// Register    : PROC_PIO_RSTSEQ_PARALLEL
// Description : None
#define PROC_PIO_RSTSEQ_PARALLEL_OFFSET _u(0x00000174)
#define PROC_PIO_RSTSEQ_PARALLEL_BITS   _u(0x00000001)
#define PROC_PIO_RSTSEQ_PARALLEL_RESET  _u(0x00000000)
#define PROC_PIO_RSTSEQ_PARALLEL_WIDTH  _u(32)
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_RSTSEQ_PARALLEL_BUSADAPTER
// Description : Is this reset parallel (i.e. not part of the sequence)
#define PROC_PIO_RSTSEQ_PARALLEL_BUSADAPTER_RESET  _u(0x0)
#define PROC_PIO_RSTSEQ_PARALLEL_BUSADAPTER_BITS   _u(0x00000001)
#define PROC_PIO_RSTSEQ_PARALLEL_BUSADAPTER_MSB    _u(0)
#define PROC_PIO_RSTSEQ_PARALLEL_BUSADAPTER_LSB    _u(0)
#define PROC_PIO_RSTSEQ_PARALLEL_BUSADAPTER_ACCESS "RO"
// =============================================================================
// Register    : PROC_PIO_RSTSEQ_CTRL
// Description : None
#define PROC_PIO_RSTSEQ_CTRL_OFFSET _u(0x00000178)
#define PROC_PIO_RSTSEQ_CTRL_BITS   _u(0x00000001)
#define PROC_PIO_RSTSEQ_CTRL_RESET  _u(0x00000000)
#define PROC_PIO_RSTSEQ_CTRL_WIDTH  _u(32)
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_RSTSEQ_CTRL_BUSADAPTER
// Description : 1 = keep the reset asserted
//               0 = keep the reset deasserted
//               This is ignored if rstseq_auto=1
#define PROC_PIO_RSTSEQ_CTRL_BUSADAPTER_RESET  _u(0x0)
#define PROC_PIO_RSTSEQ_CTRL_BUSADAPTER_BITS   _u(0x00000001)
#define PROC_PIO_RSTSEQ_CTRL_BUSADAPTER_MSB    _u(0)
#define PROC_PIO_RSTSEQ_CTRL_BUSADAPTER_LSB    _u(0)
#define PROC_PIO_RSTSEQ_CTRL_BUSADAPTER_ACCESS "RW"
// =============================================================================
// Register    : PROC_PIO_RSTSEQ_TRIG
// Description : None
#define PROC_PIO_RSTSEQ_TRIG_OFFSET _u(0x0000017c)
#define PROC_PIO_RSTSEQ_TRIG_BITS   _u(0x00000001)
#define PROC_PIO_RSTSEQ_TRIG_RESET  _u(0x00000000)
#define PROC_PIO_RSTSEQ_TRIG_WIDTH  _u(32)
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_RSTSEQ_TRIG_BUSADAPTER
// Description : Pulses the reset output
#define PROC_PIO_RSTSEQ_TRIG_BUSADAPTER_RESET  _u(0x0)
#define PROC_PIO_RSTSEQ_TRIG_BUSADAPTER_BITS   _u(0x00000001)
#define PROC_PIO_RSTSEQ_TRIG_BUSADAPTER_MSB    _u(0)
#define PROC_PIO_RSTSEQ_TRIG_BUSADAPTER_LSB    _u(0)
#define PROC_PIO_RSTSEQ_TRIG_BUSADAPTER_ACCESS "SC"
// =============================================================================
// Register    : PROC_PIO_RSTSEQ_DONE
// Description : None
#define PROC_PIO_RSTSEQ_DONE_OFFSET _u(0x00000180)
#define PROC_PIO_RSTSEQ_DONE_BITS   _u(0x00000001)
#define PROC_PIO_RSTSEQ_DONE_RESET  _u(0x00000000)
#define PROC_PIO_RSTSEQ_DONE_WIDTH  _u(32)
// -----------------------------------------------------------------------------
// Field       : PROC_PIO_RSTSEQ_DONE_BUSADAPTER
// Description : Indicates the current state of the reset
#define PROC_PIO_RSTSEQ_DONE_BUSADAPTER_RESET  _u(0x0)
#define PROC_PIO_RSTSEQ_DONE_BUSADAPTER_BITS   _u(0x00000001)
#define PROC_PIO_RSTSEQ_DONE_BUSADAPTER_MSB    _u(0)
#define PROC_PIO_RSTSEQ_DONE_BUSADAPTER_LSB    _u(0)
#define PROC_PIO_RSTSEQ_DONE_BUSADAPTER_ACCESS "RO"
// =============================================================================
#endif // HARDWARE_REGS_PROC_PIO_DEFINED
