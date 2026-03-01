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
    struct sensor_value enable = { .val1 = 1 };
    struct sensor_value result_value;
	int ret;

    if (!device_is_ready(dev)) {
        printf("Sensor device not ready\n");
        return 0;
    }

    ret = sensor_attr_set(dev, SENSOR_CHAN_ALL, (enum sensor_attribute)SENSOR_ATTR_VL53L5CX_RANGING_STATE, &enable);
    if (ret != 0) {
        printf("Failed to start ranging: %d\n", ret);
        return 0;
    }

    printf("VL53L5CX Ranging Started...\n");

    result_value = (struct sensor_value){0};
	ret =sensor_attr_get(dev, SENSOR_CHAN_ALL, SENSOR_ATTR_RESOLUTION, &result_value);
    if (ret != 0) {
        printf("Failed to get resolution: %d\n", ret);
		return 0;
    }
	int num_zones = result_value.val1;

	result_value = (struct sensor_value){0};
	ret = sensor_attr_get(dev, SENSOR_CHAN_ALL, SENSOR_ATTR_SAMPLING_FREQUENCY, &result_value);
	if (ret != 0) {
		printf("Failed to get sampling frequency: %d\n", ret);
		return 0;
	}
	int freq_hz = result_value.val1;

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
            sensor_channel_get(dev, (enum sensor_channel)(SENSOR_CHAN_VL53L5CX_DISTANCE_BASE + i), &result_value);

            printf("[%2d]: %d.%03dm  ", i, result_value.val1, result_value.val2 / 1000);
            
            int grid_width = (num_zones == 64) ? 8 : 4;
            if ((i + 1) % grid_width == 0) printf("\n");
        }

        k_msleep(1000/freq_hz);
    }

    return 0;
}