/*
 * elog_drv.h
 *
 * Created: 2018/9/30 21:04:11
 *  Author: Administrator
 */ 


#ifndef ELOG_DRV_H_
#define ELOG_DRV_H_


#ifdef __cplusplus
extern "C" {
#endif
	

struct sys_config_t{
	uint8_t elog_print;
	uint8_t fpga_lookback;
	uint8_t elog_2_fpga;
	uint8_t fpga_2_elog;
	uint8_t fpga_2_can;
	uint8_t can_2_fpga;
	
};

struct sys_config_t sys_config;
	
extern uint8_t fpga_uart_loopback;
extern uint8_t can_2_fpga_flag;
extern uint8_t fpga_2_can_flag;
	
void sys_config_set_default(void);
void elog_printf(const char *fmt, ...);
void elog_init(void);
void elog_process(void);
void elog_print_dataes(char *str, uint8_t len);



#ifdef __cplusplus
}
#endif	
	




#endif /* ELOG_DRV_H_ */