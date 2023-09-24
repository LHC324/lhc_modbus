/*
 * lhc_modbus.h
 *
 *  Created on: 2022年04月08日
 *      Author: LHC324
 */

#ifndef INC_LHC_MODBUS_H_
#define INC_LHC_MODBUS_H_
#include "lhc_modbus_cfg.h"
#include <stdbool.h>
#include <string.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C"
{
#endif

#ifndef uint8_t
#define uint8_t unsigned char
#endif

#ifndef uint16_t
#define uint16_t unsigned short
#endif

#ifndef uint32_t
#define uint32_t unsigned int
#endif

    typedef enum
    {
        uart_using_it = 0x01,
        uart_using_dma = 0x02,
        uart_using_rs485 = 0x04,
        uart_none_mode = 0xFF,
    } uart_wmode;

    typedef struct
    {
        unsigned int wmode;
        unsigned char *pbuf;
        unsigned int size, count;
    } lhc_modbus_uart_data_t __attribute__((aligned(4)));

    typedef struct lhc_modbus_uart *p_modbus_uart_t;
    typedef struct lhc_modbus_uart modbus_uart_t;
    struct lhc_modbus_uart
    {
        void *huart, *phdma;
        lhc_modbus_uart_data_t rx, tx;
#if (!LHC_MODBUS_RTOS)
        bool recive_finish_flag;
#else
    void *semaphore;
#endif
    } __attribute__((aligned(4)));

	/*错误码状态*/
	typedef enum
	{
		lhc_mod_ok = 0,		/*正常*/
		lhc_mod_err_config, /*配置错误*/
		lhc_mod_err_len,	/*数据帧长错误*/
		lhc_mod_err_id,		/*地址号错误*/
		lhc_mod_err_crc,	/*校验码错误*/
		lhc_mod_err_cmd,	/*不支持的功能码*/
		lhc_mod_err_addr,	/*寄存器地址错误*/
		lhc_mod_err_value,	/*数据值域错误*/
		lhc_mod_err_write,	/*写入失败*/
	} lhc_modbus_state_code;

	typedef enum
	{
		lhc_modbus_Master = 0x00,
		lhc_modbus_Slave,
	} lhc_modbus_type;

	typedef enum
	{
		Coil = 0x00,
		InputCoil,
		InputRegister,
		HoldRegister,
		NullRegister,
	} lhc_modbus_reg_type __attribute__((aligned(4)));

	typedef enum
	{
		ReadCoil = 0x01,
		ReadInputCoil = 0x02,
		ReadHoldReg = 0x03,
		ReadInputReg = 0x04,
		WriteCoil = 0x05,
		WriteHoldReg = 0x06,
		WriteCoils = 0x0F,
		WriteHoldRegs = 0x10,
		ReportSeverId = 0x11,

	} lhc_modbus_func_code __attribute__((aligned(4)));

	typedef enum
	{
		lhc_modbus_read = 0x00,
		lhc_modbus_write,
	} lhc_modbus_reg_op __attribute__((aligned(4)));

	enum lhc_modbus_crc
	{
		lhc_used_crc,
		lhc_not_used_crc
	};

	typedef struct lhc_modbus *p_lhc_modbus_t;
	typedef struct lhc_modbus lhc_modbus_t;
#if defined(LHC_MODBUS_USING_MASTER)
	typedef struct
	{
		lhc_modbus_func_code code;
		uint16_t reg_len;
		uint16_t reg_start_addr;
	} Request_HandleTypeDef;
#endif

	typedef struct
	{
#if defined(LHC_MODBUS_USING_COIL)
		uint8_t Coils[LHC_MODBUS_REG_POOL_SIZE];
#endif
#if defined(LHC_MODBUS_USING_INPUT_COIL)
		uint8_t InputCoils[LHC_MODBUS_REG_POOL_SIZE];
#endif
#if defined(LHC_MODBUS_USING_INPUT_REGISTER)
		uint16_t InputRegister[LHC_MODBUS_REG_POOL_SIZE * 2U];
#endif
#if defined(LHC_MODBUS_USING_HOLD_REGISTER)
		uint16_t HoldRegister[LHC_MODBUS_REG_POOL_SIZE * 2U];
#endif
	} lhc_modbus_pools_t __attribute__((aligned(4)));

	typedef struct
	{
		lhc_modbus_reg_type type;
		void *registers;
	} Pool __attribute__((aligned(4)));

	struct lhc_modbus
	{
#if (LHC_MODBUS_RTOS == 2U)
		rt_device_t dev, old_console;
		void *old_rx_indicate;
#endif
		uint8_t code;
		lhc_modbus_type type;
		void (*callback)(p_lhc_modbus_t, lhc_modbus_func_code);
		void (*recive)(void *);
		void (*poll)(p_lhc_modbus_t);
		void (*transmit)(p_lhc_modbus_t, enum lhc_modbus_crc);
#if defined(LHC_MODBUS_USING_MASTER)
		void (*code_46h)(p_lhc_modbus_t, uint16_t, uint8_t *, uint8_t);
		void (*request)(p_lhc_modbus_t);
#endif
		bool (*operatex)(p_lhc_modbus_t, lhc_modbus_reg_type, lhc_modbus_reg_op, uint16_t, uint8_t *, uint16_t);
		unsigned short (*crc16)(unsigned char *, unsigned short, unsigned short);
#if (LHC_MODBUS_RTOS)
		void (*lock)(void);
		void (*ulock)(void);
#endif
		// void (*Mod_ReportSeverId)(p_lhc_modbus_t);
		void (*error)(p_lhc_modbus_t, lhc_modbus_state_code);
		void (*ota)(p_lhc_modbus_t);
#if defined(LHC_MODBUS_USING_MASTER)
		struct
		{
			/*主机id：可变*/
			uint8_t id;
			Request_HandleTypeDef request_data;
			void *p_handle;
		} master;
#endif
		struct
		{
			/*从机id*/
			uint8_t id;
			/*预留外部数据结构接口*/
			void *p_handle;
		} slave;
		lhc_modbus_pools_t *pPools;
		modbus_uart_t Uart;
	} __attribute__((aligned(4)));

#define LHC_MODBUS_POOL_SIZE sizeof(lhc_modbus_pools_t)

/*带上(pd)解决多级指针解引用问题：(*pd)、(**pd)*/
#define lhc_modbus_tx_size(pd) ((pd)->Uart.tx.size)
#define lhc_modbus_rx_size(pd) ((pd)->Uart.rx.size)
#define lhc_modbus_tx_count(pd) ((pd)->Uart.tx.count)
#define lhc_modbus_rx_count(pd) ((pd)->Uart.rx.count)
#define lhc_modbus_tx_buf ((pd)->Uart.tx.pbuf)
#define lhc_modbus_rx_buf ((pd)->Uart.rx.pbuf)

#define LHC_MODBUS_WORD 1U
#define LHC_MODBUS_DWORD 2U
/*获取主机号*/
#define get_lhc_modbus_id() (lhc_modbus_rx_buf[0U])
/*获取Modbus功能号*/
#define get_lhc_modbus_fun_code() (lhc_modbus_rx_buf[1U])
/*获取Modbus协议数据*/
#define get_lhc_modbus_data(__pbuf, __s, __size)                             \
	((__size) < 2U ? ((__pbuf[__s] << 8U) |                                  \
					  (__pbuf[__s + 1U]))                                    \
				   : ((__pbuf[__s] << 24U) |                                 \
					  (__pbuf[__s + 1U] << 16U) | (__pbuf[__s + 2U] << 8U) | \
					  (__pbuf[__s + 3U])))

#if (LHC_MODBUS_USING_MALLOC == 0)
#define Get_SmallModbus_Object(id) (static lhc_modbus_t Connect_Str(Modbus_Object, id))
#define get_lhc_modbus_object(id) (&Connect_Str(Modbus_Object, id))
#else
#define lhc_modbus_handler(obj) (obj->poll(obj))
extern int create_lhc_modbus(p_lhc_modbus_t *pd, p_lhc_modbus_t ps);
extern void free_lhc_modbus(p_lhc_modbus_t *pd);
#endif

#ifdef __cplusplus
}
#endif
#endif /* INC_LHC_MODBUS_H_ */
