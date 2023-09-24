/*
 * lhc_modbus.c
 *
 *  Created on: 2022年04月08日
 *      Author: LHC324
 */
#include "lhc_modbus.h"

/*静态函数声明*/
static void lhc_modbus_poll(p_lhc_modbus_t pd);
#if defined(LHC_MODBUS_USING_MASTER)
static void lhc_modbus_46h(p_lhc_modbus_t pd, uint16_t regaddr, uint8_t *pdata, uint8_t datalen);
static void lhc_modbus_master_request(p_lhc_modbus_t pd);
#endif
static bool lhc_modbus_operatex(p_lhc_modbus_t pd,
                                lhc_modbus_reg_type reg_type,
                                lhc_modbus_reg_op operate,
                                uint16_t addr, uint8_t *pdata, uint16_t len);

/**
 * @brief  创建Modbus协议站对象(动态方式)
 * @param  pd 需要初始化对象指针
 * @param  ps 初始化数据指针
 * @retval None
 */
int create_lhc_modbus(p_lhc_modbus_t *pd, p_lhc_modbus_t ps)
{
    uint8_t *p_tx_buf = NULL;
    uint8_t *p_rx_buf = NULL;
    if (!ps)
        return -1;
#if (LHC_MODBUS_USING_MALLOC)
    (*pd) = (p_lhc_modbus_t)lhc_modbus_malloc(sizeof(lhc_modbus_t));
    if (!(*pd))
        lhc_modbus_free(*pd);
    p_tx_buf = (uint8_t *)lhc_modbus_malloc(lhc_modbus_tx_size(ps));
    if (!p_tx_buf)
    {
#if (LHC_MODBUS_USING_DEBUG)
        LHC_MODBUS_DEBUG_D("@error:pTxbuf Creation failed!\r\n");
#endif
        lhc_modbus_free(p_tx_buf);
        return -1;
    }

    p_rx_buf = (uint8_t *)lhc_modbus_malloc(lhc_modbus_rx_size(ps));
    if (!p_rx_buf)
    {
#if (LHC_MODBUS_USING_DEBUG)
        LHC_MODBUS_DEBUG_D("@error:pRxbuf Creation failed!\r\n");
#endif
        lhc_modbus_free(p_rx_buf);
        return -1;
    }
#else
    static uint8_t p_tx_buf[lhc_modbus_tx_size(ps)];
    static uint8_t p_rx_buf[lhc_modbus_rx_size(ps)];
#endif

    memset(p_tx_buf, 0x00, lhc_modbus_tx_size(ps));
    memset(p_rx_buf, 0x00, lhc_modbus_rx_size(ps));
#if (LHC_MODBUS_USING_DEBUG)
    LHC_MODBUS_DEBUG_D("@note:Modbus[%d] = 0x%p\r\n", ps->slave.id, *pd);
#endif
    (*pd)->type = ps->type > lhc_modbus_Slave ? lhc_modbus_Slave : ps->type;
    (*pd)->callback = ps->callback;
    (*pd)->error = ps->error;
    (*pd)->ota = ps->ota;
#if (LHC_MODBUS_RTOS)
    (*pd)->lock = ps->lock;
    (*pd)->ulock = ps->ulock;
#endif
#if (LHC_MODBUS_USING_DMA)
#if (TOOL_USING_STM32HAL)
    (*pd)->recive = (void (*)(void *))uartx_recive_handle;
#else
    (*pd)->recive = ps->recive;
#endif
#endif
    (*pd)->transmit = ps->transmit;
    (*pd)->poll = lhc_modbus_poll;
#if defined(LHC_MODBUS_USING_MASTER)
    (*pd)->code_46h = lhc_modbus_46h;
    (*pd)->request = lhc_modbus_master_request;
#endif
    (*pd)->operatex = lhc_modbus_operatex;
    (*pd)->crc16 = ps->crc16;

    if (!ps->Uart.tx.pbuf)
    {
        ps->Uart.tx.pbuf = p_tx_buf;
    }
    if (!ps->Uart.rx.pbuf)
    {
        ps->Uart.rx.pbuf = p_rx_buf;
    }
    memcpy(&(*pd)->Uart, &ps->Uart, sizeof(modbus_uart_t));
    memcpy(&(*pd)->master, &ps->master, sizeof(ps->master));
    memcpy(&(*pd)->slave, &ps->slave, sizeof(ps->slave));
    (*pd)->pPools = ps->pPools;

    return 0;
}

