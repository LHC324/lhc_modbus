/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2022-09-15     LHC324       the first version
 *  @verbatim
 *  使用： 1、用户需要完善"rt_lhc_modbus_init/MX_ModbusInit"、"lhc_modbus_send"函数
 *         2、用户需要定义"lhc_modbus_pools_t"寄存器池
 *         3、"rt_lhc_modbus_init/MX_ModbusInit"初始化时需要明确指定"modbus_uart_t"参数
 *         4、"lhc_modbus_callback"函数用户按需编写
 *         5、按需配置"lhc_modbus_cfg.h"
 */

#if 0
#include <rtdevice.h>
#include <board.h>
#include "lhc_modbus_port.h"

#define __init_modbus(__name, __type, __master_id, __slave_id, __get_crc16,       \
                      __callback, __lock, __unlock, __ota_update, __error_handle, \
                      __transmit, __uart, __pools, __user_handle)                 \
    lhc_modbus_t __name##_lhc_modbus = {                                          \
        .type = __type,                                                           \
        .master.id = __master_id,                                                 \
        .slave.id = __slave_id,                                                   \
        .crc16 = __get_crc16,                                                     \
        .callback = __callback,                                                   \
        .lock = __lock,                                                           \
        .ulock = __unlock,                                                        \
        .ota = __ota_update,                                                      \
        .error = __error_handle,                                                  \
        .transmit = __transmit,                                                   \
        .Uart = __uart,                                                           \
        .pPools = &__pools,                                                       \
        .slave.p_handle = __user_handle,                                          \
    };

/*定义Modbus对象*/
p_lhc_modbus_t lhc_modbus_test_object;
p_lhc_modbus_t modbus_console_handle = RT_NULL;
static lhc_modbus_pools_t Spool;
/* 指向互斥量的指针 */
static rt_mutex_t modbus_mutex = RT_NULL;
rt_sem_t  g_console_sem = RT_NULL;

static unsigned short lhc_modbus_get_crc16(unsigned char *ptr, unsigned short len, unsigned short init_val);
static void lhc_modbus_callback(p_lhc_modbus_t pd, lhc_modbus_func_code code);
static void lhc_console_transfer(p_lhc_modbus_t pd);
static void lhc_modbus_error_handle(p_lhc_modbus_t pd, lhc_modbus_state_code error_code);
#if (LHC_MODBUS_RTOS)
static void lhc_modbus_lock(void);
static void lhc_modbus_ulock(void);
#endif
static void lhc_modbus_send(p_lhc_modbus_t pd, enum lhc_modbus_crc crc);

#if (LHC_MODBUS_RTOS == 2)
#if (LHC_MODBUS_USING_MALLOC)
extern DMA_HandleTypeDef hdma_usart2_rx;
extern UART_HandleTypeDef huart2;

typedef enum
{
    using_semaphore = 0x00,
    unusing_semaphore,
} semaphore_state;

/*rt_thread 线程池*/
typedef struct rt_thread_pools
{
    rt_thread_t thread_handle;
    const char *name;
    void *parameter;
    rt_uint32_t stack_size;
    rt_uint8_t priority;
    rt_uint32_t tick;
    void (*thread)(void *parameter);
    semaphore_state sema_state;
    rt_sem_t semaphore;
} rt_thread_pools_t;

typedef struct
{
    rt_thread_pools_t *pools;
    rt_uint32_t rt_thread_numbers;
} rt_thread_pool_map_t;

/*线程入口函数声明区*/
static void modbus_slave_thread_entry(void *parameter);


#define __init_rt_thread_pools(__name, __handle, __param, __statck_size,    \
                               __prio, __tick, __fun, __sema_state, __sema) \
    {                                                                       \
        .name = __name,                                                     \
        .thread_handle = __handle,                                          \
        .parameter = __param,                                               \
        .stack_size = __statck_size,                                        \
        .priority = __prio,                                                 \
        .tick = __tick,                                                     \
        .thread = __fun,                                                    \
        .sema_state = __sema_state,                                         \
        .semaphore = __sema,                                                \
    }
/*finsh控制台卡死问题：https://www.cnblogs.com/jzcn/p/16450021.html*/
static rt_thread_pools_t thread_pools[] = {
    __init_rt_thread_pools("md_slave", RT_NULL, RT_NULL, 1024U, 0x05,
                           10, modbus_slave_thread_entry, using_semaphore, RT_NULL),

};

/**
 * @brief	rt thread 线程池初始化
 * @details 不支持自动初始化，此时调度器还没启动
 * @param	none
 * @retval  none
 */
