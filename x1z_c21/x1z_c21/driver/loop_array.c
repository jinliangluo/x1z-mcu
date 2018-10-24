/*
 * loop_array.c
 *
 * Created: 2018/10/17 17:51:50
 *  Author: Administrator
 */ 
#include <atmel_start.h>
#include <string.h>
#include "loop_array.h"
#include "elog_drv.h"

int32_t loop_array_init(struct loop_array_t *la, uint8_t *buf, uint32_t buf_len)
{
	if (la == NULL || buf == NULL)
		return ERR_BAD_ADDRESS;
	if (buf_len == 0)
		return ERR_WRONG_LENGTH;
		
	la->data = buf;
	la->size = buf_len;
	la->rd = 0;
	la->wr = 0;
	return ERR_NONE;
}

int32_t loop_array_write(struct loop_array_t *la, uint8_t *src, uint32_t len)
{
	if (la == NULL || src == NULL || len == 0)
		return ERR_NONE;
		
	//uart_rcv_total += len;

	// 以下方式暂未考虑数据溢出情况
	uint32_t len1,len2;
	len1 = ((len + la->wr) <= la->size) ? len : (la->size - la->wr);
	len2 = len - len1;

	// 缓存区数据溢出
	if (((la->wr > la->rd) && (len2 >= la->rd && len2 != 0))
		|| ((la->wr < la->rd) && (la->wr + len > la->rd)))
	{
		elog_printf("loop-array read over flow:rd=%ld, wr=%ld, len = %ld\r\n", la->rd, la->wr, len);
		la->rd = 0;
		la->wr = 0;
		len1 = len;
		len2 = 0;
		return ERR_NO_MEMORY;
	}
	memcpy(&(la->data[la->wr]), src, len1);
	memcpy(la->data, &src[len1], len2);

	la->wr = (la->wr + len) % la->size;
	return len;
}

int32_t loop_array_read(struct loop_array_t *la, uint8_t *dest, uint32_t max_len)
{
	if (la == NULL || dest == NULL || max_len == 0)
	return ERR_NONE;
	
	uint32_t ret_cnt = 0;
	
	// 串口在中断中接收此处读取应屏蔽中断
	CRITICAL_SECTION_ENTER();
	
	// 将串口数据从缓存区uart_fpga_rx_fifo中读取，本次读取的最大长度为max_len
	if (la->rd < la->wr)
	{
		ret_cnt = min(la->wr - la->rd, max_len);
		memcpy(dest, &(la->data[la->rd]), ret_cnt);
		la->rd += ret_cnt;
	}
	else if (la->rd > la->wr)
	{
		ret_cnt = min(la->wr + la->size - la->rd, max_len);
		int len1 = min(la->size - la->rd, ret_cnt);
		memcpy(dest, &(la->data[la->rd]), len1);
		memcpy(&dest[len1], la->data, ret_cnt - len1);
		la->rd = (la->rd + ret_cnt) % la->size;
	}
	else
	{
		;
	}
	CRITICAL_SECTION_LEAVE();
	return ret_cnt;
}