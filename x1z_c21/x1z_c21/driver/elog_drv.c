/*
 * elog_drv.c
 *
 * Created: 2018/9/30 21:02:11
 *  Author: ljl
 *   noted: 串口SERCOM2（ELOG）用于与PC通信，波特率配置为240400（hpl_sercom_config.h）
 */ 
#include <string.h>
#include <stdio.h>
#include <malloc.h>
#include <stdarg.h>
#include "uart_tx_fifo.h"
#include "elog_drv.h"
#include "uart_fpga.h"
#include "driver_init.h"
#include "utils.h"
#include "headfile.h"
#include "loop_array.h"




#define USART_RX_END_WORD			'$'
#define USART_RX_LENGTH				4096
#define ELOG_ONLINE_TIME			1000*30	// 30S

struct usart_async_descriptor * const USART_ELOG = &USART_0;

//static volatile uint32_t elog_rx_off = 0;	// 数据接收偏移位置
static volatile uint8_t  elog_rx_buf[USART_RX_LENGTH];	// 数据接收缓存
static volatile uint8_t  elog_tx_unfinish = 0;	// 标识还有串口数据待发送

uint32_t elog_list_deep = 0;
static struct uart_tx_type pElog_head = {NULL, 0, NULL};
struct loop_array_t array_elog;

struct sys_config_t sys_config;



#if 1
// 实现elog输出的超时关闭功能（超时则停止打印elog信息）

static struct timer_task elog_online_task;

static void elog_keep_online_cb(const struct timer_task *const timer_task)
{
	sys_config.elog_print = 0;
}


static int32_t elog_keep_online_redelay(uint32_t delay)
{
	if (delay == 0)
	return ERR_INVALID_DATA;
	
	timer_remove_task(&TIMER_0, &elog_online_task);
	
	elog_online_task.mode = TIMER_TASK_ONE_SHOT;
	elog_online_task.interval = delay;
	elog_online_task.cb = elog_keep_online_cb;
	return timer_add_task(&TIMER_0, &elog_online_task);
}
#endif


void sleep_preApp(void)
{
	while (pElog_head.next)
	{
		tx_fifo_dec(&pElog_head);
	}
	usart_async_disable(&USART_0);
	
	uart_fpag_stop();
	
	can_async_disable(&CAN_0);
	gpio_set_pin_level(PIN_CAN_MODE, true);
	gpio_set_pin_level(PIN_FPGA_POWER, false);
	
	wdt_disable(&WDT_0);
	timer_stop(&TIMER_0);
	sleep(4);
}



struct timer_task fpga_reboot_tt;
void fpga_reboot_cb(const struct timer_task *const timer_task)
{
	gpio_set_pin_level(PIN_FPGA_POWER, true);
	elog_printf("fpga_reboot success\r\n");
}

void fpga_reboot_f(uint32_t delay)
{
	if (delay <= 10)
	{
		elog_printf("fpga_reboot failed: delay time too low\r\n");	
		return;
	}
	fpga_reboot_tt.mode = TIMER_TASK_ONE_SHOT;
	fpga_reboot_tt.cb = fpga_reboot_cb;
	fpga_reboot_tt.interval = delay;
	timer_add_task(&TIMER_0, &fpga_reboot_tt);
}
	
void sys_config_set_default(void)
{
	sys_config.elog_print = 1;
	sys_config.can_2_fpga = 1;
	sys_config.fpga_2_can = 1;
	sys_config.elog_2_fpga = 0;
	sys_config.fpga_2_elog = 0;
	sys_config.fpga_lookback = 0;
}

uint8_t wakeup_flag = 0;

void can_wakeup_cb(void)
{
	wakeup_flag = 1;	
}