static int rt_thread_pools_init(void)
{
    rt_thread_pool_map_t rt_thread_pool_map = {
        .pools = thread_pools,
        .rt_thread_numbers = sizeof(thread_pools) / sizeof(rt_thread_pools_t),
    };

    rt_thread_pool_map_t *p_rt_thread_map = &rt_thread_pool_map;

    rt_thread_pools_t *p_rt_thread_pools = p_rt_thread_map->pools;
    if (p_rt_thread_map->rt_thread_numbers && p_rt_thread_pools)
    {
        /*从线程池中初始化线程*/
        for (rt_thread_pools_t *p = p_rt_thread_pools;
             p < p_rt_thread_pools + p_rt_thread_map->rt_thread_numbers; ++p)
        {
            if (p->thread)
            {
                /*线程自生参数信息传递给线程入口函数*/
                // p->parameter = p;
                p->thread_handle = rt_thread_create(p->name, p->thread, p,
                                                    p->stack_size, p->priority, p->tick);
            }
            /* 创建一个动态信号量，初始值是 0 */
            if (p->sema_state == using_semaphore)
            {
                char *psemaphore_name = rt_malloc(RT_NAME_MAX);
                if (psemaphore_name)
                {
                    rt_strncpy(psemaphore_name, p->name, RT_NAME_MAX); // 最大拷贝RT_NAME_MAX
                    p->semaphore = rt_sem_create(strncat(psemaphore_name, "_sem", 5), 0, RT_IPC_FLAG_PRIO);
                }
                rt_free(psemaphore_name);
            }

            if (p->thread_handle)
                rt_thread_startup(p->thread_handle);
        }
    }
       return 0;
}
/*在内核对象中初始化:https://blog.csdn.net/yang1111111112/article/details/93982354*/
// INIT_ENV_EXPORT(rt_thread_pools_init);

/**
 * @brief	modbus 从机接收回调函数
 * @details
 * @param	dev 设备句柄
 * @param   size 当前尺寸
 * @retval  None
 */
static rt_err_t modbus_rtu_rx_ind(rt_device_t dev, rt_size_t size)
{
    p_lhc_modbus_t pd = lhc_modbus_test_object;

    if (pd && pd->Uart.semaphore)
    {
        pd->Uart.rx.count = size;
        rt_sem_release((rt_sem_t)pd->Uart.semaphore);
    }

    return RT_EOK;
}

/**
 * @brief	modbus 从机接收线程
 * @details rt thread V1和V2串口驱动说明：https://club.rt-thread.org/ask/article/8e1d18464219fae7.html
 * @note    V版本使用DMA+idle数据被拆包解决办法：https://club.rt-thread.org/ask/article/8f4b74aa754fcc93.html
 *          粘包、封包解析器：https://github.com/aeo123/upacker
 * @param	parameter:线程初始参数
 * @retval  None
 */
void modbus_slave_thread_entry(void *parameter)
{
    rt_thread_pools_t *p_rt_thread_pool = (rt_thread_pools_t *)parameter;

    p_lhc_modbus_t pd = lhc_modbus_test_object;
    rt_device_t dev = RT_NULL;

    RT_ASSERT(pd);
#ifdef LHC_MODBUS_USING_SAMPLES
    extern void lhc_modbus_slave_sample(p_lhc_modbus_t pd);
    lhc_modbus_slave_sample(pd);
#endif

    /*确认目标串口设备存在*/
    dev = rt_device_find(LHC_MODBUS_DEVICE_NAME);
    if (dev)
    {
        // 记录当前设备指针if (RT_NULL == pd->dev)
        pd->dev = dev;
        pd->Uart.semaphore = p_rt_thread_pool->semaphore;
        rt_device_open(dev, RT_DEVICE_FLAG_TX_BLOCKING | RT_DEVICE_FLAG_RX_NON_BLOCKING);
        /*挂接目标接收中断函数*/
        rt_device_set_rx_indicate(dev, modbus_rtu_rx_ind);                         
    }
    else
        rt_kprintf("@error: Target device [%s] not found.^_^\r\n", LHC_MODBUS_DEVICE_NAME);

    rt_sem_release(g_console_sem); //释放控制台
    for (;;)
    {
        /* 永久方式等待信号量，获取到信号量则解析modbus协议*/
        if ((rt_sem_take(p_rt_thread_pool->semaphore, RT_WAITING_FOREVER) == RT_EOK)
            && p_rt_thread_pool->semaphore)
        {
            rt_device_read(dev, 0, pd->Uart.rx.pbuf, pd->Uart.rx.count);
            if (pd)
            {
                lhc_modbus_handler(pd);
            }
        }
    }
}

