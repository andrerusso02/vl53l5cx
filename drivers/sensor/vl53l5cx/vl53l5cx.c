#define DT_DRV_COMPAT st_vl53l5cx

#include <errno.h>
#include <zephyr/logging/log.h>

#include "vl53l5cx.h"

#define VL53L5CX_INITIAL_ADDRESS  0x29

LOG_MODULE_REGISTER(VL53L5CX, CONFIG_SENSOR_LOG_LEVEL);

static int vl53l5cx_initialize(const struct device *dev)
{
    const struct vl53l5cx_config *config = dev->config;
    struct vl53l5cx_data *data = dev->data; 

    data->st_device.platform.i2c = &config->i2c;
    data->st_device.platform.address = VL53L5CX_INITIAL_ADDRESS;

    if (!device_is_ready(config->i2c.bus)) {
        printk("I2C device not ready\n");
        return -ENODEV;
    }

    /* If lpn_gpio is defined, wake the sensor's I2C interface */
    if (config->lpn_gpio.port != NULL) {
        gpio_pin_configure_dt(&config->lpn_gpio, GPIO_OUTPUT_ACTIVE);
        k_msleep(5);
    }

#ifdef CONFIG_VL53L5CX_RECONFIGURE_ADDRESS
    /* Early SYS_INIT vl53l5cx_silence_all_sensors disabled all sensors.*/
    if (config->lpn_gpio.port == NULL) {
        LOG_ERR("[%s] lpn-gpios required when CONFIG_VL53L5CX_RECONFIGURE_ADDRESS is enabled", dev->name);
        return -ENODEV;
    }

    /* Required delay for the VL53L5CX to boot and attach to the I2C bus */
    k_msleep(1000);  // TODO check if needed

    if (config->i2c.addr != VL53L5CX_INITIAL_ADDRESS) {
        uint8_t status = vl53l5cx_set_i2c_address(&data->st_device, config->i2c.addr);
        if (status != VL53L5CX_STATUS_OK) {
            LOG_ERR("[%s] Failed to set I2C address to %d", dev->name, config->i2c.addr);
            return -EIO;
        }
    }
#else
    if (config->i2c.addr != VL53L5CX_INITIAL_ADDRESS) {
        LOG_ERR("[%s] DT address must be %d unless CONFIG_VL53L5CX_RECONFIGURE_ADDRESS is enabled", dev->name, VL53L5CX_INITIAL_ADDRESS);
        return -EINVAL;
    }
#endif


    if (vl53l5cx_init(&data->st_device) != VL53L5CX_STATUS_OK)  // Upload firmware
    {
        LOG_ERR("Failed to upload firmware to VL53L5CX");
        return -EIO;
    }
    
    data->resolution = VL53L5CX_RESOLUTION_4X4;
#ifdef CONFIG_VL53L5CX_RESOLUTION_8X8
    data-> resolution = VL53L5CX_RESOLUTION_8X8;
#endif

    if (vl53l5cx_set_resolution(&data->st_device, data->resolution) != VL53L5CX_STATUS_OK) {
        LOG_ERR("Failed to set resolution");
        return -EIO;
    }

    if (vl53l5cx_set_ranging_frequency_hz(&data->st_device, CONFIG_VL53L5CX_RANGING_FREQ) != VL53L5CX_STATUS_OK) {
        LOG_ERR("Failed to set frequency");
        return -EIO;
    }

    LOG_INF("VL53L5CX initialized: %s @ %dHz", 
            data->resolution == VL53L5CX_RESOLUTION_4X4 ? "4x4" : "8x8",
            CONFIG_VL53L5CX_RANGING_FREQ);
    
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


static int vl53l5cx_attr_get(const struct device *dev,
                             enum sensor_channel chan,
                             enum sensor_attribute attr,
                             struct sensor_value *val)
{
    struct vl53l5cx_data *data = dev->data;

    if (chan != SENSOR_CHAN_ALL) {
        return -ENOTSUP;
    }
    if (attr == SENSOR_ATTR_RESOLUTION) {
        val->val1 = data->resolution; 
        val->val2 = 0;
        return 0;
    }
    if (attr == SENSOR_ATTR_SAMPLING_FREQUENCY) {
        val->val1 = CONFIG_VL53L5CX_RANGING_FREQ;
        val->val2 = 0;
        return 0;
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
    .attr_get = vl53l5cx_attr_get,
    .sample_fetch = vl53l5cx_sample_fetch,
    .channel_get = vl53l5cx_channel_get,
};

#define VL53L5CX_DEFINE(inst)						                \
    static struct vl53l5cx_config vl53l5cx_##inst##_config = {	    \
        .i2c = I2C_DT_SPEC_INST_GET(inst),                          \
        .lpn_gpio = GPIO_DT_SPEC_INST_GET_OR(inst, lpn_gpios, {0}), \
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


#ifdef CONFIG_VL53L5CX_RECONFIGURE_ADDRESS

static int vl53l5cx_silence_all_sensors(void)
{
#define SILENCE_LPN(inst)                                                       \
    do {                                                                        \
        const struct gpio_dt_spec lpn_##inst =                                  \
            GPIO_DT_SPEC_INST_GET_OR(inst, lpn_gpios, {0});                     \
        if (lpn_##inst.port != NULL) {                                          \
            gpio_pin_configure_dt(&lpn_##inst, GPIO_OUTPUT_INACTIVE);           \
        }                                                                       \
    } while (0);

    DT_INST_FOREACH_STATUS_OKAY(SILENCE_LPN)
    
    return 0;
}

/* Run before the individual sensor drivers initialize */
SYS_INIT(vl53l5cx_silence_all_sensors, POST_KERNEL, 10);

#endif /* CONFIG_VL53L5CX_RECONFIGURE_ADDRESS */