/*
 * Copyright (c) 
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2022-09-15     LHC324       the first version
 */
#ifndef _INC_LHC_MODBUS_PORT_H_
#define _INC_LHC_MODBUS_PORT_H_
#include "lhc_modbus.h"
#ifdef __cplusplus
extern "C"
{
#endif

    extern p_lhc_modbus_t lhc_modbus_test_object;

#if (LHC_MODBUS_RTOS == 2)
    extern int rt_lhc_modbus_init(void);
#else
extern void MX_ModbusInit(void);
#endif

#ifdef __cplusplus
}
#endif
#endif /* _INC_LHC_MODBUS_PORT_H_ */
