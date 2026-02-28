#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <vl53l5cx.h>


/* Custom attribute and channel definitions (matching your driver) */
#define SENSOR_ATTR_VL53L5CX_RANGING_STATE SENSOR_ATTR_PRIV_START
#define SENSOR_CHAN_VL53L5CX_DISTANCE_BASE SENSOR_CHAN_PRIV_START

int main(void)
{
    const struct device *dev = DEVICE_DT_GET_ANY(st_vl53l5cx);
    struct sensor_value value;
    struct sensor_value enable = { .val1 = 1 };
    int ret;

    if (!device_is_ready(dev)) {
        printf("Sensor device not ready\n");
        return 0;
    }

    /* 1. Start Ranging */
    ret = sensor_attr_set(dev, SENSOR_CHAN_ALL, 
                          (enum sensor_attribute)SENSOR_ATTR_VL53L5CX_RANGING_STATE, 
                          &enable);
    if (ret != 0) {
        printf("Failed to start ranging: %d\n", ret);
        return 0;
    }

    printf("VL53L5CX Ranging Started...\n");

    while (1) {
        /* 2. Fetch data from hardware (Blocking call) */
        ret = sensor_sample_fetch(dev);
        if (ret != 0) {
            printf("Fetch failed: %d\n", ret);
            k_msleep(100);
            continue;
        }

        /* 3. Loop through 16 zones (for 4x4 resolution) */
        /* If using 8x8, change this loop to 64 */
        printf("\033[H\033[J"); // Clear terminal screen
        printf("Zonal Distances (meters):\n-------------------------\n");

        for (int i = 0; i < 64; i++) {
            sensor_channel_get(dev, 
                               (enum sensor_channel)(SENSOR_CHAN_VL53L5CX_DISTANCE_BASE + i), 
                               &value);

            printf("[%2d]: %d.%03dm  ", i, value.val1, value.val2 / 1000);
            
            if ((i + 1) % 8 == 0) printf("\n"); // Format as a 8x8 grid
        }

        k_msleep(100); // 10Hz update rate
    }

    return 0;
}