static void elog_rx_deal(char *dat, uint32_t len)
{
	if (len <= 0)
	return;
	
	if ((sys_config.elog_print == 0) && (strstr(dat, "elogstart") != NULL))
	{
		elog_keep_online_redelay(ELOG_ONLINE_TIME);	// 注释此函数，不启动超时自动关闭elog功能
		sys_config.elog_print = 1;
		elog_printf("elog start\r\n");
		return;
	}
	if ((sys_config.elog_print != 0) && (strstr(dat, "elogstop") != NULL))
	{
		elog_printf("elog stop\r\n");
		timer_remove_task(&TIMER_0, &elog_online_task);
		sys_config.elog_print = 0;
		return;
	}
	if (strstr(dat, "fl=en"))
	{
		sys_config.fpga_lookback = 1;
		elog_printf("config fpga-loopback:enable\r\n");
		return;
	}
	if (strstr(dat, "fl=dis"))
	{
		sys_config.fpga_lookback = 0;
		elog_printf("config fpga-loopback:disable\r\n");
		return;
	}
	if (strstr(dat, "c2f=en"))
	{
		sys_config.can_2_fpga = 1;
		elog_printf("config can-to-fpga:enable\r\n");
		return;
	}
	if (strstr(dat, "c2f=dis"))
	{
		sys_config.can_2_fpga = 0;
		elog_printf("config can-to-fpga:disable\r\n");
		return;
	}
	if (strstr(dat, "f2c=en"))
	{
		sys_config.fpga_2_can = 1;
		elog_printf("config fpga-to-can:enable\r\n");
		return;
	}
	if (strstr(dat, "f2c=dis"))
	{
		sys_config.fpga_2_can = 0;
		elog_printf("config fpga-to-can:disable\r\n");
		return;
	}
	if (strstr(dat, "f2e=en"))
	{
		sys_config.fpga_2_elog = 1;
		elog_printf("config fpga-to-elog:enable\r\n");
		return;
	}
	if (strstr(dat, "f2e=dis"))
	{
		sys_config.fpga_2_elog = 0;
		elog_printf("config fpga-to-elog:disable\r\n");
		return;
	}
	if (strstr(dat, "getsyscfg"))
	{
		char str[20];
		memcpy(str, (char *)&sys_config, sizeof(sys_config));
		str[sizeof(sys_config)] = '\0';
		elog_printf("fl=%s, f2c=%s, c2f=%s, f2e=%s\r\n", 
				sys_config.elog_print ? "enable" : "disable", 
			    sys_config.fpga_lookback ? "enable" : "disable", 
				sys_config.fpga_2_can ? "enable" : "disable", 
				sys_config.can_2_fpga ? "enable" : "disable", 
				sys_config.elog_2_fpga ? "enable" : "disable", 
				sys_config.fpga_2_elog ? "enable" : "disable");
		return;
	}
	if (strstr(dat, "fpga-poweron"))
	{
		gpio_set_pin_level(PIN_FPGA_POWER, true);
		elog_printf("poweronfpga\r\n");
		return;
	}
	if (strstr(dat, "fpga-poweroff"))
	{
		gpio_set_pin_level(PIN_FPGA_POWER, false);
		elog_printf("poweroff fpga\r\n");
		return;
	}
	if (strstr(dat, "fpga-reboot"))
	{
		elog_printf("fpga powerdown...\r\n");
		gpio_set_pin_level(PIN_FPGA_POWER, false);	
		fpga_reboot_f(1000);
		return;
	}
	if (strstr(dat, "get-can-cnt"))
	{
		elog_printf("[CAN]rcv:%d, send:%d\r\n", can_rcv_total_cnt, can_send_total_cnt);
		return;
	}
	if (strstr(dat, "get-fpga-cnt"))
	{
		elog_printf("[FPGA]:rcv:%d, send:%d\r\n", fpga_rcv_total_cnt, fpga_send_total_cnt);
		return;
	}
	if (strstr(dat, "get-list-deep"))
	{
		elog_printf("[DEEP]fpga:%ld, elog:%ld\r\n", elog_list_deep, fpga_list_deep);
		return;
	}
	if (strstr(dat, "sys-reset"))
	{
		_reset_mcu();
		while(1);
	}
	if (strstr(dat, "gosleep"))
	{
		//uint8_t cnt = 0;
		wdt_disable(&WDT_0);
		can_async_disable(&CAN_0);
		gpio_set_pin_level(PIN_CAN_MODE, true);
		gpio_set_pin_level(PIN_FPGA_POWER, false);
		ext_irq_register(PIN_PA25, &can_wakeup_cb);

		ext_irq_enable(PIN_PA25);
		//while (++cnt < 100)
			sleep(4);
		//sleep_preApp();
		can_async_enable(&CAN_0);
		ext_irq_disable(PIN_PA25);
		gpio_set_pin_level(PIN_CAN_MODE, false);
		gpio_set_pin_level(PIN_FPGA_POWER, true);
		return;
	}
	if (strstr(dat, "get-wakesrc"))
	{
		elog_printf("wakeup flag:%d\r\n", wakeup_flag);
		return;
	}
}