/**
 * @brief  初始化Modbus协议库
 * @retval None
 */
int rt_lhc_modbus_init(void)
{
    int ret = 0;
    /* 创建一个动态互斥量 */
    modbus_mutex = rt_mutex_create("modbus_mutex", RT_IPC_FLAG_PRIO);
    /* 创建控制信号量 */
    g_console_sem = rt_sem_create("console_sem", 0, RT_IPC_FLAG_FIFO);
    modbus_uart_t lhc_modbus_uart = {
        .huart = &huart2,
        .phdma = &hdma_usart2_rx,
#if (LHC_MODBUS_RTOS)
        .semaphore = NULL,
#else
        .recive_finish_flag = false,
#endif
        .tx = {
            .wmode = uart_using_it | uart_using_rs485,
            .size = LHC_MODBUS_TX_BUF_SIZE,
            .count = 0,
            .pbuf = NULL,
        },
        .rx = {
            .wmode = uart_using_dma,
            .size = LHC_MODBUS_RX_BUF_SIZE,
            .count = 0,
            .pbuf = NULL,
        },
    };
    __init_modbus(temp, lhc_modbus_Slave, LHC_MODBUS_MASTER_ADDR, LHC_MODBUS_SLAVE_ADDR,
                  lhc_modbus_get_crc16, lhc_modbus_callback, lhc_modbus_lock, lhc_modbus_ulock,
                  lhc_console_transfer, lhc_modbus_error_handle,
                  lhc_modbus_send, lhc_modbus_uart, Spool, NULL);
#if (LHC_MODBUS_RTOS == 2)
    temp_lhc_modbus.dev = NULL;
    temp_lhc_modbus.old_console = NULL;
#endif
    ret = create_lhc_modbus(&lhc_modbus_test_object, &temp_lhc_modbus);
    if (ret != RT_EOK)
        return ret;

#if 0
/*-------------------------------------------------------------------*/
    modbus_uart_t lte_lhc_modbus_uart = {
        .huart = &huart2,
        .phdma = &hdma_usart2_rx,
#if (LHC_MODBUS_RTOS)
        .semaphore = NULL,
#else
        .recive_finish_flag = false,
#endif
        .tx = {
            .wmode = uart_using_it,
            .size = LHC_MODBUS_TX_BUF_SIZE,
            .count = 0,
            .pbuf = NULL,
        },
        .rx = {
            .wmode = uart_using_dma,
            .size = LHC_MODBUS_RX_BUF_SIZE,
            .count = 0,
            .pbuf = NULL,
        },
    };
    __init_modbus(lte, lhc_modbus_Slave, LHC_MODBUS_MASTER_ADDR, 0x02,
                  lhc_modbus_callback, lhc_modbus_lock, lhc_modbus_ulock,
                  lhc_console_transfer, lhc_modbus_error_handle,
                  lhc_modbus_send, lte_lhc_modbus_uart, Spool, NULL);
#if (LHC_MODBUS_RTOS == 2U)
    lte_lhc_modbus.dev = NULL;
    lte_lhc_modbus.old_console = NULL;
#endif
    create_lhc_modbus(&Lte_Modbus_Object, &lte_lhc_modbus);

/*-------------------------------------------------------------------*/
    modbus_uart_t wifi_lhc_modbus_uart = {
        .huart = &huart5,
        .phdma = &hdma_uart5_rx,
#if (LHC_MODBUS_RTOS)
        .semaphore = NULL,
#else
        .recive_finish_flag = false,
#endif
        .tx = {
            .wmode = uart_using_it,
            .size = LHC_MODBUS_TX_BUF_SIZE,
            .count = 0,
            .pbuf = NULL,
        },
        .rx = {
            .wmode = uart_using_dma,
            .size = LHC_MODBUS_RX_BUF_SIZE,
            .count = 0,
            .pbuf = NULL,
        },
    };
    __init_modbus(wifi, lhc_modbus_Slave, LHC_MODBUS_MASTER_ADDR, 0x03,
                  lhc_modbus_callback, lhc_modbus_lock, lhc_modbus_ulock,
                  lhc_console_transfer, lhc_modbus_error_handle,
                  lhc_modbus_send, wifi_lhc_modbus_uart, Spool, NULL);
#if (LHC_MODBUS_RTOS == 2U)
    wifi_lhc_modbus.dev = NULL;
    wifi_lhc_modbus.old_console = NULL;
#endif
    create_lhc_modbus(&Wifi_Modbus_Object, &wifi_lhc_modbus);
#endif

    rt_thread_pools_init();
    return 0;
}
/*在内核对象中初始化:https://blog.csdn.net/yang1111111112/article/details/93982354*/
INIT_DEVICE_EXPORT(rt_lhc_modbus_init);

