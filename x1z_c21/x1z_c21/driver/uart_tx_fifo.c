/*
 * uart_tx_fifo.c
 *
 * Created: 2018/10/13 11:16:39
 *  Author: Administrator
 */ 
#include <atmel_start.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include "uart_tx_fifo.h"


void tx_fifo_add(struct uart_tx_type *phead, struct uart_tx_type *node)
{
	struct uart_tx_type *ptmp;
	if (NULL == node || phead == NULL)
	return;
	
	//node->next = NULL;
	if (phead->next == NULL)
	{
		phead->next = node;
		return;
	}
	ptmp = phead->next;
	while (ptmp->next != NULL)
	{
		ptmp = ptmp->next;	
	}
	ptmp->next = node;
	return;
}

void tx_fifo_dec(struct uart_tx_type *phead)
{
	if (phead == NULL || phead->next == NULL)  return;
	struct uart_tx_type *ptmp = phead->next;
	
	phead->next = ptmp->next;

	free(ptmp);
}

uint32_t tx_fifo_write(struct uart_tx_type *phead, char *dat, uint32_t len)
{
	struct uart_tx_type *prt = malloc(sizeof(struct uart_tx_type) + len);
	if (prt == NULL)
	return 0;
	
	prt->next = NULL;
	prt->len = len;
	prt->dat = ((char *)prt + sizeof(struct uart_tx_type));
	memcpy(prt->dat, dat, len);
	tx_fifo_add(phead, prt);
	return len;
}
