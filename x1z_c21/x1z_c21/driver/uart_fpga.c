/*
 * uart_fpga.c
 *
 * Created: 2018/10/13 10:24:57
 *  Author: Administrator
 */ 
#include <atmel_start.h>
#include <string.h>
#include <malloc.h>
#include <stdio.h>
#include "uart_tx_fifo.h"
#include "uart_fpga.h"
#include "elog_drv.h"
#include "can_drv.h"
#include "driver_init.h"
#include "utils.h"
#include "loop_array.h"

#define UART_FPGA_RX_CAN_FLAG				0x0C
#define USART_FPGA_RX_LENGTH				4096

struct usart_async_descriptor *const USART_FPGA = &USART_1;


enum uart_fpga_type_t {RCV_NONE = 0, RCV_CAN_FRAME};
enum uart_fpga_rx_state {RX_HEAD, RX_TOTAL_CNT, RX_DATA, RX_CALIBRATION, RX_FINISHED};
struct rcv_info_t{
	enum uart_fpga_type_t type;
	enum uart_fpga_rx_state sta;
	uint32_t all_cnt;
	uint32_t index;
	uint8_t data[256];
	uint8_t crc;
};

volatile uint32_t fpga_rcv_total_cnt = 0;
volatile uint32_t fpga_send_total_cnt = 0;
volatile uint32_t can_rcv_total_cnt = 0;
volatile uint32_t can_send_total_cnt = 0;

struct rcv_info_t cur_rcv_info = {RCV_NONE, RX_HEAD, 0, 0, {0}, 0};

volatile uint32_t fpga_list_deep = 0;
static struct uart_tx_type phead_fpga_tx = {NULL, 0, NULL};

static volatile uint8_t  uart_fpga_tx_unifinish = 0;	// 一帧串口数据没有发送完成(发送中断中清除此状态)

struct loop_array_t fpga_array;	// elog接收循环数据信息
static volatile uint8_t  uart_fpga_rx_fifo[USART_FPGA_RX_LENGTH];




static void uart_fpga_rx_cb(const struct usart_async_descriptor *const descr)
{
	uint32_t len = usart_async_is_rx_not_empty(USART_FPGA);
	uint8_t buff[50];

	if (0 < len)
	{
		struct io_descriptor *io;
		usart_async_get_io_descriptor(USART_FPGA, &io);
		len = io_read(io, buff, len);
		fpga_rcv_total_cnt += len;
		int32_t ret = loop_array_write(&fpga_array, buff, len);
		if (ret == ERR_NO_MEMORY)
		{
			elog_printf("fpga_uart: write loop-array error\r\n");
		}
	}
}

static void uart_fpga_tx_cb(const struct usart_async_descriptor *const descr)
{
	uart_fpga_tx_unifinish = 0;
}

void can_rcv_2_fpga_send(struct can_message *msg)
{
	uint8_t data[16];	// 一组CAN报文的串口包长度最大为 (1+1+4+1+8+1 = 16byte)
	
	if (msg == NULL) return;
	
	data[0] = 0x0C;
	
	uint32_t id = msg->id;
	if (msg->fmt == CAN_FMT_EXTID)
	id |= CAN_EFF_FLAG;
	if (msg->type == CAN_TYPE_REMOTE)
	id |= CAN_RTR_FLAG;
	
	data[1] = 4 + 1 + msg->len + 1;
	memcpy(&data[2], (uint8_t *)&id, 4);
	data[6] = msg->len;
	memcpy(&data[7], msg->data, msg->len);
	uint8_t crc = 0;
	for (uint8_t i = 0; i < (5 + msg->len); i++)
	{
		crc ^= data[i + 2];
	}
	data[1+1+4+1+ msg->len] = crc;
	fpga_print_dataes((char *)data, 1+1+4+1+ msg->len + 1);
}

/* 用于测试fpga 串口收发是否正常 */
void uart_fpga_loopback_pack(struct can_frame *frame)
{
	struct can_message msg;
	
	msg.id = frame->can_id & 0x1fffffff;
	msg.fmt = (frame->can_id & CAN_EFF_FLAG) ? CAN_FMT_EXTID : CAN_FMT_STDID;
	msg.type = (frame->can_id & CAN_RTR_FLAG) ? CAN_TYPE_REMOTE : CAN_TYPE_DATA;
	msg.len = frame->can_dlc;
	msg.data = frame->data;
	can_rcv_2_fpga_send(&msg);
}


void uart_fpga_rcv_canframe(struct can_frame *frame)
{
	struct can_message msg;
	
	//memset(&msg, 0x00, sizeof(struct can_message));			
	msg.id = frame->can_id & 0x1fffffff;
	msg.type = (frame->can_id & CAN_RTR_FLAG) ? CAN_TYPE_REMOTE : CAN_TYPE_DATA;
	msg.fmt  = (frame->can_id & CAN_EFF_FLAG) ? CAN_FMT_EXTID : CAN_FMT_STDID;
	msg.len = frame->can_dlc;
	msg.data = frame->data;
	
	int32_t err = can_async_write(&CAN_0, &msg);
	if (ERR_NONE != err)
	{
		elog_printf("can_async_write error:%d\r\n", err);
	}
	else
	{
		can_send_total_cnt += 1;
	}
}


static void uart_fpga_rcv_init(void)
{
	cur_rcv_info.all_cnt = 0;
	cur_rcv_info.index = 0;
	cur_rcv_info.crc = 0;
	cur_rcv_info.sta = RX_HEAD;
	cur_rcv_info.type = RCV_NONE;
}