#if (LHC_MODBUS_USING_MALLOC)
/**
 * @brief  销毁modbus对象
 * @param  pd 需要初始化对象指针
 * @retval None
 */
void free_lhc_modbus(p_lhc_modbus_t *pd)
{
    if (*pd)
    {
        lhc_modbus_free((*pd)->Uart.tx.pbuf);
        lhc_modbus_free((*pd)->Uart.rx.pbuf);
        lhc_modbus_free((*pd));
    }
}
#endif

/*私有接口不对外开放*/
#if defined(LHC_MODBUS_USING_COIL) || defined(LHC_MODBUS_USING_INPUT_COIL)
static void lhc_modbus_read_x_coil(p_lhc_modbus_t pd);
static void lhc_modbus_write_coil(p_lhc_modbus_t pd);
#endif
#if defined(LHC_MODBUS_USING_INPUT_REGISTER) || defined(LHC_MODBUS_USING_HOLD_REGISTER)
static void lhc_modbus_read_x_register(p_lhc_modbus_t pd);
static void lhc_modbus_write_hold_register(p_lhc_modbus_t pd);
#endif
#if defined(LHC_MODBUS_USING_REPORT_ID)
static void lhc_modbus_report_slaveid(p_lhc_modbus_t pd);
#endif

#if 0
typedef void (*operate_x)(p_lhc_modbus_t);
/**
 * @brief     modbus根据协议栈注册类型获取操作指针
 * @param  pd modbus对象句柄
 * @param  pg 函数操作指针句柄
 * @retval None
 */
static void Modbus_Get_Operate_Pointer(p_lhc_modbus_t pd,
                                       operate_x *pg)
{
    lhc_modbus_type lhc_modbus_type = pd->type;
    operate_x operate_group[] = {
#if defined(LHC_MODBUS_USING_COIL) || defined(LHC_MODBUS_USING_INPUT_COIL)
        lhc_modbus_read_x_coil,
        lhc_modbus_write_coil,
#endif
#if defined(LHC_MODBUS_USING_INPUT_REGISTER) || defined(LHC_MODBUS_USING_HOLD_REGISTER)
        lhc_modbus_read_x_register,
        lhc_modbus_write_hold_register,
#endif
    };
    if (!pg)
    {
#if (LHC_MODBUS_USING_DEBUG)
        LHC_MODBUS_DEBUG_D("@error:Manipulate null pointer or illegal type operation.\r\n");
#endif
        return;
    }
    if (lhc_modbus_type == lhc_modbus_Master)
    {
        uint16_t pg_num = sizeof(operate_group) / sizeof(operate_x);
        if (pg_num % 2U)
        {
#if (LHC_MODBUS_USING_DEBUG)
            LHC_MODBUS_DEBUG_D("@error:Non-2-aligned access.\r\n");
#endif
            return;
        }
        for (operate_x *p = operate_group, *q;
             p < operate_group + pg_num; p += 2)
        {
            q = p + 1;
            SWAP(operate_x, *p, *q);
        }
    }
    if (lhc_modbus_type == lhc_modbus_Slave)
    {
        /*do something*/
    }
    memcpy(pg, operate_group, sizeof(operate_group));
}
#endif

/**
 * @brief  Modbus接收数据帧合法性检查
 * @param  pd modbus对象句柄
 * @retval lhc_modbus_state_code 检验结果
 */
