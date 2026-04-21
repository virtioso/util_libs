/*
 * Copyright 2024, Unikie
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */
#pragma once

/* Orin AGX uses TCU (combined UART) - not used directly by platsupport */

enum chardev_id {
    NUM_CHARDEV,
    PS_SERIAL_DEFAULT = 0
};

#define DEFAULT_SERIAL_PADDR 0
#define DEFAULT_SERIAL_INTERRUPT 0
