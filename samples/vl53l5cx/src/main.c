#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>


int main(void)
{
	printk("Hello World!\n");
	const struct device *const dev = DEVICE_DT_GET_ANY(st_vl53l5cx);
}
