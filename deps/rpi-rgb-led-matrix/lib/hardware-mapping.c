/* -*- mode: c; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 * Copyright (C) 2013, 2016 Henner Zeller <h.zeller@acm.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation version 2.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http: *gnu.org/licenses/gpl-2.0.txt>
 */

/*
 * We do this in plain C so that we can use designated initializers.
 */
#include "hardware-mapping.h"

#define GPIO_BIT(b) ((uint64_t)1<<(b))

struct HardwareMapping matrix_hardware_mappings[] = {
  /*
   * The regular hardware mapping described in the wiring.md and used
   * by the adapter PCBs.
   */
  {
    .name          = "regular",

    .output_enable = GPIO_BIT(18),
    .clock         = GPIO_BIT(17),
    .strobe        = GPIO_BIT(4),

    /* Address lines */
    .a             = GPIO_BIT(22),
    .b             = GPIO_BIT(23),
    .c             = GPIO_BIT(24),
    .d             = GPIO_BIT(25),
    .e             = GPIO_BIT(15),  /* RxD kept free unless 1:64 */

    /* Parallel chain 0, RGB for both sub-panels */
    .p0_r1         = GPIO_BIT(11),  /* masks: SPI0_SCKL  */
    .p0_g1         = GPIO_BIT(27),  /* Not on RPi1, Rev1; use "regular-pi1" instead */
    .p0_b1         = GPIO_BIT(7),   /* masks: SPI0_CE1   */
    .p0_r2         = GPIO_BIT(8),   /* masks: SPI0_CE0   */
    .p0_g2         = GPIO_BIT(9),   /* masks: SPI0_MISO  */
    .p0_b2         = GPIO_BIT(10),  /* masks: SPI0_MOSI  */

    /* All the following are only available with 40 GPIP pins, on A+/B+/Pi2,3 */
    /* Chain 1 */
    .p1_r1         = GPIO_BIT(12),
    .p1_g1         = GPIO_BIT(5),
    .p1_b1         = GPIO_BIT(6),
    .p1_r2         = GPIO_BIT(19),
    .p1_g2         = GPIO_BIT(13),
    .p1_b2         = GPIO_BIT(20),

    /* Chain 2 */
    .p2_r1         = GPIO_BIT(14), /* masks TxD when parallel=3 */
    .p2_g1         = GPIO_BIT(2),  /* masks SCL when parallel=3 */
    .p2_b1         = GPIO_BIT(3),  /* masks SDA when parallel=3 */
    .p2_r2         = GPIO_BIT(26),
    .p2_g2         = GPIO_BIT(16),
    .p2_b2         = GPIO_BIT(21),
  },

  /*
   * This is used if you have an Adafruit HAT in the default configuration
   */
  {
    .name          = "adafruit-hat",

    .output_enable = GPIO_BIT(4),
    .clock         = GPIO_BIT(17),
    .strobe        = GPIO_BIT(21),

    .a             = GPIO_BIT(22),
    .b             = GPIO_BIT(26),
    .c             = GPIO_BIT(27),
    .d             = GPIO_BIT(20),
    .e             = GPIO_BIT(24),  /* Needs manual wiring, see README.md */

    .p0_r1         = GPIO_BIT(5),
    .p0_g1         = GPIO_BIT(13),
    .p0_b1         = GPIO_BIT(6),
    .p0_r2         = GPIO_BIT(12),
    .p0_g2         = GPIO_BIT(16),
    .p0_b2         = GPIO_BIT(23),
  },

  /*
   * An Adafruit HAT with the PWM modification
   */
  {
    .name          = "adafruit-hat-pwm",

    .output_enable = GPIO_BIT(18),  /* The only change compared to above */
    .clock         = GPIO_BIT(17),
    .strobe        = GPIO_BIT(21),

    .a             = GPIO_BIT(22),
    .b             = GPIO_BIT(26),
    .c             = GPIO_BIT(27),
    .d             = GPIO_BIT(20),
    .e             = GPIO_BIT(24),

    .p0_r1         = GPIO_BIT(5),
    .p0_g1         = GPIO_BIT(13),
    .p0_b1         = GPIO_BIT(6),
    .p0_r2         = GPIO_BIT(12),
    .p0_g2         = GPIO_BIT(16),
    .p0_b2         = GPIO_BIT(23),
  },

  /*
   * The regular pin-out, but for Raspberry Pi1. The very first Pi1 Rev1 uses
   * the same pin for GPIO-21 as later Pis use GPIO-27. Make it work for both.
   */
  {
    .name          = "regular-pi1",

    .output_enable = GPIO_BIT(18),
    .clock         = GPIO_BIT(17),
    .strobe        = GPIO_BIT(4),

    /* Address lines */
    .a             = GPIO_BIT(22),
    .b             = GPIO_BIT(23),
    .c             = GPIO_BIT(24),
    .d             = GPIO_BIT(25),
    .e             = GPIO_BIT(15),  /* RxD kept free unless 1:64 */

    /* Parallel chain 0, RGB for both sub-panels */
    .p0_r1         = GPIO_BIT(11),  /* masks: SPI0_SCKL  */
    /* On Pi1 Rev1, the pin other Pis have GPIO27, these have GPIO21. So make
     * this work for both Rev1 and Rev2.
     */
    .p0_g1         = GPIO_BIT(21) | GPIO_BIT(27),
    .p0_b1         = GPIO_BIT(7),   /* masks: SPI0_CE1   */
    .p0_r2         = GPIO_BIT(8),   /* masks: SPI0_CE0   */
    .p0_g2         = GPIO_BIT(9),   /* masks: SPI0_MISO  */
    .p0_b2         = GPIO_BIT(10),  /* masks: SPI0_MOSI  */

    /* No more chains - there are not enough GPIO */
  },

