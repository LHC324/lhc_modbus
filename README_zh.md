## Lightweight and high-performance modbus library based on C language（轻量级高性能基于C语言的modbus库）



[EN](README.md) | 中文

## 简介



[lhc modbus](https://github.com/armink/FlashDB) （*Lightweight and high-performance modbus library based on C language*）是一款超轻量级、高性能和低资源占用率的modbus协议栈，专注于提供工业环境中设备间数据通信和数据交换功能。虽然都是modbus协议，但是不同的是，比freemodbus、samll modbus等其他协议栈更加轻便和易用，且具有很大的可裁剪性。关键是可以很容易移植到裸机、freertos和其他平台。



## 为什么开发这个库

关于modbus相关的协议栈，多如牛毛，但实际在项目开发过程中大多数协议栈都很难实际应用，原因如下:

- 客户给的工期短，学习比较全面的协议栈花费时间多，且接口复杂，对于新手难于快速应用。
- 老板开发产品预算紧张，尽管资源丰富的mcu很多，但是很多时候，老板不愿意多花哪怕一分钱。
- 配合市面上大多数云协议和应用场景，modbus协议上云，串口屏幕同步显示。
- 这个库主要用于满足上面这些场景，在我实际开发项目过程中，我深知客户对于产品要求的多变性，很难有一套软件包能真正适配所有的应用场景，而不加以调整，我想那是不可能的，至少我目前没遇到过。
- 当然，这套协议也是如此，实际使用时需结合公司应用场景酌情考虑。
- 把它独立出来，并整合到开源生态，方便后续自己、公司或者其他有需要的人能一键式配置使用。



## 主要特性

- 资源占用极低。
- 性能优越，数据处理几乎**0**拷贝。
- 主机和从机都基于单一的操作接口`operatex`接口便可以操作所有。
- 经过裁剪后可直接运行到51单片机、stm32f030之类的小资源单片机上。
- 特色功能：modbus端口支持shell控制台转移。
- 特别支持有人云的0x46指令。



## 目录结构

| 名称    | 说明             |
| :------ | :--------------- |
| doc     | 文档             |
| samples | 例子             |
| figures | 素材             |
| inc     | 头文件           |
| src     | 源代码           |
| port    | 平台移植相关接口 |



## 如何使用

下面分别提供在**rt-thread**和**freertos**下使用例子

### **rt-thread**

重点关注`lhc_modbus_port.c`

- 定义modbus对象

```c
/*定义Modbus对象*/
p_lhc_modbus_t lhc_modbus_test_object;
```



- 定义一个modbus寄存器池

```c
/*定义一个共用寄存器池*/
static lhc_modbus_pools_t Spool;
```



- 实现crc校验接口（**必须**）

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



- 实现内部数据处理回调函数接口（**非必须**）

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



- 在多线程或者中断中使用时，需要加锁保护（**非必须**）

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



- 实现modbus错误处理函数（**非必须**）

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



- 实现modbus底层发送函数（**必须**）

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

**注意：以上接口均可以使用现成的。**

如果使用**freertos**，则以上**api**仅仅需要替换操作系统相关的部分即可。



下面演示，具体使用：

- 根据自身需求，完善**lhc_modbus_init**函数

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



- 如果支持os，则创建一个数据接收处理线程

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



#### 效果演示

<img src="figures\lhc_modbus操作四类寄存器并读取结果.png" style="zoom:80%;" />

<img src="figures\lhc_modbus基本寄存器读写.gif" style="zoom:80%;" />

### 控制台转移（对rt-thread原生支持）

#### **用途**

- 用于直接转移shell控制台到任意串口端，然后支持本地、远程直接使用shell原生生态。
- 使用`ymodem`协议通过不同串口升级mcu内置固件。
- 自定义shell指令，然后通过不同的串口本地、远程控制mcu，来实现一些特殊功能。

#### **配置**

- 重载`lhc modbus`的内置`ota`方法

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



- 添加finsh控制台自定义指令

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



#### 效果演示

<img src="figures\lhc_modbus执行控制台转移.gif" style="zoom:80%;" />



### 其他说明

- 主机例程暂时没有添加
- modbus tcp暂时不支持
- 当前控制台转移功能支持自定义指令，当暂时不开放重载自定义识别功能
- 更多有趣、实用的功能，不妨大胆的发挥你的想象力，进行探索吧




## 联系方式 & 感谢

- 维护：LHC324
- 主页：https://github.com/LHC324/lhc_modbus
- 邮箱：[1606820017@qq.com](mailto:1606820017@qq.com)



## 许可

采用 Apache-2.0 开源协议，细节请阅读项目中的 LICENSE 文件内容。