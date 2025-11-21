
#include "channels.h"
#include <zephyr/kernel.h>
#include <zephyr/zbus/zbus.h>

#define LOG_MODULE_NAME weight
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(weight, CONFIG_LOG_DEFAULT_LEVEL);

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/sys/util.h>
#include <zephyr/types.h>

#include <sensor/hx711/hx711.h>
#include <stddef.h>

// Define the thread stack and object
#define WEIGHT_THREAD_STACK_SIZE 1024
#define WEIGHT_THREAD_SLEEP_MS 1000 


// SCALE
const struct device *hx711_dev;

void weight_thread_entry(void *p1, void *p2, void *p3)
{    
    // 1. Initial setup and calibration (Tare here or during startup)
    hx711_dev = DEVICE_DT_GET_ANY(avia_hx711);
	__ASSERT(hx711_dev == NULL, "Failed to get device binding");

	LOG_INF("Device is %p, name is %s", hx711_dev, hx711_dev->name);
    
    while (1) {
        // 2. Read the Sensor
        static struct sensor_value weight;
        int ret;

        ret = sensor_sample_fetch(hx711_dev);
        if (ret != 0) {
            LOG_ERR("Cannot take measurement: %d", ret);
        } else {
            sensor_channel_get(hx711_dev, HX711_SENSOR_CHAN_WEIGHT, &weight);
        }
        
        // 3. Create the ZBUS message
        struct weight_msg msg = {
            .weight_g = weight
        };

        //LOG_INF("Publishing weight: %d.%06d grams", weight.val1, weight.val2);

        // 4. Publish the Weight
        // Publish reliably to the channel. 
        // Subscribers (Display, Broadcast) will wake up asynchronously.
        ret = zbus_chan_pub(&weight_channel, &msg, K_MSEC(500)); 
        if (ret != 0) {
            LOG_ERR("Failed to publish weight data: %d", ret);
            // Handle error, e.g., K_MSEC(500) timeout was too short
        }

        // 5. Yield/Sleep
        // Wait for the next sampling period to ensure stability and save power.
        k_msleep(WEIGHT_THREAD_SLEEP_MS);
    }
}

K_THREAD_DEFINE(weight_thread, WEIGHT_THREAD_STACK_SIZE,
                weight_thread_entry, NULL, NULL, NULL,
                3, 0, 0);