  /*
   * Classic: Early forms of this library had this as default mapping, mostly
   * derived from the 26 GPIO-header version so that it also can work
   * on 40 Pin GPIO headers with more parallel chains.
   * Not used anymore.
   */
  {
    .name          = "classic",

    .output_enable = GPIO_BIT(27),  /* Not available on RPi1, Rev 1 */
    .clock         = GPIO_BIT(11),
    .strobe        = GPIO_BIT(4),

    .a             = GPIO_BIT(7),
    .b             = GPIO_BIT(8),
    .c             = GPIO_BIT(9),
    .d             = GPIO_BIT(10),

    .p0_r1         = GPIO_BIT(17),
    .p0_g1         = GPIO_BIT(18),
    .p0_b1         = GPIO_BIT(22),
    .p0_r2         = GPIO_BIT(23),
    .p0_g2         = GPIO_BIT(24),
    .p0_b2         = GPIO_BIT(25),

    .p1_r1         = GPIO_BIT(12),
    .p1_g1         = GPIO_BIT(5),
    .p1_b1         = GPIO_BIT(6),
    .p1_r2         = GPIO_BIT(19),
    .p1_g2         = GPIO_BIT(13),
    .p1_b2         = GPIO_BIT(20),

    .p2_r1         = GPIO_BIT(14),   /* masks TxD if parallel = 3 */
    .p2_g1         = GPIO_BIT(2),    /* masks SDA if parallel = 3 */
    .p2_b1         = GPIO_BIT(3),    /* masks SCL if parallel = 3 */
    .p2_r2         = GPIO_BIT(15),
    .p2_g2         = GPIO_BIT(26),
    .p2_b2         = GPIO_BIT(21),
  },

  /*
   * Classic pin-out for Rev-A Raspberry Pi.
   */
  {
    .name          = "classic-pi1",

    /* The Revision-1 and Revision-2 boards have different GPIO mappings
     * on the P1-3 and P1-5. So we use both interpretations.
     * To keep the I2C pins free, we avoid these in later mappings.
     */
    .output_enable = GPIO_BIT(0) | GPIO_BIT(2),
    .clock         = GPIO_BIT(1) | GPIO_BIT(3),
    .strobe        = GPIO_BIT(4),

    .a             = GPIO_BIT(7),
    .b             = GPIO_BIT(8),
    .c             = GPIO_BIT(9),
    .d             = GPIO_BIT(10),

    .p0_r1         = GPIO_BIT(17),
    .p0_g1         = GPIO_BIT(18),
    .p0_b1         = GPIO_BIT(22),
    .p0_r2         = GPIO_BIT(23),
    .p0_g2         = GPIO_BIT(24),
    .p0_b2         = GPIO_BIT(25),
  },

#ifdef ENABLE_WIDE_GPIO_COMPUTE_MODULE
  /*
   * Custom pin-out for compute-module
   */
  {
    .name          = "compute-module",

    /* This GPIO mapping is made for the official I/O development
     * board. No pin is left free when using 6 parallel chains.
     */
    .output_enable = GPIO_BIT(18),
    .clock         = GPIO_BIT(16),
    .strobe        = GPIO_BIT(17),

    .a             = GPIO_BIT(2),
    .b             = GPIO_BIT(3),
    .c             = GPIO_BIT(4),
    .d             = GPIO_BIT(5),
    .e             = GPIO_BIT(6),  /* RxD kept free unless 1:64 */

    /* Chain 0 */
    .p0_r1         = GPIO_BIT(7),
    .p0_g1         = GPIO_BIT(8),
    .p0_b1         = GPIO_BIT(9),
    .p0_r2         = GPIO_BIT(10),
    .p0_g2         = GPIO_BIT(11),
    .p0_b2         = GPIO_BIT(12),

    /* Chain 1 */
    .p1_r1         = GPIO_BIT(13),
    .p1_g1         = GPIO_BIT(14),
    .p1_b1         = GPIO_BIT(15),
    .p1_r2         = GPIO_BIT(19),
    .p1_g2         = GPIO_BIT(20),
    .p1_b2         = GPIO_BIT(21),

    /* Chain 2 */
    .p2_r1         = GPIO_BIT(22),
    .p2_g1         = GPIO_BIT(23),
    .p2_b1         = GPIO_BIT(24),
    .p2_r2         = GPIO_BIT(25),
    .p2_g2         = GPIO_BIT(26),
    .p2_b2         = GPIO_BIT(27),

    /* Chain 3 */
    .p3_r1         = GPIO_BIT(28),
    .p3_g1         = GPIO_BIT(29),
    .p3_b1         = GPIO_BIT(30),
    .p3_r2         = GPIO_BIT(31),
    .p3_g2         = GPIO_BIT(32),
    .p3_b2         = GPIO_BIT(33),

    /* Chain 4 */
    .p4_r1         = GPIO_BIT(34),
    .p4_g1         = GPIO_BIT(35),
    .p4_b1         = GPIO_BIT(36),
    .p4_r2         = GPIO_BIT(37),
    .p4_g2         = GPIO_BIT(38),
    .p4_b2         = GPIO_BIT(39),

    /* Chain 5 */
    .p5_r1         = GPIO_BIT(40),
    .p5_g1         = GPIO_BIT(41),
    .p5_b1         = GPIO_BIT(42),
    .p5_r2         = GPIO_BIT(43),
    .p5_g2         = GPIO_BIT(44),
    .p5_b2         = GPIO_BIT(45),
  },
#endif

  {0}
};
