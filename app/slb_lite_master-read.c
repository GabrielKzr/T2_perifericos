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
	case -1:gpio_dev_api->gpio_set(gpio, GPIO_PIN7);
			return ((gpio_dev_api->gpio_get(gpio) & GPIO_PIN7) >> 7);
	case 0: gpio_dev_api->gpio_clear(gpio, GPIO_PIN7); return 0;
	case 1: gpio_dev_api->gpio_set(gpio, GPIO_PIN7); return 0;
	default: return -1;
	}
}

/* SLB_LITE (bitbang) configuration and driver instantiation */
const struct slb_config_s slb_config = {
	.sync_time = 33,
    .device_mode = SLB_MASTER,
    .own_address = 0x00,
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

const struct device_s *slb_master = &slb_device;

void idle(void)
{
	for (;;);
}

void task0(void)
{
	uint8_t buf_write[100];
	uint8_t buf_read[100];
	int ret = 0;
	
	printf("SLB_LITE: task0()\n");

	memset(buf_read, 0, sizeof(buf_read)); 
	memset(buf_write, 0, sizeof(buf_write)); 

	// _delay_ms(3000);

	buf_write[0] = 0x02;
	uint32_t value = 536871524;
	buf_write[1] = (value >> 24) & 0xFF;
	buf_write[2] = (value >> 16) & 0xFF;
	buf_write[3] = (value >> 8) & 0xFF;
	buf_write[4] = value & 0xFF;

	dev_open(slb_master, 0);
	while (1) {
		ret = 0;

		memset(buf_read, 0, sizeof(buf_read)); 

		dev_write(slb_master, buf_write, 5);

		ret = dev_read(slb_master, buf_read, 10);

		printf("ret = %d\n", ret);
		if(ret < 0) printf("Erro na leitura\n");

		for(int i = 0; i < 5; i++) {
			printf("buf[%d] = %d\n", i, buf_read[i]);
		}

		// TALVEZ SEJA INTERESSANTE TER DELAY PARA GARANTIA DE ENTREGA DE DADOS
		// _delay_ms(800);
	}

	dev_close(slb_master);
}

int32_t app_main(void)
{
	//ucx_task_spawn(idle, DEFAULT_STACK_SIZE);
	ucx_task_spawn(task0, DEFAULT_STACK_SIZE);

	dev_init(slb_master);

	// start UCX/OS, preemptive mode
	return 1;
}
