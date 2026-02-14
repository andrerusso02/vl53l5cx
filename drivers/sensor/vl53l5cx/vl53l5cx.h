#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/sensor.h>

struct vl53l5cx_config {
	// struct i2c_dt_spec i2c;
	// struct gpio_dt_spec xshut;
};



struct vl53l5cx_data {
	// bool started;
	// VL53L5CX_ResultsData results;
};


static const struct sensor_driver_api vl53l5cx_api_funcs = {
    .sample_fetch = NULL,
    .channel_get = NULL,
};