static lhc_modbus_state_code lhc_modbus_recv_check(p_lhc_modbus_t pd)
{
    uint16_t crc16 = 0;
    /*检查协议栈类型*/
    if (pd->type > lhc_modbus_Slave)
    {
#if (LHC_MODBUS_USING_DEBUG)
        LHC_MODBUS_DEBUG_D("@error:Protocol stack type error,Must Master/Slave.\r\n");
#endif
        return lhc_mod_err_config;
    }
    /*首次调度时RXcount值被清零，导致计算crc时地址越界*/
    if ((lhc_modbus_rx_count(pd) < 2U) || (lhc_modbus_rx_count(pd) > lhc_modbus_rx_size(pd)))
    {
#if (LHC_MODBUS_USING_DEBUG)
        LHC_MODBUS_DEBUG_D("@error:Protocol frame length error,cur: %d.\r\n", lhc_modbus_rx_count(pd));
#endif
        return lhc_mod_err_len;
    }
    uint8_t lhc_modbus_id = pd->slave.id;
    if (pd->type == lhc_modbus_Master)
        lhc_modbus_id = pd->master.id;
    /*检查是否是目标从站*/
    if (get_lhc_modbus_id() != lhc_modbus_id)
    {
#if (LHC_MODBUS_USING_DEBUG)
        LHC_MODBUS_DEBUG_D("@error:Protocol stack address error,target: %d,cur: %d.\r\n",
                        lhc_modbus_id, get_lhc_modbus_id());
#endif
        return lhc_mod_err_id;
    }
    if (pd->crc16)
        crc16 = pd->crc16(lhc_modbus_rx_buf, lhc_modbus_rx_count(pd) - 2U, 0xffff);

    if (get_lhc_modbus_data(lhc_modbus_rx_buf, lhc_modbus_rx_count(pd) - 2U, LHC_MODBUS_WORD) !=
        ((uint16_t)((crc16 >> 8U) | (crc16 << 8U))))
    {
#if (LHC_MODBUS_USING_DEBUG)
        LHC_MODBUS_DEBUG_D("@error: crc check code error,target crc[%#x],cur crc[%#x],count: %d.\r\n",
                        (uint16_t)((crc16 >> 8U) | (crc16 << 8U)),
                        get_lhc_modbus_data(lhc_modbus_rx_buf, lhc_modbus_rx_count(pd) - 2U, LHC_MODBUS_WORD), lhc_modbus_rx_count(pd));
#endif
        return lhc_mod_err_crc;
    }
    return lhc_mod_ok;
}

/**
 * @brief	Determine how the wifi module works
 * @details
 * @param	pd:modbus master/slave handle
 * @retval	true：MODBUS;fasle:shell
 */
static bool lhc_check_is_ota(p_lhc_modbus_t pd)
{
// #define LHC_MODBUS_OTA_KEY "lhc_ota"
//     return (!strcmp(LHC_MODBUS_OTA_KEY, lhc_modbus_rx_buf));
// #undef LHC_MODBUS_OTA_KEY
#define ENTER_OTA_MODE_CODE 0x0D
    return (((lhc_modbus_rx_count(pd) == 1U) &&
             (lhc_modbus_rx_buf[0] == ENTER_OTA_MODE_CODE)));
#undef ENTER_OTA_MODE_CODE
}

/**
 * @brief  Modbus接收数据解析[支持主机/从机]
 * @param  pd lhc modbus对象句柄
 * @retval None
 */
