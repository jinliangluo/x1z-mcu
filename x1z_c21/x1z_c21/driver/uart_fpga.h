/*
 * uart_fpga.h
 *
 * Created: 2018/10/13 10:25:35
 *  Author: Administrator
 */ 



#ifndef UART_FPGA_H_
#define UART_FPGA_H_

#include <atmel_start.h>

struct can_frame{
	uint32_t can_id;
	uint8_t can_dlc;
	uint8_t data[8];
};




extern volatile uint32_t fpga_rcv_total_cnt;
extern volatile uint32_t fpga_send_total_cnt;
extern volatile uint32_t can_rcv_total_cnt;
extern volatile uint32_t can_send_total_cnt;

extern volatile uint32_t fpga_list_deep;

void can_rcv_2_fpga_send(struct can_message *msg);
void fpga_print_dataes(char *data, uint8_t len);
void uart_fpga_init(void);
void uart_fpag_stop(void);
void uart_fpga_process(void);


#endif /* UART_FPGA_H_ */