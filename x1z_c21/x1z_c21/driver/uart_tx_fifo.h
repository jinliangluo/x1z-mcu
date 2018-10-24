/*
 * uart_tx_fifo.h
 *
 * Created: 2018/10/13 11:17:45
 *  Author: Administrator
 */ 


#ifndef UART_TX_FIFO_H_
#define UART_TX_FIFO_H_

struct uart_tx_type{
	struct uart_tx_type *next;
	int len;
	char *dat;
};



void tx_fifo_add(struct uart_tx_type *phead, struct uart_tx_type *node);
void tx_fifo_dec(struct uart_tx_type *phead);
uint32_t tx_fifo_write(struct uart_tx_type *phead, char *dat, uint32_t len);


#endif /* UART_TX_FIFO_H_ */