static void lhc_modbus_poll(p_lhc_modbus_t pd)
{
	lhc_modbus_state_code lhc_state;
//	operate_x pFunc_Group[] = {NULL, NULL, NULL, NULL};
//    operate_x *pOpt = pFunc_Group;
#if (!LHC_MODBUS_RTOS)
    if (!pd->Uart.recive_finish_flag)
        return;
    pd->Uart.recive_finish_flag = false;
#endif
    /*检查是否进入OTA升级*/
    if (lhc_check_is_ota(pd))
    {
#if (LHC_MODBUS_USING_DEBUG)
        LHC_MODBUS_DEBUG_R("\r\n@success: exit modbus mode.");
#endif
        if (pd->ota)
            pd->ota(pd);
        goto __exit;
    }
    /*可以利用功能码和数据长度预测帧长度：可解析粘包数据*/
    lhc_state = lhc_modbus_recv_check(pd);
    if (lhc_state != lhc_mod_ok)
    {
#if (LHC_MODBUS_USING_DEBUG)
        // LHC_MODBUS_DEBUG_R("modbus_rx_buf[%d]:", lhc_modbus_rx_count(pd));
        // for (uint8_t i = 0; i < lhc_modbus_rx_count(pd); i++)
        // {
        //     LHC_MODBUS_DEBUG_R("%02X ", lhc_modbus_rx_buf[i]);
        // }
        // LHC_MODBUS_DEBUG_R("\r\n\r\n");
        LHC_MODBUS_HEX(DBG_TAG, 16, lhc_modbus_rx_buf, lhc_modbus_rx_count(pd));
#endif
        if (pd->error)
            pd->error(pd, lhc_state);
        goto __exit;
    }

#define Using_Opt(__pd, __pOpt, __id)       \
    do                                      \
    {                                       \
        if (*((__pOpt) + (__id)))           \
            (*((__pOpt) + (__id)))((__pd)); \
    } while (false)
    /*确认协议栈是工作在主机还是从机*/
    // Modbus_Get_Operate_Pointer(pd, pFunc_Group);
    switch (get_lhc_modbus_fun_code())
    {
#if defined(LHC_MODBUS_USING_COIL) || defined(LHC_MODBUS_USING_INPUT_COIL)
    case ReadCoil:
    case ReadInputCoil:
    {
        // Using_Opt(pd, pOpt, 0U);
        lhc_modbus_read_x_coil(pd);
    }
    break;
    case WriteCoil:
    case WriteCoils:
    {
        // Using_Opt(pd, pOpt, 1U);
        lhc_modbus_write_coil(pd);
    }
    break;
#endif
#if defined(LHC_MODBUS_USING_INPUT_REGISTER) || defined(LHC_MODBUS_USING_HOLD_REGISTER)
    case ReadHoldReg:
    case ReadInputReg:
    {
        // Using_Opt(pd, pOpt, 2U);
        lhc_modbus_read_x_register(pd);
    }
    break;
    case WriteHoldReg:
    case WriteHoldRegs:
    {
        // Using_Opt(pd, pOpt, 3U);
        lhc_modbus_write_hold_register(pd);
    }
    break;
#endif
    case ReportSeverId:
    {
        lhc_modbus_report_slaveid(pd);
    
    }
    break;
    default:
        break;
    }
__exit:
    if (pd->callback)
        pd->callback(pd, (lhc_modbus_func_code)get_lhc_modbus_fun_code());
    memset(lhc_modbus_rx_buf, 0x00, lhc_modbus_rx_size(pd));
    lhc_modbus_rx_count(pd) = 0U;
}

/*获取寄存器类型*/
#define Get_RegType(__obj, __type) \
    ((__type) < InputRegister ? (__obj)->pPools->Coils : (__obj)->pPools->InputRegister)

/*获取寄存器地址*/
#if defined(LHC_MODBUS_USING_COIL) && defined(LHC_MODBUS_USING_INPUT_COIL) && \
    defined(LHC_MODBUS_USING_INPUT_REGISTER) && defined(LHC_MODBUS_USING_HOLD_REGISTER)
#define Get_RegAddr(__obj, __type, __addr)                                                                 \
    ((__type) == Coil ? (uint8_t *)&(__obj)->pPools->Coils[__addr]                                         \
                      : ((__type) == InputCoil ? (uint8_t *)&(__obj)->pPools->InputCoils[__addr]           \
                                               : ((__type) == InputRegister                                \
                                                      ? (uint8_t *)&(__obj)->pPools->InputRegister[__addr] \
                                                      : (uint8_t *)&(__obj)->pPools->HoldRegister[__addr])))
#elif defined(LHC_MODBUS_USING_COIL) || defined(LHC_MODBUS_USING_INPUT_COIL)
#define Get_RegAddr(__obj, __type, __addr)                         \
    ((__type) == Coil ? (uint8_t *)&(__obj)->pPools->Coils[__addr] \
                      : (uint8_t *)&(__obj)->pPools->InputCoils[__addr])
#elif defined(LHC_MODBUS_USING_INPUT_REGISTER) || defined(LHC_MODBUS_USING_HOLD_REGISTER)
#define Get_RegAddr(__obj, __type, __addr)                                          \
    ((__type) == InputRegister ? (uint8_t *)&(__obj)->pPools->InputRegister[__addr] \
                               : (uint8_t *)&(__obj)->pPools->HoldRegister[__addr])
