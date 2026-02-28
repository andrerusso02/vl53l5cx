#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/gpio.h>

#include "vl53l5cx_api.h"


enum sensor_attribute_vl53l5cx {
    SENSOR_ATTR_VL53L5CX_RANGING_STATE = SENSOR_ATTR_PRIV_START,
};

struct vl53l5cx_config {
	struct i2c_dt_spec i2c;
	// struct gpio_dt_spec xshut;
};



struct vl53l5cx_data {
    VL53L5CX_Configuration st_device;
    VL53L5CX_ResultsData last_results;
    bool is_ranging;
    uint8_t resolution;
};
