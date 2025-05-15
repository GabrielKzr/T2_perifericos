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

static int slb_master_transfer(const struct device_s *dev, uint8_t data)
{
	struct slb_config_s *config;
	int i;

	config = (struct slb_config_s *)dev->config;

	int len_data = sizeof(data);

	for (i = 0; i < 8; ++i) {

		config->gpio_sdl(0); // set data low
		_delay_us(config->sync_time); // delay de sync de 33us
		
		config->gpio_sdl(1); // set data high
		_delay_us((((data & 0x80) ==  len_data<<7) + 1) *config->sync_time); // delay de sync de 33us

		data <<= 1; // pega o proximo bit
	}

	return 0;
}

static int slb_read_bit(const struct device_s *dev)
{
  	struct slb_config_s *config;
	int counter = 0, val;
	uint64_t lasttime = 0, actualtime = 0;

	config = (struct slb_config_s *)dev->config;

	while(!config->gpio_sdl(-1)); // espera de fato o bit começar a ser enviado (tempo de sync)

	lasttime = _read_us(); // pega o tempo atual
	while (config->gpio_sdl(-1)); // espera o tempo em que o barramento fica em high
	actualtime = _read_us(); // pega o tempo atual

	val = actualtime - lasttime; // calcula o tempo que ficou em high

	if(val > 80) { // tempo do stop bit
		return -1; // stop bit
	} else if (counter > 40) { // entre 80us e 40us é bit 1
		return 1; // bit 1
	} else { // menor que 40us é bit 0
		return 0; // bit 0
	}
}

static int slb_read_byte(const struct device_s *dev)
{
	struct slb_config_s *config;
	uint8_t byte;
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
    if (val < 0) return val;

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
	struct slb_config_s *config;
	struct slb_data_s *data;
	int retval = 0;

	config = (struct slb_config_s *)dev->config;
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

	if(!retval && config->device_mode == SLB_MASTER) {
		config->gpio_sdl(0); 

		_delay_us(900); // delay de sync de 900us para iniciar select

		config->gpio_sdl(1); 

		_delay_us(100); // delay de sync de 100us para start

		//config->gpio_sdl(0); acho que não seta aqui e sim no write, pensar!

	} else {
		return -1; // slave mode não manda open
	}

	return retval;
}

static int slb_driver_close(const struct device_s *dev) 
{
	struct slb_config_s *config;
	struct slb_data_s *data;
	
	config = (struct slb_config_s *)dev->config;
	data = (struct slb_data_s *)dev->data;

	if(!data->init) return -1;

	if(config->device_mode == SLB_MASTER) {

		config->gpio_sdl(1); 

		_delay_us(100); // delay de sync de 100us para start

	} else {
		return -1; // slave mode não manda close
	}

	CRITICAL_ENTER();
	data->busy = 0;
	CRITICAL_LEAVE();

	return 0;
}

static size_t slb_driver_read(const struct device_s *dev, void *buf, size_t count) 
{
	struct slb_config_s *config;
	struct slb_data_s *data;
	uint8_t *p;
	int i, val = 0;
	uint64_t lasttime = 0, actualtime = 0;
	uint32_t checksum = 0;

	config = (struct slb_config_s *)dev->config;
	data = (struct slb_data_s *)dev->data;
	p = (uint8_t *)buf;

	if(!data->init) return -1;

	NOSCHED_ENTER();

	// espera até que o barramento vá para high
	while(!config->gpio_sdl(-1)); // espera acabar o tempo de select
	
	// espera até que o barramento vá para low
	lasttime = _read_us(); 
	while (config->gpio_sdl(-1)); // espera de fato o bit começar a ser enviado
	actualtime = _read_us(); // pega o tempo atual

	// verifica se o tempo que ficou esperando é o tempo minimo definido para start
	if(actualtime - lasttime < 80) return -1; // tempo de start muito curto

	// o +1 é porque tem o bit a mais do checksum
	for(i = 0; i < count + 1; i++) { // número máximo de bytes a serem lidos, se passar desse valor, barramento só não será mais lido
		val = slb_read_byte(dev); // read data
		
		if(val < 0) break; // se for stop bit, sai do loop

		p[i] = val; // do contrário, armazena o byte lido
		checksum += val; // soma o checksum
	}

	if(p[i-1] != (uint8_t)(checksum%256)) {
		return -1;
	}

	NOSCHED_LEAVE();

	return i;
}

static size_t slb_driver_write(const struct device_s *dev, void *buf, size_t count) 
{
	struct slb_config_s *config;
	struct slb_data_s *data;
	uint8_t *p;
	int i;
	
	config = (struct slb_config_s *)dev->config;
	data = (struct slb_data_s *)dev->data;
	p = (uint8_t *)buf;

	if(!data->init) return -1;

	uint32_t checksum = 0;

	// REMEMBER -> SLAVE JUST TRANSMIT WHEN MASTER SENDS A READ REQUEST

	NOSCHED_ENTER();

	for (i = 0; i < count; i++) { 
		
		slb_master_transfer(dev, p[i]);
	
		checksum += p[i]; // soma o checksum
	}	

	slb_master_transfer(dev, (uint8_t)(checksum%256)); // envia o checksum

	config->gpio_sdl(1); // antes de select sempre ta high

	NOSCHED_LEAVE();

	return i;
}

/* device driver function mapping for generic API */
struct device_api_s slb_api = {
	.dev_init = slb_driver_init,
	.dev_deinit = NULL,
	.dev_open = slb_driver_open,
	.dev_close = slb_driver_close,
	.dev_read = slb_driver_read,
	.dev_write = slb_driver_write
};

