int slb_init(const struct device_s *dev);
int slb_open(const struct device_s *dev, int mode);
int slb_close(const struct device_s *dev);
size_t slb_read(const struct device_s *dev, void *buf, size_t count);
size_t slb_write(const struct device_s *dev, void *buf, size_t count);

/* SLB Lite configuration definitions */
enum slb_config_values {
    SLB_MASTER, SLB_SLAVE
};

struct slb_config_s {
    int sync_time; // sync bit
    char device_mode; // master or slave mode
    char own_address; // device address
    // int en_write;
    int (*gpio_configpins)(void); // setup pins callback
    int (*gpio_sdl)(int val); // pin read or write callback
};

/* SLB Lite data definitions */
struct slb_data_s {
    char init; // device initialized
    char busy; // device is busy
};

extern struct device_api_s slb_api;