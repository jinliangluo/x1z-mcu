#include <atmel_start.h>
#include "driver_examples.h"
#include "./driver/elog_drv.h"
#include "./driver/uart_fpga.h"
#include "./driver/can_drv.h"

#include "headfile.h"

void adas_power_enable(void)
{
	gpio_set_pin_level(PIN_FPGA_POWER, true);
	gpio_set_pin_direction(PIN_FPGA_POWER, GPIO_DIRECTION_OUT);
}

void adas_power_disable(void)
{
	gpio_set_pin_level(PIN_FPGA_POWER, false);
}

#if 0
extern uint32_t uart_rcv_total;
static void can0_send_task1_cb(const struct timer_task *const timer_task)
{
	//CAN_0_example();
	struct can_message msg;
	msg.id = 0x55;
	msg.len = 4;
	msg.fmt = CAN_FMT_STDID;
	msg.type = CAN_TYPE_DATA;
	msg.data = (uint8_t*)&uart_rcv_total;
	can_async_write(&CAN_0, &msg);
}

static void can0_send_test(void)
{
	struct timer_task tt;
	tt.interval = 1000;
	tt.mode = TIMER_TASK_REPEAT;
	tt.cb = can0_send_task1_cb;
	timer_add_task(&TIMER_0, &tt);
}
#endif




int main(void)
{
	/* Initializes MCU, drivers and middleware */
	atmel_start_init();

	
	//TIMER_0_example();
	elog_init();
	uart_fpga_init();
	
	timer_start(&TIMER_0);
	adas_power_enable();
	can_drv_init();
	wdt_set_timeout_period(&WDT_0, 1000, 4096);
	wdt_enable(&WDT_0);
	wdt_feed(&WDT_0);
	sys_config_set_default();

	//can0_send_test();

	while (1) {
		wdt_feed(&WDT_0);
		elog_process();
		uart_fpga_process();
	}
}
