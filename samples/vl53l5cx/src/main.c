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

	int num_zones = 16; /* Default fallback */
    struct sensor_value res_val;
    if (sensor_attr_get(dev, SENSOR_CHAN_ALL, 
                        (enum sensor_attribute)SENSOR_ATTR_RESOLUTION, 
                        &res_val) == 0) {
        num_zones = res_val.val1; 
    }

	int freq_hz = 10; /* Default fallback */
	struct sensor_value freq_val;
	if (sensor_attr_get(dev, SENSOR_CHAN_ALL, 
						(enum sensor_attribute)SENSOR_ATTR_SAMPLING_FREQUENCY, 
						&freq_val) == 0) {
		freq_hz = freq_val.val1;
	}

	printf("VL53L5CX Resolution: %dx%d (%d zones)\n", 
			 num_zones == 64 ? 8 : 4, 
			 num_zones == 64 ? 8 : 4, 
			 num_zones);
	printf("VL53L5CX Sampling Frequency: %d Hz\n", freq_hz);

    while (1) {
        ret = sensor_sample_fetch(dev);
        if (ret != 0) {
            printf("Fetch failed: %d\n", ret);
            k_msleep(1000/freq_hz);
            continue;
        }

		printf("\033[H\033[J"); // Clear terminal screen
        printf("Zonal Distances (meters):\n-------------------------\n");

        for (int i = 0; i < num_zones; i++) {
            sensor_channel_get(dev, 
                               (enum sensor_channel)(SENSOR_CHAN_VL53L5CX_DISTANCE_BASE + i), 
                               &value);

            printf("[%2d]: %d.%03dm  ", i, value.val1, value.val2 / 1000);
            
            int grid_width = (num_zones == 64) ? 8 : 4;
            if ((i + 1) % grid_width == 0) printf("\n");
        }

        k_msleep(1000/freq_hz);
    }

    return 0;
}