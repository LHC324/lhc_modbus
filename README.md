# Lightweight and high-performance modbus library based on C language



EN| [中文](README_zh.md)

## Introduction



[lhc modbus](https://github.com/armink/FlashDB) （*Lightweight and high-performance modbus library based on C language*）It is an ultra lightweight, high-performance, and low resource utilization modbus protocol stack that focuses on providing data communication and exchange functions between devices in industrial environments. Although they are all modbus protocols, the difference is that they are lighter and easier to use than other protocol stacks such as freemodbus and samll modbus, and have great scalability. The key is that it can be easily ported to bare metal, freertos, and other platforms.



## Why develop this library

There are many protocol stacks related to modbus, but in reality, most of them are difficult to apply in the project development process due to the following reasons:

- The client provided a short project schedule, which requires a lot of time to learn a comprehensive protocol stack, and the interface is complex, making it difficult for beginners to quickly apply it.
- The boss has a tight budget for developing products, and although there are many resource rich MCUs, many times the boss is unwilling to spend even a penny more.
- In conjunction with most cloud protocols and application scenarios on the market, the modbus protocol is used for cloud and serial screen synchronization.
- This library is mainly used to meet the above scenarios. In the actual development process of my project, I am well aware of the variability of customer requirements for the product. It is difficult to have a set of software packages that can truly adapt to all application scenarios without adjusting them. I think that is impossible, at least I have not encountered it before.
- Of course, this set of protocols is also the same, and in practical use, it needs to be considered according to the company's application scenarios.
- Separate it and integrate it into the open source ecosystem, making it convenient for users, companies, or others who need it to configure and use it in one click.



## Main characteristics

- Extremely low resource usage.
- Excellent performance, with almost **0** copies of data processing.
- Both the host and slave machines can operate all based on a single operation interface 'operatiex'.
- After cutting, it can directly run on small resource microcontrollers such as 51 microcontroller and stm32f030.
- Feature: The modbus port supports shell console transfer。
- Special support for the 0x46 command with cloud of people.



## directory structure

|  name   |              illustrate               |
| :-----: | :-----------------------------------: |
|   doc   |               document                |
| samples |                example                |
| figures |            source material            |
|   inc   |              Header file              |
|   src   |              source code              |
|  port   | Platform migration related interfaces |



## *How to use*

Below are examples of using **rt thread ** and **freertos** respectively

### **rt-thread**

Focus on ` lhc_ Modbus_ Port. c`

- Define modbus objects

```c
/*定义Modbus对象*/
p_lhc_modbus_t lhc_modbus_test_object;
```



- Define a modbus register pool

```c
/*定义一个共用寄存器池*/
static lhc_modbus_pools_t Spool;
```



- Implement CRC verification interface (**mandatory**)

```c
/**
 * @brief  modbus协议栈获取crc16校验码
 * @note   可以自己实现，也可以直接使用该接口
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
            crc16 = (crc16 & 0x0001) ? ((crc16 >>  1) ^ 0xa001) : (crc16 >> 1U);
        }
    }
    return crc16;
}
```



- Implement internal data processing callback function interface (**non mandatory **)

```c
/**
 * @brief  modbus协议栈外部回调函数
 * @param  pd 需要初始化对象指针
 * @param  code 功能码
 * @retval None
 */
static void lhc_modbus_callback(p_lhc_modbus_t pd, lhc_modbus_func_code code)
{
}
```



- When used in multithreading or interrupts, lock protection is required (**not mandatory**)

```c
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
```



- Implement modbus error handling function (**non mandatory**)

```c
/**
 * @brief  modbus协议栈接收帧错误处理
 * @param  pd 需要初始化对象指针
 * @param  error_code 错误码
 * @retval None
 */
static void lhc_modbus_error_handle(p_lhc_modbus_t pd, lhc_modbus_state_code error_code)
{
    /*
    * @note 主cpu切换一次finsh后，modbus接收的size会丢失，怀疑是rtt底层驱动有问题
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
```



- Implement the modbus underlying sending function (**mandatory **)

```c
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
     if (pd->Uart.tx.wmode & uart_using_rs485)
        HAL_GPIO_WritePin(RS485_DIR_GPIO_Port, RS485_DIR_Pin, GPIO_PIN_SET);
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
    if (pd->Uart.tx.wmode & uart_using_rs485)
         HAL_GPIO_WritePin(RS485_DIR_GPIO_Port, RS485_DIR_Pin, GPIO_PIN_RESET);
}
```

**Note: The above interfaces can all be ready-made.**

If you use **freertos**  , the above **apis ** only need to replace the relevant parts of the operating system.



Here is a demonstration, specifically using:

- Improve according to one's own needs **lhc_modbus_init**

```c
#define __init_modbus(__name, __type, __master_id, __slave_id, __get_crc16,       \
                      __callback, __lock, __unlock, __ota_update, __error_handle, \
                      __transmit, __uart, __pools, __user_handle)                 \
    lhc_modbus_t __name##_lhc_modbus = {                                          \ //临时变量名
        .type = __type,                                                           \ //对象类型（主/从）
        .master.id = __master_id,                                                 \ //主机id
        .slave.id = __slave_id,                                                   \ //从机id
        .crc16 = __get_crc16,                                                     \ //自定义crc函数
        .callback = __callback,                                                   \ //数据处理时回调函数
        .lock = __lock,                                                           \ //加锁
        .ulock = __unlock,                                                        \ //解锁
        .ota = __ota_update,                                                      \ //自定义ota或者其他功能
        .error = __error_handle,                                                  \ //错误处理函数
        .transmit = __transmit,                                                   \ //数据发送函数
        .Uart = __uart,                                                           \ //串口参数
        .pPools = &__pools,                                                       \ //此对象关联的寄存器池
        .slave.p_handle = __user_handle,                                          \ //从机使用的外部句柄
    };

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
    console_sem = rt_sem_create("console_sem", 0, RT_IPC_FLAG_FIFO);
    modbus_uart_t lhc_modbus_uart = { //挂接对应串口句柄
        .huart = &huart2,
        .phdma = &hdma_usart2_rx, //如果使用了dma，则挂接dma句柄
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

    rt_thread_pools_init();
    return 0;
}
/*在内核对象中初始化:https://blog.csdn.net/yang1111111112/article/details/93982354*/
INIT_DEVICE_EXPORT(rt_lhc_modbus_init);
```



- If OS is supported, create a data receiving and processing thread

```c
/*
* @note 此线程只是简单示例，不一定适用一些复杂情况，也不涉及具体的粘包、分包处理，如遇特殊情况需要处理，
*       请自行实现，比较灵活。
*/

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
        rt_sem_release((rt_sem_t)pd->Uart.semaphore); //假设此通知为收到完整数据帧
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
```



#### Effect demonstration

<img src="D:\STM32_WorkSpace\lhc_modbus\rt-thread\packages\lhc_modbus\figures\lhc_modbus操作四类寄存器并读取结果.png" style="zoom:80%;" />

<img src="D:\STM32_WorkSpace\lhc_modbus\rt-thread\packages\lhc_modbus\figures\lhc_modbus基本寄存器读写.gif" style="zoom:80%;" />



### Console transfer (native support for rt threads)

#### **Purpose**

- Used to directly transfer the shell console to any serial port, and then support local and remote direct use of the shell native ecosystem.
- Use the `ymodem` protocol to upgrade the built-in firmware of the MCU through different serial ports.
- Customize shell instructions, and then control the MCU locally and remotely through different serial ports to achieve some special functions.

#### **Configuration**

- Overloading the built-in `ota` method of l`hc modbus`

```c
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

```



- Add finsh console custom instructions

```c
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
```



#### Effect demonstration

<img src="D:\STM32_WorkSpace\lhc_modbus\rt-thread\packages\lhc_modbus\figures\lhc_modbus执行控制台转移.gif" style="zoom:80%;" />



### others

- The host routine has not been added yet
- Modbus TCP is currently not supported
- The current console transfer function supports custom instructions. When the overload custom recognition function is temporarily not enabled
- For more interesting and practical features, let's boldly unleash your imagination and explore them




## Contact information&thank you

- maintenance：LHC324

- homepage：https://github.com/LHC324/lhc_modbus

- mailbox：[1606820017@qq.com](mailto:1606820017@qq.com)



## License

The project uses the Apache-2.0 open source protocol. For details, please read the contents of the LICENSE file in the project.