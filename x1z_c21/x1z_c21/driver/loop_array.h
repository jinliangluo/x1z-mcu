/*
 * loop_array.h
 *
 * Created: 2018/10/17 17:52:12
 *  Author: Administrator
 */ 


#ifndef LOOP_ARRAY_H_
#define LOOP_ARRAY_H_


struct loop_array_t{
	uint32_t rd;
	uint32_t wr;
	uint32_t size;
	uint8_t *data;
};

int32_t loop_array_init(struct loop_array_t *la, uint8_t *buf, uint32_t buf_len);
int32_t loop_array_write(struct loop_array_t *la, uint8_t *src, uint32_t len);
int32_t loop_array_read(struct loop_array_t *la, uint8_t *dest, uint32_t max_len);

#endif /* LOOP_ARRAY_H_ */