/*主机使用example*/
/*void small_modbus_master_request(void)
{
    p_lhc_modbus_t pd = Modbus_Object;
    Request_HandleTypeDef request = {
        .code = ReadHoldReg,
        .reg_start_addr = 0x0000,
        .reg_len = 0x01,
    };

    if (pd)
    {
        memcpy(&pd->master.request_data, &request, sizeof(request));
        pd->request(pd);
    }
}*/
#else
int rt_lhc_modbus_init(void)
{
    MdbusHandle temp_small_modbus = {
        /*User init info*/
    };
    create_lhc_modbus(&Modbus_Object, &temp_small_modbus);
    return 0;
}
#endif

#else
void MX_ModbusInit(void)
{
}

#endif

/**
 * @brief  modbus协议栈获取crc16校验码
 * @param  ptr 数据缓冲区指针
 * @param  len 数据缓冲区长度
 * @param  init_val 校验码初值
 * @retval None
 */
static unsigned short lhc_modbus_get_crc16(unsigned char *ptr, unsigned short len, unsigned short init_val)
{
    uint16_t i;
    uint16_t crc16 = init_val;

    if (ptr == NULL)
        return 0;
    for (i = 0; i < len; i++)
    {
        crc16 ^= *ptr++;
        for (uint16_t j = 0; j < 8; j++)
        {
            crc16 = (crc16 & 0x0001) ? ((crc16 >> 1) ^ 0xa001) : (crc16 >> 1U);
        }
    }
    return crc16;
}

/**
 * @brief  modbus协议栈外部回调函数
 * @param  pd 需要初始化对象指针
 * @param  code 功能码
 * @retval None
 */
static void lhc_modbus_callback(p_lhc_modbus_t pd, lhc_modbus_func_code code)
{
}

#if (LHC_MODBUS_RTOS)
/**
 * @brief  modbus协议栈加锁函数
 * @param  pd 需要初始化对象指针
 * @param  code 功能码
 * @retval None
 */
static void lhc_modbus_lock(void)
{
    rt_mutex_take(modbus_mutex, RT_WAITING_FOREVER);
}

/**
 * @brief  modbus协议栈解锁函数
 * @param  pd 需要初始化对象指针
 * @param  code 功能码
 * @retval None
 */
static void lhc_modbus_ulock(void)
{
    rt_mutex_release(modbus_mutex);
}
#endif

/**
 * @brief  modbus协议栈进行控制台转移
 * @param  pd modbus协议站句柄
 * @retval None
 */
static void lhc_console_transfer(p_lhc_modbus_t pd)
{
    extern void finsh_set_device(const char *device_name);
    rt_err_t ret = RT_EOK;
    if (NULL == pd || NULL == pd->Uart.huart ||
        NULL == pd->dev)
        return;

    pd->old_rx_indicate = pd->dev->rx_indicate; //备份接收完成通知(串口关闭前)
    ret = rt_device_close(pd->dev);
    if ((RT_EOK == ret) && (RT_EOK == rt_sem_trytake(g_console_sem)))
    {
        modbus_console_handle = pd; //记录当前正在使用控制台的modbus句柄
        // rt_device_set_rx_indicate(pd->dev, RT_NULL); // 置空当前串口回调函数
        pd->old_console = rt_console_set_device(pd->dev->parent.name);
        finsh_set_device(pd->dev->parent.name);
        rt_kprintf("@note: enter finsh mode.\r\n");
    }
    else //转移失败后必须重启当前串口和挂接接收中断，否则该串口将无法使用
    {
        rt_device_open(pd->dev, RT_DEVICE_FLAG_TX_BLOCKING | RT_DEVICE_FLAG_RX_NON_BLOCKING);
        /*挂接目标接收中断函数*/
        rt_device_set_rx_indicate(pd->dev, (rt_err_t(*)(rt_device_t, rt_size_t))pd->old_rx_indicate);  
        rt_kprintf("@error: finsh already locked.\r\n");
    }
    // 没有挂起的必要：中断函数转移后将会永久挂起
}

/**
 * @brief  modbus协议栈接收帧错误处理
 * @param  pd 需要初始化对象指针
 * @param  error_code 错误码
 * @retval None
 */
