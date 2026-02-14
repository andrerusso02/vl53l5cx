#define DT_DRV_COMPAT st_vl53l5cx

#include "vl53l5cx.h"

static int vl53l5cx_init(const struct device *dev)
{
    printk("Initializing vl53l5cx\n");
    return 0;
}


#define VL53L5CX_DEFINE(inst)						 \
    static struct vl53l5cx_config vl53l5cx_##inst##_config = {	 \
    };								 \
                                     \
    static struct vl53l5cx_data vl53l5cx_##inst##_driver;		 \
                                     \
    SENSOR_DEVICE_DT_INST_DEFINE(inst, vl53l5cx_init,		 \
                  NULL,		 \
                  &vl53l5cx_##inst##_driver,			 \
                  &vl53l5cx_##inst##_config,			 \
                  POST_KERNEL,				 \
                  CONFIG_SENSOR_INIT_PRIORITY,		 \
                  &vl53l5cx_api_funcs);


DT_INST_FOREACH_STATUS_OKAY(VL53L5CX_DEFINE);
