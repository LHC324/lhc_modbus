/*
 * Copyright (c) 
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2022-09-15     LHC324       the first version
 */
#ifndef _INC_LHC_MODBUS_CFG_H_
#define _INC_LHC_MODBUS_CFG_H_
#ifdef __cplusplus
extern "C"
{
#endif
/*****************************************************功能配置区*******************************************/
/*Configuration Wizard：https://blog.csdn.net/qq_15647227/article/details/89297207*/
// <<< Use Configuration Wizard in Context Menu >>>

// <o>Selsect lhc modbus thread Running Environment
//  <i>Default: 2
//  <0=> Bare pager
//  <1=> Freertos
//  <2=> rt_thread
/*lhc_modbus使用RTOS[0:不使用RTOS;1:Freertos;2:rt_thread]*/
#define LHC_MODBUS_RTOS 2

#if (LHC_MODBUS_RTOS == 2)
// <s>Valid when using rtthread
//  <i>Device name
#define LHC_MODBUS_DEVICE_NAME "uart2"
#endif

// <o>Set lhc modbus Dynamic Memory allocation mode
//  <i>Default: 1
//  <0=> Unused
//  <1=> Used
/*lhc_modbus动态内存分配[0:不使用；1：使用]*/
#define LHC_MODBUS_USING_MALLOC 1

// <o>Set lhc modbus Dma peripheral
//  <i>Default: 1
//  <0=> Unused
//  <1=> Used
/*lhc_modbusDMA选项*/
#define LHC_MODBUS_USING_DMA 1

// <o>Set lhc modbus crc algorithm
//  <i>Default: 1
//  <0=> Unused
//  <1=> Used
/*lhc_modbusCRC校验*/
#define LHC_MODBUS_USING_CRC 1

// <o>Selsect lhc modbus Debug Options
//  <i>Default: 2
//  <0=> Unused debug
//  <1=> leeter shell
//  <2=> finish shell
/*lhc_modbus调试选项[0:不使用调试终端；1：leeter shell; 2:finish shell]*/
#define LHC_MODBUS_USING_DEBUG 2

/*lhc_modbus调试输出终端选择:[0:不使用调试终端；1：leeter shell; 2:finish shell]*/
// #define SMODBUS_USING_SHELL 2

// <o>Set the buffer size of lhc modbus receiving thread
//  <i>Default: 128 (Unit: byte)
//  <0-1024>
/*lhc_modbus数据缓冲区尺寸*/
#define LHC_MODBUS_RX_BUF_SIZE 512

// <o>Set the buffer size of lhc modbus sending thread
//  <i>Default: 128 (Unit: byte)
//  <0-1024>
#define LHC_MODBUS_TX_BUF_SIZE 256

#if (LHC_MODBUS_USING_MALLOC)
#define lhc_modbus_malloc rt_malloc
#define lhc_modbus_free rt_free
#endif

// <o>Set lhc modbus master address
//  <i>Default: 1
//  <0-255>
/*协议栈参数配置*/
#define LHC_MODBUS_MASTER_ADDR 0x01

    // <o>Set lhc modbus slave address
    //  <i>Default: 2
    //  <0-255>

#define LHC_MODBUS_SLAVE_ADDR 0x02
// <o>Set the number of registers in the lhc modbus register pool
//  <i>Default: 128
//  <0-1024>
/*寄存器池尺寸*/
#define LHC_MODBUS_REG_POOL_SIZE 1024U

#define COIL_OFFSET (1)
#define INPUT_COIL_OFFSET (10001)
#define INPUT_REGISTER_OFFSET (30001)
#define HOLD_REGISTER_OFFSET (40001)

// <c1>Enable lhc modbus master mode
//  <i>Enable HAL Driver Component
/*启用主机模式*/
#define LHC_MODBUS_USING_MASTER
// </c>

// <c1>Enable lhc modbus report id
//  <i>Enable HAL Driver Component
/*启用主机模式*/
#define LHC_MODBUS_USING_REPORT_ID
// </c>

// <c1>Enable coil function in register pool
//  <i>Enable HAL Driver Component
/*启用线圈*/
#define LHC_MODBUS_USING_COIL
// </c>

// <c1>Enable input coil function in register pool
//  <i>Enable HAL Driver Component
/*启用输入线圈*/
#define LHC_MODBUS_USING_INPUT_COIL
// </c>

// <c1>Enable input register function in register pool
//  <i>Enable HAL Driver Component
/*启用输入寄存器*/
#define LHC_MODBUS_USING_INPUT_REGISTER
// </c>

// <c1>Enable hold register function in register pool
//  <i>Enable HAL Driver Component
/*启用保持寄存器*/
#define LHC_MODBUS_USING_HOLD_REGISTER
// </c>

// <<< end of configuration section >>>

/*包含对应操作系统接口:tool.h中包含对应api*/
#if (LHC_MODBUS_RTOS == 1)

#elif (LHC_MODBUS_RTOS == 2)

#else

#endif

/*检查关联寄存器组是否同时启用*/
#if defined(LHC_MODBUS_USING_COIL) && !defined(LHC_MODBUS_USING_INPUT_COIL)
#error Input coil not defined!
#elif !defined(LHC_MODBUS_USING_COIL) && defined(LHC_MODBUS_USING_INPUT_COIL)
#error Coil not defined!
#elif defined(LHC_MODBUS_USING_INPUT_REGISTER) && !defined(LHC_MODBUS_USING_HOLD_REGISTER)
#error Holding register not defined!
#elif !defined(LHC_MODBUS_USING_INPUT_REGISTER) && defined(LHC_MODBUS_USING_HOLD_REGISTER)
#error The input register is not defined!
#endif

#if (LHC_MODBUS_USING_DEBUG == 1)
#include "cmsis_os.h"
/*lhc modbus调试接口*/
#define LHC_MODBUS_DEBUG (fmt, ...) shellPrint(Shell_Object, fmt, ##__VA_ARGS__)
#elif (LHC_MODBUS_USING_DEBUG == 2)
#include <rtthread.h>
#undef DBG_TAG
#define DBG_TAG "lhc_modbus"
#define DBG_LVL DBG_LOG
/*必须位于DBG_SECTION_NAME之后*/
#include <rtdbg.h>
#define LHC_MODBUS_DEBUG_R LOG_RAW
#define LHC_MODBUS_DEBUG_D LOG_D
#define LHC_MODBUS_DEBUG_I LOG_I
#define LHC_MODBUS_DEBUG_W LOG_W
#define LHC_MODBUS_DEBUG_E LOG_E
#define LHC_MODBUS_HEX     LOG_HEX
#else
#define LHC_MODBUS_DEBUG
#endif

#ifdef __cplusplus
}
#endif
#endif /* _INC_LHC_MODBUS_CFG_H_ */
