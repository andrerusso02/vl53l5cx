#define DT_DRV_COMPAT st_vl53l5cx

#include <errno.h>
#include <zephyr/logging/log.h>

#include "vl53l5cx.h"

LOG_MODULE_REGISTER(VL53L5CX, CONFIG_SENSOR_LOG_LEVEL);

static int vl53l5cx_initialize(const struct device *dev)
{
    const struct vl53l5cx_config *config = dev->config;
    struct vl53l5cx_data *data = dev->data; 

    data->st_device.platform.i2c = &config->i2c;

    if (!device_is_ready(config->i2c.bus)) {
        printk("I2C device not ready\n");
        return -ENODEV;
    }

    if (vl53l5cx_init(&data->st_device) != VL53L5CX_STATUS_OK)  // Upload firmware
    {
        LOG_ERR("Failed to upload firmware to VL53L5CX");
        return -EIO;
    }
    if (vl53l5cx_set_resolution(&data->st_device, VL53L5CX_RESOLUTION_4X4) != VL53L5CX_STATUS_OK)
    {
        LOG_ERR("Failed to set resolution");
        return -EIO;
    }
    if (vl53l5cx_set_ranging_frequency_hz(&data->st_device, 10) != VL53L5CX_STATUS_OK)
    {
        LOG_ERR("Failed to set ranging frequency");
        return -EIO;
    }
    LOG_INF("VL53L5CX initialized successfully");
    return 0;
}


#define VL53L5CX_DEFINE(inst)						                \
    static struct vl53l5cx_config vl53l5cx_##inst##_config = {	    \
        .i2c = I2C_DT_SPEC_INST_GET(inst),                          \
        /*.xshut = GPIO_DT_SPEC_INST_GET(inst, xshut_gpios),*/      \
    };								                                \
                                                                    \
    static struct vl53l5cx_data vl53l5cx_##inst##_driver;		    \
                                                                    \
    SENSOR_DEVICE_DT_INST_DEFINE(inst, vl53l5cx_initialize,		    \
                  NULL,		                                    \
                  &vl53l5cx_##inst##_driver,			            \
                  &vl53l5cx_##inst##_config,			            \
                  POST_KERNEL,				                        \
                  CONFIG_SENSOR_INIT_PRIORITY,			            \
                  &vl53l5cx_api_funcs);


DT_INST_FOREACH_STATUS_OKAY(VL53L5CX_DEFINE);