static void lhc_modbus_error_handle(p_lhc_modbus_t pd, lhc_modbus_state_code error_code)
{
    /*@note 主cpu切换一次finsh后，modbus接收的size会丢失，怀疑是rtt底层驱动有问题
    */
    if (pd && pd->dev) //解决数据帧错位后，连续错位问题
        rt_device_read(pd->dev, 0, pd->Uart.rx.pbuf, pd->Uart.rx.size);

    pd->old_rx_indicate = pd->dev->rx_indicate; //备份接收完成通知(串口关闭前)
    if(RT_EOK == rt_device_close(pd->dev))
    {
         /*挂接目标接收中断函数*/
        rt_device_set_rx_indicate(pd->dev, (rt_err_t(*)(rt_device_t, rt_size_t))pd->old_rx_indicate);
        rt_device_open(pd->dev, RT_DEVICE_FLAG_TX_BLOCKING | RT_DEVICE_FLAG_RX_NON_BLOCKING);
        LHC_MODBUS_DEBUG_R("device[%s] restarted successfully.", pd->dev->parent.name);
    }
    
}

/**
 * @brief  Modbus协议发送
 * @param  pd 需要初始化对象指针
 * @retval None
 */
static void lhc_modbus_send(p_lhc_modbus_t pd, enum lhc_modbus_crc crc)
{
    uint16_t crc16 = 0;
#if (LHC_MODBUS_RTOS == 2U)
    if (NULL == pd || NULL == pd->dev)
#else
    if (NULL == pd || NULL == pd->Uart.huart)
#endif
        return;

    if (crc == lhc_used_crc)
    {
        if (pd->crc16)
            crc16 = pd->crc16(lhc_modbus_tx_buf, lhc_modbus_tx_count(pd), 0xffff);
        memcpy(&lhc_modbus_tx_buf[lhc_modbus_tx_count(pd)], (uint8_t *)&crc16, sizeof(crc16));
        lhc_modbus_tx_count(pd) += sizeof(crc16);
    }
    // if (pd->Uart.tx.wmode & uart_using_rs485)
        // HAL_GPIO_WritePin(RS485_DIR_GPIO_Port, RS485_DIR_Pin, GPIO_PIN_SET);
#if (LHC_MODBUS_RTOS == 2U)
    rt_device_write(pd->dev, 0, lhc_modbus_tx_buf, lhc_modbus_tx_count(pd));
#else
    switch (pd->Uart.tx.wmode)
    {
    case uart_using_it:
    {
        HAL_UART_Transmit((UART_HandleTypeDef *)pd->Uart.huart, lhc_modbus_tx_buf, lhc_modbus_tx_count(pd), 0x100);
    }
    break;
    case uart_using_dma:
    {
        HAL_UART_Transmit_DMA((UART_HandleTypeDef *)pd->Uart.huart, lhc_modbus_tx_buf, lhc_modbus_tx_count(pd));
        while (__HAL_UART_GET_FLAG((UART_HandleTypeDef *)pd->Uart.huart, UART_FLAG_TC) == RESET)
        {
        }
    }
    break;
    default:
        break;
    }
#endif
    // if (pd->Uart.tx.wmode & uart_using_rs485)
    //     HAL_GPIO_WritePin(RS485_DIR_GPIO_Port, RS485_DIR_Pin, GPIO_PIN_RESET);
}

#ifdef FINSH_USING_MSH
#include <finsh.h>

/**
 * @brief  finsh进行控制台还原
 * @param  pd modbus协议站句柄
 * @retval None
 */
void re_console(void)
{
    rt_err_t ret = RT_EOK;
    p_lhc_modbus_t pd = modbus_console_handle;
    rt_device_t console = pd->old_console;

    if ((RT_NULL == pd) || (RT_NULL == pd->dev) || (RT_NULL == console))
        return;
    ret = rt_device_close(pd->dev);
    if (RT_EOK == ret)
    {
        finsh_set_device(console->parent.name);
        rt_console_set_device(console->parent.name);
        ret = rt_device_open(pd->dev, RT_DEVICE_FLAG_TX_BLOCKING | RT_DEVICE_FLAG_RX_NON_BLOCKING);
        if (RT_EOK == ret)
        {
            rt_device_set_rx_indicate(pd->dev,
                                      (rt_err_t(*)(rt_device_t, rt_size_t))pd->old_rx_indicate); // 重新设置空当前串口回调函数
            rt_kprintf("\r\n@note: enter modbus_rtu mode.\r\n");
        }

        if (g_console_sem->value < 1) //做二值信号量使用
            rt_sem_release(g_console_sem); //释放控制台
    }
}
MSH_CMD_EXPORT(re_console, finsh console restore.);
#endif

#endif