#else
#define Get_RegAddr(__obj, __type, __addr) (__obj, __type, __addr)
#endif

/**
 * @brief  Modbus协议读取/写入寄存器
 * @note   pd->Mod_Lock非空时（加锁）时，请不要在软件定时器或中断中调用该函数
 * @param  pd 需要初始化对象指针
 * @param  regaddr 寄存器地址[寄存器起始地址从0开始]
 * @param  pdat 数据指针
 * @param  len  读取数据长度
 * @retval None
 */
static bool lhc_modbus_operatex(p_lhc_modbus_t pd,
                                lhc_modbus_reg_type reg_type,
                                lhc_modbus_reg_op operate,
                                uint16_t addr, uint8_t *pdata, uint16_t len)
{
    if (pd == NULL)
        return true;
    if (pd->lock)
        pd->lock();
    uint32_t max = reg_type < InputRegister
                       ? LHC_MODBUS_REG_POOL_SIZE
                       : LHC_MODBUS_REG_POOL_SIZE * 2U;
    uint8_t *pDest, *pSou;
    bool ret = false;

    switch (reg_type)
    {
#if defined(LHC_MODBUS_USING_COIL)
    case Coil:
        max = sizeof(pd->pPools->Coils);
        break;
#endif
#if defined(LHC_MODBUS_USING_INPUT_COIL)
    case InputCoil:
        max = sizeof(pd->pPools->InputCoils);
        break;
#endif
#if defined(LHC_MODBUS_USING_INPUT_REGISTER)
    case InputRegister:
        max = sizeof(pd->pPools->InputRegister);
        break;
#endif
#if defined(LHC_MODBUS_USING_HOLD_REGISTER)
    case HoldRegister:
        max = sizeof(pd->pPools->HoldRegister);
        break;
#endif
    default:
        max = 0;
        break;
    }
#if defined(LHC_MODBUS_RTOS)
    // taskENTER_CRITICAL();
#endif
    if ((addr < max) && (len <= max))
    {
#if defined(LHC_MODBUS_USING_COIL) || defined(LHC_MODBUS_USING_INPUT_COIL) || \
    defined(LHC_MODBUS_USING_INPUT_REGISTER) || defined(LHC_MODBUS_USING_HOLD_REGISTER)
        if (operate == lhc_modbus_read)
        {
            pDest = pdata, pSou = Get_RegAddr(pd, reg_type, addr);
        }
        else
        {
            pDest = Get_RegAddr(pd, reg_type, addr), pSou = pdata;
        }
#endif
        if (pDest && pSou && len) // 防止拷贝空指针
        {
            if (memcpy(pDest, pSou, len))
                ret = true;
        }
        // __DSB();
        // __DMB();
    }
#if defined(LHC_MODBUS_RTOS)
    // taskEXIT_CRITICAL();
#endif
#if (LHC_MODBUS_USING_DEBUG)
//    LHC_MODBUS_DEBUG_D("pdest[%#x] = %#X, psou[%#x]= %#X, len= %d.\r\n", pDest, *pDest, pSou, *pSou, len);
#endif
    if (pd->ulock)
        pd->ulock();
    return ret;
}

#if defined(LHC_MODBUS_USING_MASTER)
/**
 * @brief  Modbus协议主站有人云拓展46指令
 * @param  pd 需要初始化对象指针
 * @param  regaddr 寄存器地址
 * @param  pdata 数据指针
 * @param  datalen 数据长度
 * @retval None
 */
static void lhc_modbus_46h(p_lhc_modbus_t pd, uint16_t regaddr, uint8_t *pdata, uint8_t datalen)
{
#define MASTER_FUNCTION_CODE 0x46
    uint8_t buf[] = {pd->master.id, MASTER_FUNCTION_CODE, regaddr >> 8U, regaddr,
                     (datalen / 2U) >> 8U, (datalen / 2U), datalen};

    memset(lhc_modbus_tx_buf, 0x00, lhc_modbus_tx_size(pd));
    lhc_modbus_tx_count(pd) = 0U;
    memcpy(lhc_modbus_tx_buf, buf, sizeof(buf));
    lhc_modbus_tx_count(pd) += sizeof(buf);
    memcpy(&lhc_modbus_tx_buf[lhc_modbus_tx_count(pd)], pdata, datalen);
    lhc_modbus_tx_count(pd) += datalen;

    pd->transmit(pd, lhc_used_crc);
}

