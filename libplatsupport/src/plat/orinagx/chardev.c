/*
 * Copyright 2024, Unikie
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */
#include "../../chardev.h"
#include "../../common.h"
#include <utils/util.h>

/* Orin AGX uses TCU (Combined UART) which is managed by firmware.
 * Console access is provided through the VMM, not platsupport chardev. */

struct ps_chardevice*
ps_cdev_init(enum chardev_id id, const ps_io_ops_t* o, struct ps_chardevice* d)
{
    return NULL;
}