static void elog_tx_cb(const struct usart_async_descriptor *const descr)
{
	elog_tx_unfinish = 0;
}


static void elog_rx_cb(const struct usart_async_descriptor *const descr)
{
	int32_t i = 0, len = usart_async_is_rx_not_empty(USART_ELOG);//usart_async_is_tx_empty(USART_ELOG);
	if (len > 0)
	{
		struct io_descriptor *io;
		usart_async_get_io_descriptor(USART_ELOG, &io);
		
		uint8_t buf[64];
		while (i < len)
		{
			int32_t rd_len = io_read(io, buf, 64);
			if (rd_len <= 0)
			break;
			
			int32_t ret = loop_array_write(&array_elog, buf, rd_len);
			if (ret == ERR_NO_MEMORY)
			{
				elog_printf("elog: write loop-array error, exit\r\n");
				break;	
			}
			i += rd_len;
		}
	}
}




// 此函数实现FPGA-TO-ELOG强制转发，故不判断sys_config.elog_print
void elog_print_dataes(char *str, uint8_t len)	
{
	if (str == NULL || len == 0)
	return;
	//if (sys_config.elog_print)	
	tx_fifo_write(&pElog_head, str, len);
	elog_list_deep++;
}

void elog_print_string(char *str)
{
	if (str == NULL)
		return;
	if (sys_config.elog_print)
	{
		tx_fifo_write(&pElog_head, str, strlen(str));
		elog_list_deep++;
	}
}



void elog_process(void)
{
	// 发送部分
	CRITICAL_SECTION_ENTER();
	if (pElog_head.next != NULL && !elog_tx_unfinish)
	{
		struct io_descriptor *io;
		usart_async_get_io_descriptor(USART_ELOG, &io);
		elog_tx_unfinish = 1;
		if (pElog_head.next->len != io_write(io, (uint8_t *)(pElog_head.next->dat), pElog_head.next->len))
		{
			elog_printf("elog write error\r\n");	// 发送出现异常
		}
		tx_fifo_dec(&pElog_head);
		elog_list_deep--;
	}
	CRITICAL_SECTION_LEAVE();
	
	// 接收部分
	static uint8_t cur_rcv_data[256+1];
	static uint32_t cur_rcv_off = 0;
	uint8_t buf[256];
	int32_t read_len = loop_array_read(&array_elog, buf, 256);
	if (read_len < 0)
	{
		elog_printf("elog_process error:read err=%d\r\n", read_len);
		return;
	}
	for (int32_t i = 0; i < read_len; i++)
	{
		cur_rcv_data[cur_rcv_off] = buf[i];
		if (buf[i] == USART_RX_END_WORD || buf[i] == '\n')
		{
			cur_rcv_data[cur_rcv_off + 1] = '\0';
			elog_rx_deal((char *)cur_rcv_data, cur_rcv_off);
			cur_rcv_off = 0;
		}
		else
		{
			cur_rcv_data[cur_rcv_off++] = buf[i];
			if (cur_rcv_off >= 256)
			{
				cur_rcv_off = 0;
				elog_printf("elog_process error:over flow\r\n");
			}
		}
	}
	if (sys_config.elog_2_fpga)
	{
		fpga_print_dataes((char *)buf, read_len);
	}
}

void elog_init(void)
{
	struct io_descriptor *io;
	
	loop_array_init(&array_elog, (uint8_t *)elog_rx_buf, USART_RX_LENGTH);
	
	usart_async_get_io_descriptor(USART_ELOG, &io);
	usart_async_register_callback(USART_ELOG, USART_ASYNC_RXC_CB, elog_rx_cb);
	usart_async_register_callback(USART_ELOG, USART_ASYNC_TXC_CB, elog_tx_cb);
	usart_async_enable(USART_ELOG);
}

void elog_printf(const char *fmt, ...)
{
	char str[256];
	va_list argp;
	va_start(argp, fmt);
	vsprintf(str, fmt, argp);
	va_end(argp);
	elog_print_string(str);
}