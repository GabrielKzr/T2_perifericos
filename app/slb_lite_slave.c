#include <ucx.h>
#include <device.h>
#include <gpio.h>
#include <slb_lite.h>

/* GPIO configuration: PB7 (sda) - port it! */
const struct gpio_config_s gpio_config = {
	.config_values.port	  = GPIO_PORTB,
	.config_values.pinsel = GPIO_PIN7,
	.config_values.mode	  = GPIO_OUTPUT_OD << GPIO_PIN7_OPT,
	.config_values.pull	  = GPIO_NOPULL << GPIO_PIN7_OPT // LEMBRAR QUE NÂO TEM PULL AQUI, TEM QUE COLOCAR NA PLACA, SENÃO F
};

/* GPIO device driver instantiation */
const struct device_s gpio_device = {
	.name = "gpio_device",
	.config = &gpio_config,
	.custom_api = &gpio_api
};

const struct device_s *gpio = &gpio_device;
const struct gpio_api_s *gpio_dev_api = (const struct gpio_api_s *)(&gpio_device)->custom_api;

/* GPIO template callbacks - port them! */

/* configure SDA pin direction
 * 
 * SDA - open drain, output configured as logic low
 */

int gpio_configpins(void)
{
	printf("SLB_LITE: gpio_configpins()\n");
	gpio_dev_api->gpio_setup(gpio);

	return 0;
}

/* read or write SCL and SDA pins
 * if val == -1, read pin and return value (0 or 1)
 * if val == 0, write value low
 * if val == 1, write value high
 */
int gpio_sdl(int val)
{
	switch (val) {
	case -1: gpio_dev_api->gpio_set(gpio, GPIO_PIN7);
	return ((gpio_dev_api->gpio_get(gpio) & GPIO_PIN7) >> 7);
	case 0: gpio_dev_api->gpio_clear(gpio, GPIO_PIN7); return 0;
	case 1: gpio_dev_api->gpio_set(gpio, GPIO_PIN7); return 0;
	default: return -1;
	}
}

/* SLB_LITE (bitbang) configuration and driver instantiation */
const struct slb_config_s slb_config = {
	.sync_time = 33,
    .device_mode = SLB_SLAVE,
    .own_address = 0x01,
	.gpio_configpins = gpio_configpins,
    .gpio_sdl = gpio_sdl,
};

struct slb_data_s slb_data;

const struct device_s slb_device = {
	.name = "slbdevice",
	.config = &slb_config,
	.data = &slb_data,
	.api = &slb_api
};

const struct device_s *slb_slave = &slb_device;

void idle(void)
{
	for (;;);
}

void task0(void)
{
	uint8_t buf[100];
	
	dev_open(slb_slave, 0);

	printf("uepa\n");

	memset(buf, 0, sizeof(buf)); // não sei qual o padrão de 0x69 em bytes na hora que for ver no analisador lógico

	while (1) {

		//cgpio_dev_api->gpio_set(gpio, GPIO_PIN7);

		if (dev_read(slb_slave, buf, 33) < 0) printf("DEU MERDA\n");        
		
		for(int i = 0; i < 35; i++) {
			printf("buf[%d] = %d\n", i, buf[i]);
		}
	
	}

	dev_close(slb_slave);


}

void task2(void) 
{
	uint8_t buf[100];
	
	dev_open(slb_slave, 0);

	printf("uepa\n");

	memset(buf, 0, sizeof(buf)); // não sei qual o padrão de 0x69 em bytes na hora que for ver no analisador lógico

	while (1) {

		if (dev_read(slb_slave, buf, 5) < 0) printf("DEU MERDA\n");        
		
		for(int i = 0; i < 7; i++) {
			printf("buf[%d] = %d\n", i, buf[i]);
		}
	
	}

	dev_close(slb_slave);
}

void task1(void)
{
	static uint8_t buf_write[100];
	uint8_t buf_read[100];
	
	memset(buf_read, 0, sizeof(buf_read)); 
	memset(buf_write, 0, sizeof(buf_write)); 

	int value_returned;
	uint32_t address_read = 0;

	dev_open(slb_slave, 0);

	buf_write[0] = 0x00;
	buf_write[1] = 50;
	buf_write[2] = 51;

	while (1) {
		memset(buf_read, 0, sizeof(buf_read)); 
		
		value_returned = dev_read(slb_slave, buf_read, 10);

		/*for(int i = 0; i < 10; i++) {
			printf("buf[%d] = %d\n", i, buf_read[i]);
		}*/

		// printf("vr=%d\n", value_returned);
		if(value_returned < 0) continue;

		if((buf_read[0] & 0x01) == 0){
			address_read = (uint32_t)buf_read[1] << 24 
						 | (uint32_t)buf_read[2] << 16 
						 | (uint32_t)buf_read[3] << 8 
						 | (uint32_t)buf_read[4];

			if(address_read == 536871524) {
				// _delay_ms(50);
				dev_write(slb_slave, buf_write, 3);
			}	
		}
	}

	dev_close(slb_slave);
}

int32_t app_main(void)
{
	//ucx_task_spawn(idle, DEFAULT_STACK_SIZE);
	ucx_task_spawn(task1, DEFAULT_STACK_SIZE);

	dev_init(slb_slave);

	// start UCX/OS, preemptive mode
	return 1;
}