/**
 * @brief  Modbus协议主站主动请求从机数据
 * @param  pd 需要初始化对象指针
 * @param  code 功能码
 * @param  regaddr 寄存器开始地址
 * @param  reglen 寄存器长度
 * @retval None
 */
static void lhc_modbus_master_request(p_lhc_modbus_t pd)
{
    uint8_t buf[] = {
        (uint8_t)pd->master.id,
        pd->master.request_data.code,
        pd->master.request_data.reg_start_addr >> 8U,
        pd->master.request_data.reg_start_addr,
        (pd->master.request_data.reg_len) >> 8U,
        (pd->master.request_data.reg_len),
    };
    memset(lhc_modbus_tx_buf, 0x00, lhc_modbus_tx_size(pd));
    lhc_modbus_tx_count(pd) = 0U;
    memcpy(lhc_modbus_tx_buf, buf, sizeof(buf));
    lhc_modbus_tx_count(pd) += sizeof(buf);

    pd->transmit(pd, lhc_used_crc);
}
#endif

/**
 * @brief  Modbus协议读取线圈和输入线圈状态(0x01\0x02)
 * @note   暂未加入主机适配
 * @param  pd 需要初始化对象指针
 * @retval None
 */
#if defined(LHC_MODBUS_USING_COIL) || defined(LHC_MODBUS_USING_INPUT_COIL)
static void lhc_modbus_read_x_coil(p_lhc_modbus_t pd)
{
#define Byte_To_Bits 8U
    uint8_t len = get_lhc_modbus_data(lhc_modbus_rx_buf, 4U, LHC_MODBUS_WORD);
    // uint8_t bytes = len % Byte_To_Bits > 0 ? len / Byte_To_Bits + 1U : len / Byte_To_Bits;
    uint8_t bytes = len / Byte_To_Bits + !!(len % Byte_To_Bits);
    // uint8_t bytes = (len + Byte_To_Bits - 1U) / Byte_To_Bits;
    uint8_t *prbits = (uint8_t *)lhc_modbus_malloc(len);
    lhc_modbus_reg_type reg_type = NullRegister;
    if (!prbits)
        goto __exit;

    memset(prbits, 0x00, len);
    memset(lhc_modbus_tx_buf, 0x00, lhc_modbus_tx_size(pd));
    lhc_modbus_tx_count(pd) = 0U;
    memcpy(lhc_modbus_tx_buf, lhc_modbus_rx_buf, 2U);
    lhc_modbus_tx_count(pd) += 2U;
    lhc_modbus_tx_buf[lhc_modbus_tx_count(pd)++] = bytes;
    /*通过功能码寻址寄存器*/
    reg_type = get_lhc_modbus_fun_code() == ReadCoil ? Coil
                                                : InputCoil;
    pd->operatex(pd, reg_type, lhc_modbus_read, get_lhc_modbus_data(lhc_modbus_rx_buf, 2U, LHC_MODBUS_WORD),
                     prbits, len);
#if (LHC_MODBUS_USING_DEBUG)
    // for (uint8_t i = 0; i < len; i++)
    // LHC_MODBUS_DEBUG_D("prbits[%d] = 0x%X, len= %d.\r\n", i, prbits[i], len);
#endif
    for (uint8_t i = 0; i < bytes; i++)
    {
        for (uint8_t j = 0; j < Byte_To_Bits && (i * Byte_To_Bits + j) < len; j++)
        {
            uint8_t bit = (prbits[i * Byte_To_Bits + j] & 0x01);
            if (bit)
                lhc_modbus_tx_buf[lhc_modbus_tx_count(pd)] |= (bit << j);
            else
                lhc_modbus_tx_buf[lhc_modbus_tx_count(pd)] &= ~(bit << j);
        }
#if (LHC_MODBUS_USING_DEBUG)
        // LHC_MODBUS_DEBUG_D("pTbuf[%d] = 0x%X.\r\n", i, lhc_modbus_tx_buf[lhc_modbus_tx_count(pd)]);
#endif
        lhc_modbus_tx_count(pd)++;
    }
#if (LHC_MODBUS_USING_DEBUG)
    // LHC_MODBUS_DEBUG_D("lhc_modbus_tx_count(pd) = %d.\r\n", lhc_modbus_tx_count(pd));
#endif
    pd->transmit(pd, lhc_used_crc);
__exit:
    lhc_modbus_free(prbits);
}

