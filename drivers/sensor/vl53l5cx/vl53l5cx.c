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
    if (vl53l5cx_set_resolution(&data->st_device, VL53L5CX_RESOLUTION_8X8) != VL53L5CX_STATUS_OK)
    {
        LOG_ERR("Failed to set resolution");
        return -EIO;
    }
    if (vl53l5cx_set_ranging_frequency_hz(&data->st_device, 10) != VL53L5CX_STATUS_OK)
    {
        LOG_ERR("Failed to set ranging frequency");
        return -EIO;
    }

    // Cache resolution for channel_get validation
    data->resolution = VL53L5CX_RESOLUTION_8X8;  // TODO

    LOG_INF("VL53L5CX initialized successfully");
    return 0;
}


static int vl53l5cx_attr_set(const struct device *dev,
                             enum sensor_channel chan,
                             enum sensor_attribute attr,
                             const struct sensor_value *val)
{
struct vl53l5cx_data *data = dev->data;
    int status;

    if (chan != SENSOR_CHAN_ALL) {
        return -ENOTSUP;
    }

    if ((int)attr == SENSOR_ATTR_VL53L5CX_RANGING_STATE) {
        if (val->val1 == 1) {
            /* Start Ranging */
            if (data->is_ranging) {
                return 0; /* Already ranging */
            }
            
            status = vl53l5cx_start_ranging(&data->st_device);
            if (status != VL53L5CX_STATUS_OK) {
                LOG_ERR("Failed to start ranging");
                return -EIO;
            }
            data->is_ranging = true;
            return 0;

        } else if (val->val1 == 0) {
            /* Stop Ranging */
            if (!data->is_ranging) {
                return 0; /* Already stopped */
            }

            status = vl53l5cx_stop_ranging(&data->st_device);
            if (status != VL53L5CX_STATUS_OK) {
                LOG_ERR("Failed to stop ranging");
                return -EIO;
            }
            data->is_ranging = false;
            return 0;
        } else {
            return -EINVAL; /* Invalid state requested */
        }
    }

    return -ENOTSUP;
}


static int vl53l5cx_sample_fetch(const struct device *dev, enum sensor_channel chan)
{
    struct vl53l5cx_data *data = dev->data;
    uint8_t is_ready = 0;
    int32_t status;
    int timeout = 50; // 50 * 10ms = 500ms timeout

    /* 1. Wait for data to be ready (Polling) */
    do {
        status = vl53l5cx_check_data_ready(&data->st_device, &is_ready);
        if (status != VL53L5CX_STATUS_OK) {
            return -EIO;
        }
        if (is_ready) {
            break;
        }
        k_msleep(10); 
    } while (--timeout > 0);

    if (timeout == 0) {
        return -ETIMEDOUT;
    }

    /* 2. Fetch all ranging data into the driver buffer */
    status = vl53l5cx_get_ranging_data(&data->st_device, &data->last_results);
    if (status != VL53L5CX_STATUS_OK) {
        return -EIO;
    }

    /* Update internal resolution cache for channel_get validation */
    vl53l5cx_get_resolution(&data->st_device, &data->resolution);

    return 0;
}


static int vl53l5cx_channel_get(const struct device *dev,
                                enum sensor_channel chan,
                                struct sensor_value *val)
{
    struct vl53l5cx_data *data = dev->data;
    uint32_t zone_idx = (uint32_t)chan - SENSOR_CHAN_PRIV_START;

    /* 1. Validate if the requested channel is a valid zone */
    if (zone_idx >= data->resolution) {
        return -ENOTSUP;
    }

    /* 2. Map Target Status (5 and 9 are "Valid" for VL53L5CX) */
    uint8_t raw_status = data->last_results.target_status[VL53L5CX_NB_TARGET_PER_ZONE * zone_idx];
    if (raw_status != 5 && raw_status != 9) {
        val->val1 = 0;
        val->val2 = 0;
        return 0; // Return 0m for invalid targets
    }

    /* 3. Convert Distance (mm) to Zephyr Standard (meters) */
    /* val1 = whole meters, val2 = microseconds of a meter (mm * 1000) */
    int32_t distance_mm = data->last_results.distance_mm[VL53L5CX_NB_TARGET_PER_ZONE * zone_idx];
    val->val1 = distance_mm / 1000;
    val->val2 = (distance_mm % 1000) * 1000;

    return 0;
}

static const struct sensor_driver_api vl53l5cx_api_funcs = {
    .attr_set = vl53l5cx_attr_set,
    .sample_fetch = vl53l5cx_sample_fetch,
    .channel_get = vl53l5cx_channel_get,
};

#define VL53L5CX_DEFINE(inst)						                \
    static struct vl53l5cx_config vl53l5cx_##inst##_config = {	    \
        .i2c = I2C_DT_SPEC_INST_GET(inst),                          \
        /*.xshut = GPIO_DT_SPEC_INST_GET(inst, xshut_gpios),*/      \
    };								                                \
                                                                    \
    static struct vl53l5cx_data vl53l5cx_##inst##_driver;		    \
                                                                    \
    SENSOR_DEVICE_DT_INST_DEFINE(inst, vl53l5cx_initialize,		    \
                  NULL,		                                        \
                  &vl53l5cx_##inst##_driver,			            \
                  &vl53l5cx_##inst##_config,			            \
                  POST_KERNEL,				                        \
                  CONFIG_SENSOR_INIT_PRIORITY,			            \
                  &vl53l5cx_api_funcs);


DT_INST_FOREACH_STATUS_OKAY(VL53L5CX_DEFINE);
