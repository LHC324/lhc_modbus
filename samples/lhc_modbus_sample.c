/*
 * Copyright (c) 
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief basic lhc mosbus samples.
 *        Process the received data and read it from the target register.
 */
#include <rtthread.h>
#include <rtdevice.h>
#include <board.h>
#include "lhc_modbus_port.h"

#ifdef LHC_MODBUS_USING_SAMPLES

void lhc_modbus_slave_sample(p_lhc_modbus_t pd)
{
    uint8_t coil[] = {1, 0, 1, 0, 0, 1, 0, 1};
    uint8_t in_coil[] = {0, 1, 0, 1, 1, 0, 1, 0};
    float hold_reg = 3.5f;
    uint16_t input_reg = 12;
    uint16_t i = 0;

    if (NULL == pd)
        return;
    pd->operatex(pd, Coil, lhc_modbus_write, 0, coil, sizeof(coil));
    pd->operatex(pd, InputCoil, lhc_modbus_write, 0, in_coil, sizeof(in_coil));
    pd->operatex(pd, HoldRegister, lhc_modbus_write, 0, (uint8_t *)&hold_reg, sizeof(hold_reg));
    pd->operatex(pd, InputRegister, lhc_modbus_write, 0, (uint8_t *)&input_reg, sizeof(input_reg));

    memset(in_coil, 0, sizeof(in_coil));
    memset(coil, 0, sizeof(coil));
    memset(&hold_reg, 0, sizeof(hold_reg));
    memset(&input_reg, 0, sizeof(input_reg));
    pd->operatex(pd, Coil, lhc_modbus_read, 0, coil, sizeof(coil));
    pd->operatex(pd, InputCoil, lhc_modbus_read, 0, in_coil, sizeof(in_coil));
    pd->operatex(pd, HoldRegister, lhc_modbus_read, 0, (uint8_t *)&hold_reg, sizeof(hold_reg));
    pd->operatex(pd, InputRegister, lhc_modbus_read, 0, (uint8_t *)&input_reg, sizeof(input_reg));

    rt_kprintf("coil:\r\n");
    for (i = 0; i < sizeof(coil); i++){
        rt_kprintf("%d\t", coil[i]);
    }
    rt_kprintf("\r\nin_coil:\r\n");
    for (i = 0; i < sizeof(in_coil); i++){
        rt_kprintf("%d\t", in_coil[i]);
    }
    rt_kprintf("\r\nhold_reg:%.2f\r\n", hold_reg);
    rt_kprintf("input_reg:%d\r\n", input_reg);
}
#endif /* LHC_MODBUS_SAMPLE */