/**
 * @brief  Modbus协议写线圈/线圈组(0x05\0x0F)
 * @param  pd 需要初始化对象指针
 * @retval None
 */
static void lhc_modbus_write_coil(p_lhc_modbus_t pd)
{
    uint16_t len = 0x00, addr;
    uint8_t *pdata = NULL;
    uint8_t wbit = 0;
    enum lhc_modbus_crc crc;

    addr = get_lhc_modbus_data(lhc_modbus_rx_buf, 2U, LHC_MODBUS_WORD);   
    /*写单个线圈*/
    if (get_lhc_modbus_fun_code() == WriteCoil)
    {
        pd->operatex(pd, Coil, lhc_modbus_read, addr, &wbit, len);
        if ((get_lhc_modbus_data(lhc_modbus_rx_buf, 4U, LHC_MODBUS_WORD) == 0xFF00)) 
        {
            wbit |= (1 << addr);
        }
        else
            wbit &= ~(1 << addr);
        len = 1U;
        pdata = &wbit;
        lhc_modbus_tx_count(pd) = lhc_modbus_rx_count(pd);
        crc = lhc_not_used_crc;
    }
    /*写多个线圈*/
    else
    {
        len = get_lhc_modbus_data(lhc_modbus_rx_buf, 4U, LHC_MODBUS_WORD);
        /*利用发送缓冲区空间暂存数据*/
        pdata = lhc_modbus_tx_buf;

        for (uint8_t i = 0; i < len; i++)
        {
            pdata[i] = (lhc_modbus_rx_buf[7U + i / Byte_To_Bits] >> (i % Byte_To_Bits)) & 0x01;
        }
        lhc_modbus_tx_count(pd) = 6U;
        crc = lhc_used_crc;
    }
#if (LHC_MODBUS_USING_DEBUG)
    LHC_MODBUS_DEBUG_D("pdata = %#X, len= %d.\r\n", *pdata, len);
#endif
    if (pdata)
        pd->operatex(pd, Coil, lhc_modbus_write, addr, pdata, len);
                        
    memset(lhc_modbus_tx_buf, 0x00, lhc_modbus_tx_size(pd));
    /*请求数据原路返回*/
    memcpy(lhc_modbus_tx_buf, lhc_modbus_rx_buf, lhc_modbus_tx_count(pd));
    /*从机模式才返回数据*/
    if (pd->type == lhc_modbus_Slave)
        pd->transmit(pd, crc);
}
#endif

/**
 * @brief  Modbus协议读输入寄存器/保持寄存器(0x03\0x04)
 * @param  pd 需要初始化对象指针
 * @retval None
 */
