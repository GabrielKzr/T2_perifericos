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
	case -1: return ((gpio_dev_api->gpio_get(gpio) & GPIO_PIN7) >> 7);
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
    .gpio_sdl = gpio_sdl
};

struct slb_data_s slb_data;

const struct device_s slb_device = {
	.name = "slbdevice",
	.config = &slb_config,
	.data = &slb_data,
	.api = &slb_api
};

const struct device_s *slb_master = &slb_device;

void slb_lite_buffwrite(uint8_t device, uint8_t *buf, uint8_t size)
{
    char data[33];
    uint8_t byte = (device << 1) | 1;  // 1 para escrita, 0 para leitura

    data[0] = byte;

    if(size > 32) size = 32;
    memcpy(data + 1, buf, size);

    dev_open(slb_master, 0);

    dev_write(slb_master, data, size + 1);

    dev_close(slb_master);

    // _delay_ms(800); // delay só pro protocolo respirar, na prática não tem necessidade, só para testes
}

void idle(void)
{
	for (;;);
}

void task0(void)
{
	uint8_t buf[100];
	
	printf("SLB_LITE: task0()\n");

	while (1) {
		//gpio_dev_api->gpio_clear(gpio, GPIO_PIN7);
		//_delay_us(500); // delay de sync de 900us para iniciar select
		//gpio_dev_api->gpio_set(gpio, GPIO_PIN7); 
		//_delay_us(500); // delay de sync de 100us para start
		
		for(int i = 0; i < 100; i++) {
			buf[i] = i + 240;
		}
		
		slb_lite_buffwrite(0x01, buf, 4);
		printf("uepa\n");
        // por enquanto não faz nada, talvez seja interessante colocar o teste de escrita aqui
		// _delay_ms(2000);
	}
}

int32_t app_main(void)
{
	ucx_task_spawn(idle, DEFAULT_STACK_SIZE);
	ucx_task_spawn(task0, DEFAULT_STACK_SIZE);

	dev_init(slb_master);

	// start UCX/OS, preemptive mode
	return 1;
}