static void uart_fpga_rcv_done(void)
{
	uart_fpga_rcv_init();
}


static void uart_fpga_rx_unpack(uint8_t data)
{
	/*
	* fpga串口数据结构： 头部(1byte) 长度(1byte) 数据区(1-255byte) 校验(1byte) 
	* 长度大小=sizeof(数据区) + sizeof(校验)
	* 校验 crc = 数据区各字节异或的结构 data[0] ^ data[1] ^ ... ^ data[n - 1]
	*/
	switch(cur_rcv_info.sta)
	{
		case RX_HEAD:
		{
			if (data == UART_FPGA_RX_CAN_FLAG)
			{
				cur_rcv_info.type = RCV_CAN_FRAME;
				cur_rcv_info.sta = RX_TOTAL_CNT;
				cur_rcv_info.all_cnt = 0;
			}
			break;
		}
		case RX_TOTAL_CNT:
		{
			if (data > 1)
			{
				cur_rcv_info.sta = RX_DATA;
				cur_rcv_info.all_cnt = data - 1;	
				cur_rcv_info.index   = 0;
			}
			else
			{
				uart_fpga_rcv_init();
			}
			break;
		}
		case RX_DATA:
		{
			cur_rcv_info.data[cur_rcv_info.index] = data;
			cur_rcv_info.crc ^= data;
			if (++cur_rcv_info.index >= cur_rcv_info.all_cnt)
			{
				cur_rcv_info.sta = RX_CALIBRATION;
			}
			break;
		}
		case RX_CALIBRATION:
		{
			if(cur_rcv_info.crc == data)
			{
				cur_rcv_info.sta = RX_FINISHED;
			}
			else
			{
				uart_fpga_rcv_init();	// 当前的操作是将接收到的数据全部抛弃，后续可以在已接收到的数据中重新定位数据
			}
			break;
		}
		case RX_FINISHED:
			break;
		default:
			uart_fpga_rcv_init();
			break;
	}
}

static void uart_fpga_rx_handler(void)
{
	static uint8_t buf[256];
	static uint32_t delay = 0;	// 一定时间内没有接收到数据，则清除已经接收到的数据即接收状态
	int32_t len = loop_array_read(&fpga_array, buf, 256);
	

	if (len <= 0)
	{
		if (cur_rcv_info.sta != RX_HEAD)
			delay++;
			
		if (delay >= 1000)
		{
			delay = 0;
			uart_fpga_rcv_init();
		}
		return;
	}
	delay = 0;
	
	for (uint8_t i = 0; i < len; i++)
	{
		uart_fpga_rx_unpack(buf[i]);
		if (cur_rcv_info.sta == RX_FINISHED)	//一组数据接收完成
		{
			if (cur_rcv_info.type == RCV_CAN_FRAME)
			{
				struct can_frame frame;
				memcpy(&frame, cur_rcv_info.data, sizeof(frame));
				if (sys_config.fpga_2_can)
				{
					uart_fpga_rcv_canframe(&frame/*(struct can_frame *)cur_rcv_info.data*/);	//直接解析成CAN报文转发	
				}
				if (sys_config.fpga_lookback)
				{
					uart_fpga_loopback_pack(&frame);	//转发	
				}
			}
			uart_fpga_rcv_done();
		}
	}
	if (sys_config.fpga_2_elog)
	{
		elog_print_dataes((char*)buf, len);	// 将接收到的adas-uart数据直接通过elog-uart输出，仅用于测试
	}
}

static void uart_fpga_tx_handler(void)
{
	CRITICAL_SECTION_ENTER();
	if (phead_fpga_tx.next != NULL && !uart_fpga_tx_unifinish)
	{
		struct io_descriptor *io;
		usart_async_get_io_descriptor(USART_FPGA, &io);
		uart_fpga_tx_unifinish = 1;
		if (phead_fpga_tx.next->len != io_write(io, (uint8_t *)(phead_fpga_tx.next->dat), phead_fpga_tx.next->len))
		{
			elog_printf("transmit fpga data failed\r\n");
		}
		fpga_send_total_cnt += phead_fpga_tx.next->len;
		tx_fifo_dec(&phead_fpga_tx);
		fpga_list_deep--;
	}
	CRITICAL_SECTION_LEAVE();
}



void uart_fpga_process(void)
{
	uart_fpga_rx_handler();
	uart_fpga_tx_handler();
}

void fpga_print_dataes(char *data, uint8_t len)
{
	if (data == NULL || len == 0)
	return;
	
	tx_fifo_write(&phead_fpga_tx, data, len);
	fpga_list_deep++;
}


void uart_fpga_init(void)
{
	struct io_descriptor *io;
	
	loop_array_init(&fpga_array, (uint8_t *)uart_fpga_rx_fifo, USART_FPGA_RX_LENGTH);
	
	usart_async_get_io_descriptor(USART_FPGA, &io);
	usart_async_register_callback(USART_FPGA, USART_ASYNC_RXC_CB, uart_fpga_rx_cb);
	usart_async_register_callback(USART_FPGA, USART_ASYNC_TXC_CB, uart_fpga_tx_cb);
	usart_async_enable(USART_FPGA);
	
	
	while (usart_async_is_rx_not_empty(USART_FPGA))
	{
		struct io_descriptor *io;
		usart_async_get_io_descriptor(USART_FPGA, &io);
		uint8_t dummy[20];
		io_read(io, dummy, 20);
	}
}


void uart_fpag_stop(void)
{
	while (phead_fpga_tx.next)
	{
		tx_fifo_dec(&phead_fpga_tx);
	}
	usart_async_disable(USART_FPGA);
}