#if defined(LHC_MODBUS_USING_INPUT_REGISTER) || defined(LHC_MODBUS_USING_HOLD_REGISTER)
static void lhc_modbus_read_x_register(p_lhc_modbus_t pd)
{
    uint8_t len = get_lhc_modbus_data(lhc_modbus_rx_buf, 4U, LHC_MODBUS_WORD) * sizeof(uint16_t);

#if defined(SMODBUS_USING_MASTER)
    /*从机模式才返回数据*/
    if (pd->type == lhc_modbus_Master)
    {
        uint16_t start_addr = pd->master.request_data.reg_start_addr;

        len = lhc_modbus_rx_buf[2U];
        pd->operatex(pd, InputRegister, lhc_modbus_write, start_addr, &lhc_modbus_rx_buf[3U], len);
        return;
    }
#endif

    uint8_t *prdata = (uint8_t *)lhc_modbus_malloc(len);

    lhc_modbus_reg_type reg_type = NullRegister;
    if (!prdata)
        goto __exit;
    memset(prdata, 0x00, len);
    memset(lhc_modbus_tx_buf, 0x00, lhc_modbus_tx_size(pd));
    lhc_modbus_tx_count(pd) = 0U;
    memcpy(lhc_modbus_tx_buf, lhc_modbus_rx_buf, 2U);
    lhc_modbus_tx_count(pd) += 2U;
    lhc_modbus_tx_buf[lhc_modbus_tx_count(pd)] = len;
    lhc_modbus_tx_count(pd) += sizeof(len);
    /*通过功能码寻址寄存器*/
    reg_type = get_lhc_modbus_fun_code() == ReadHoldReg ? HoldRegister
                                                   : InputRegister;
    pd->operatex(pd, reg_type, lhc_modbus_read, get_lhc_modbus_data(lhc_modbus_rx_buf, 2U, LHC_MODBUS_WORD),
                     prdata, len);
    memcpy(&lhc_modbus_tx_buf[lhc_modbus_tx_count(pd)], prdata, len);
    lhc_modbus_tx_count(pd) += len;

    pd->transmit(pd, lhc_used_crc);
__exit:
    lhc_modbus_free(prdata);
}

/**
 * @brief  Modbus协议写保持寄存器/多个保持寄存器(0x06/0x10)
 * @note   支持主机和从机
 * @param  pd 需要初始化对象指针
 * @retval None
 */
static void lhc_modbus_write_hold_register(p_lhc_modbus_t pd)
{
    uint8_t *pdata = NULL, len = 0x00;
    uint16_t start_addr = get_lhc_modbus_data(lhc_modbus_rx_buf, 0x02, LHC_MODBUS_WORD);
    enum lhc_modbus_crc crc = lhc_used_crc;

    switch (get_lhc_modbus_fun_code())
    { /*写单个保持寄存器*/
    case WriteHoldReg:
    {
        len = sizeof(uint16_t);
        /*改变数据指针*/
        pdata = &lhc_modbus_rx_buf[4U];
        lhc_modbus_tx_count(pd) = lhc_modbus_rx_count(pd);
        crc = lhc_not_used_crc;
    }
    break;
    case WriteHoldRegs:
    {
        len = lhc_modbus_rx_buf[6U];
        /*改变数据指针*/
        pdata = &lhc_modbus_rx_buf[7U];
        lhc_modbus_tx_count(pd) = 6U;
    }
    break;
    case ReadHoldReg: /*主机模式*/
    {
        len = lhc_modbus_rx_buf[2U];
        start_addr = pd->master.request_data.reg_start_addr;
        /*改变数据指针*/
        pdata = &lhc_modbus_rx_buf[3U];
    }
    break;
    default:
    {
        if (pd->error)
            pd->error(pd, lhc_mod_err_cmd);
    }
    break;
    }
    if (pdata)
        pd->operatex(pd, HoldRegister, lhc_modbus_write, start_addr, pdata, len);
    /*从机模式才返回数据*/
    if (pd->type == lhc_modbus_Slave)
    {
        memset(lhc_modbus_tx_buf, 0x00, lhc_modbus_tx_size(pd));
        /*请求数据原路返回*/
        memcpy(lhc_modbus_tx_buf, lhc_modbus_rx_buf, lhc_modbus_tx_count(pd));
        pd->transmit(pd, crc);
    }
}
#endif

#if defined(LHC_MODBUS_USING_REPORT_ID)
/**
 * @brief  Modbus协议上报一些特定信息
 * @param  pd 需要初始化对象指针
 * @retval None
 */
static void lhc_modbus_report_slaveid(p_lhc_modbus_t pd)
{
    memset(pd->Uart.tx.pbuf, 0x00, pd->Uart.tx.size);
    pd->Uart.tx.count = 0U;
    memcpy(pd->Uart.tx.pbuf, pd->Uart.rx.pbuf, 2U);
    pd->Uart.tx.count += 2U;
    pd->Uart.tx.pbuf[pd->Uart.tx.count++] = sizeof(uint8_t);
    /*读取卡槽与板卡编码*/
    pd->Uart.tx.pbuf[pd->Uart.tx.count++] = pd->code;
    pd->transmit(pd, lhc_used_crc);
}
#endif
