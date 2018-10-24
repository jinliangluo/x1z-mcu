/*
 * can_drv.c
 *
 * Created: 2018/10/3 18:17:37
 *  Author: Administrator
 */ 
#include <atmel_start.h>
#include <stdio.h>
#include <string.h>
#include "headfile.h"
#include "can_drv.h"
#include "elog_drv.h"
#include "uart_fpga.h"
#include "uart_tx_fifo.h"

typedef struct{
	uint32_t id;
	uint8_t dlc;
	uint8_t data[8];
	uint16_t timestamp;	
} tCanChipMsg, *tPCanChipMsg;

typedef uint8_t (*tpUserPrecopy)(tPCanChipMsg rxStruct);
typedef void (*tpUserIndication)(uint8_t rxObject);
typedef uint8_t (*tpUserPretransmit)(tPCanChipMsg txStruct);
typedef void (*tpUserConfirm)(uint8_t txObject);

struct can_transmit_t {
	uint32_t id;
	uint8_t dlc;
	uint8_t *pdat;
	tpUserPrecopy pPreCopy;
	tpUserIndication pIndication;
};

struct can_receive_t {
	uint32_t id;
	uint8_t dlc;
	uint8_t *pdat;
	tpUserPretransmit pPretransmit;
	tpUserConfirm pConfirm;
};

uint8_t canRxMsg_123[8];
uint8_t canTxMsg_485[8];



const struct can_transmit_t can_tx_list[]={
	{
		0x123,
		8u,
		canTxMsg_485,
		NULL,
		NULL
	},
};


const struct can_receive_t can_rx_list[]={
	{
		0x123,
		8u,
		canRxMsg_123,
		NULL,
		NULL
	},
};



static void can_0_rx_callback(struct can_async_descriptor *const descr)
{
	struct can_message msg;
	uint8_t            data[8];
	uint32_t index, cnt;
	//char str[100];
	
	msg.data = data;
	
	cnt = hri_can_read_RXF0S_F0FL_bf((&descr->dev)->hw);
	for (int i = 0; i < cnt; i++)
	{
		index = hri_can_read_RXF0S_F0GI_bf((&descr->dev)->hw);
		can_async_read(descr, &msg);
		can_rcv_total_cnt += 1;
		//sprintf(str, "can0-rcv: id=0x%lX, dlc=%d, data=%02X %02X %02X %02X %02X %02X %02X %02X\r\n", msg.id, msg.len, msg.data[0], msg.data[1], msg.data[2], msg.data[3], msg.data[4], msg.data[5], msg.data[6], msg.data[7]);
		//elog_print_string(str);
		hri_can_write_RXF0A_F0AI_bf((&descr->dev)->hw, index);
		if (sys_config.can_2_fpga)
		{
			can_rcv_2_fpga_send(&msg);
		}
	}
	
	cnt = hri_can_read_RXF1S_F1FL_bf((&descr->dev)->hw);
	for (int i = 0; i < cnt; i++)
	{
		index = hri_can_read_RXF1S_F1GI_bf((&descr->dev)->hw);
		can_async_read(descr, &msg);
		can_rcv_total_cnt += 1;
		//sprintf(str, "can0-rcv: id=0x%lX, dlc=%d, data=%02X %02X %02X %02X %02X %02X %02X %02X\r\n", msg.id, msg.len, msg.data[0], msg.data[1], msg.data[2], msg.data[3], msg.data[4], msg.data[5], msg.data[6], msg.data[7]);
		//elog_print_string(str);
		hri_can_write_RXF1A_F1AI_bf((&descr->dev)->hw, index);
		if (sys_config.can_2_fpga)
		{
			can_rcv_2_fpga_send(&msg);
		}
	}
	
	
	return;
}


static void can_filter_set(void)
{
	struct can_filter filter;
	
	filter.id = 0x7df;
	filter.mask = 0x0;
	can_async_set_filter(&CAN_0, 0, CAN_FMT_STDID, &filter);
	
	filter.id = 0x1fffffff;
	filter.mask = 0x0;
	can_async_set_filter(&CAN_0, 1, CAN_FMT_EXTID, &filter);
}

void can_drv_init(void)
{
	gpio_set_pin_level(PIN_CAN_MODE, false);	// 低电平，使能CAN0
	gpio_set_pin_direction(PIN_CAN_MODE, GPIO_DIRECTION_OUT);
	
	can_filter_set();
	can_async_register_callback(&CAN_0, CAN_ASYNC_RX_CB, (FUNC_PTR)can_0_rx_callback);
	
	can_async_enable(&CAN_0);
}
