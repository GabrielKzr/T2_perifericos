#include <ucx.h>
#include <device.h>
#include <slb_lite.h>

/* SLB LITE master (bit bang) API function wrappers */
int slb_init(const struct device_s *dev)
{
	return dev->api->dev_init(dev);
}

int slb_open(const struct device_s *dev, int mode)
{
	return dev->api->dev_open(dev, mode);
}

int slb_close(const struct device_s *dev)
{
	return dev->api->dev_close(dev);
}

size_t slb_read(const struct device_s *dev, void *buf, size_t count)
{
	return dev->api->dev_read(dev, buf, count);
}

size_t slb_write(const struct device_s *dev, void *buf, size_t count)
{
	return dev->api->dev_write(dev, buf, count);
}

static int slb_master_transfer(const struct device_s *dev, char data)
{
	struct slb_config_s *config;
	int i;

	config = (struct slb_config_s *)dev->config;

	for (i = 0; i < 8; ++i) {
		if (data & 0x80) {
			config->gpio_sdl(1); // set data high
			_delay_us(2*config->sync_time); // se for 1, delay de 66us
		} else {
			config->gpio_sdl(1); // set data high
			_delay_us(config->sync_time); // se for 0, delay de 33us
		}

		data <<= 1; // pega o proximo bit

		config->gpio_sdl(0); // set data low
		_delay_us(config->sync_time); // delay de sync de 33us
	}

	return 0;
}

static int slb_read_bit(const struct device_s *dev)
{
	char bit;
  	struct slb_config_s *config;
	int counter = 0;

	config = (struct slb_config_s *)dev->config;

	while (1)
	{
		if(config->gpio_sdl(-1) == 0) { // se o barramento estiver em low, significa que o slave ta respondendo
			if(counter > 8) { // maior que 80us é stop bit
				return -1; // stop bit
			} else if (counter > 4) { // entre 80us e 40us é bit 1
				return 1; // bit 1
			} else { // menor que 40us é bit 0
				return 0; // bit 0
			}
		} else { // entra aqui quando barramento ta em high
			counter++;
			_delay_us(10); // verifica de 10 em 10us
		}
	}
}

static int slb_read_byte(const struct device_s *dev)
{
	struct slb_config_s *config;
	char byte;
	unsigned bit;
	int val;

	config = (struct slb_config_s *)dev->config;
	
	for(bit = 0; bit < 8; bit++) {
		_delay_us(config->sync_time); // delay de sync de 33us
		val = slb_read_bit(dev); // read data

		if (val < 0) return val;

		byte = (byte << 1) | val;
	}

	return byte;
}

static int slb_driver_init(const struct device_s *dev) 
{
    struct slb_config_s *config;
	struct slb_data_s *data;
	int val;

    config = (struct slb_config_s *)dev->config;
    data = (struct slb_data_s *)dev->data;

    data->busy = 0;
    data->init = 0;

    val = config->gpio_configpins();
    if (val < 0)
        return val;

	if (config->device_mode == SLB_MASTER) {
		config->gpio_sdl(1); // set to master mode (começa em high)
	} else if (config->device_mode == SLB_SLAVE) {
		config->gpio_sdl(-1); // set to slave mode (slave só ouve)
	} else {
		return -1; // invalid mode
	}

	data->init = 1;

	return 0;
}

static int slb_driver_open(const struct device_s *dev, int mode) 
{
	struct slb_data_s *data;
	int retval = 0, val;

	data = (struct slb_data_s *)dev->data;

	if(!data->init) return -1;

	CRITICAL_ENTER();
	if (!data->busy) {
		data->busy = 1;
	} else {
		retval = -1;
	}
	CRITICAL_LEAVE();

	if(mode) return -1; // no mode supported

	// --------------------------
	// start the device (se for fazer algum comando para o barramento, precisa ser aqui)
	// --------------------------

	return retval;
}

static int slb_driver_close(const struct device_s *dev) 
{
	struct slb_config_s *config;
	struct slb_data_s *data;
	
	config = (struct slb_config_s *)dev->config;
	data = (struct slb_data_s *)dev->data;

	if(!data->init) return -1;

	// --------------------------
	// stop the device (se for fazer algum comando para o barramento, precisa ser aqui)
	// --------------------------

	CRITICAL_ENTER();
	data->busy = 0;
	CRITICAL_LEAVE();

	return 0;
}

static size_t slb_driver_read(const struct device_s *dev, void *buf, size_t count) 
{
	struct slb_config_s *config;
	struct slb_data_s *data;
	char *p;
	int i, val = 0;
	int counter = 0;

	config = (struct slb_config_s *)dev->config;
	data = (struct slb_data_s *)dev->data;
	p = (char *)buf;

	if(!data->init) return -1;

	if(config->device_mode == SLB_MASTER) {

		NOSCHED_ENTER();

		while (1)
		{
			if(config->gpio_sdl(-1) == 0) { // se o barramento estiver em low, significa que o slave ta respondendo
				if(counter > 15) { // maior q 100 e menor que 800, pra abranger um caso onde a leitura começa atrasada (a cada 1 são 10us)
					break;
				} else {
					return -1; // tempo começou errado ou leitura muito atrasada
				}
			} else { // entra aqui quando barramento ta em high
				counter++;
				_delay_us(10); // verifica de 10 em 10us
			}
		}
		
		counter = 0;

		while (1)
		{
			if(config->gpio_sdl(-1)) {
				if(counter > 8) {
					break;	
				} else {
					return -1; // tempo errado pra start
				}
			} else {
				counter++;
				_delay_us(10); // verifica de 10 em 10us
			}
		}

		for(i = 0; i < count; i++) { // número máximo de bytes a serem lidos
			val = slb_read_byte(dev); // read data
			
			if(val < 0) break; // se for stop bit, sai do loop

			p[i] = val; // do contrário, armazena o byte lido
		}

		NOSCHED_LEAVE();

		return i;
	} else {
		return -1; // slave mode not supported (does not receive)
	}
}

static size_t slb_driver_write(const struct device_s *dev, void *buf, size_t count) 
{
	struct slb_config_s *config;
	struct slb_data_s *data;
	char *p;
	char newdata;
	int i;
	
	config = (struct slb_config_s *)dev->config;
	data = (struct slb_data_s *)dev->data;
	p = (char *)buf;

	if(!data->init) return -1;

	if(config->device_mode == SLB_MASTER) {

		NOSCHED_ENTER();

		config->gpio_sdl(0); // set data low (inicia select)

		_delay_us(800); // delay de sync de 800us para iniciar select

		config->gpio_sdl(1); // set data high (inicia start)

		_delay_us(100); // delay de sync de 100us para start

		config->gpio_sdl(0); // set data low (sync bit)

		_delay_us(config->sync_time); // delay de sync de 33us

		for (i = 0; i < count; i++) {
			slb_master_transfer(dev, p[i]);
		}

		config->gpio_sdl(1); // antes de select sempre ta high

		NOSCHED_LEAVE();

		return i;
	} else {
		return -1; // slave mode not supported (does not transmit)
	}
}

/* device driver function mapping for generic API */
struct device_api_s i2c_api = {
	.dev_init = slb_driver_init,
	.dev_deinit = NULL,
	.dev_open = slb_driver_open,
	.dev_close = slb_driver_close,
	.dev_read = slb_driver_read,
	.dev_write = slb_driver_write
};

