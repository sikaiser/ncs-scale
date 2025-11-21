
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
#define WEIGHT_THREAD_SLEEP_MS 100 


// SCALE
const struct device *hx711_dev;

void weight_thread_entry(void *p1, void *p2, void *p3)
{    
    // 1. Initial setup and calibration (Tare here or during startup)
    hx711_dev = DEVICE_DT_GET_ANY(avia_hx711);
	__ASSERT(hx711_dev == NULL, "Failed to get device binding");

	LOG_INF("Device is %p, name is %s", hx711_dev, hx711_dev->name);

    // Let the HX711 settle? Seems to be necessary for successful tare
	//k_msleep(1000);

	// Tare
	int offset;
	offset = avia_hx711_tare(hx711_dev, 10);
        
	LOG_INF("Tare offset: %d\n", offset);

	// Calibrate and identify slope using known weight
	//calibrateWKnownWeight(337); // 337g = 0.000920

    // Set slope using known value (from above calibration)
	const struct sensor_value slope = { .val1 = 0, .val2 = 920 };
	struct hx711_data *data = hx711_dev->data;
	data->slope.val1 = slope.val1;
	data->slope.val2 = slope.val2;
    
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


void calibrateWKnownWeight(int calibration_weight) {
	printk("Calibrating...\n");
	printk("Place known weight of %d on scale...\n",
		calibration_weight);

	for (int i = 5; i >= 0; i--) {
		printk(" %d..", i);
		k_msleep(1000);
	}

	printk("\nNow assuming that the weight was placed and calculating slope...\n");
	struct sensor_value slope;
	slope = avia_hx711_calibrate(hx711_dev, calibration_weight, 5);
	printk("Slope set to : %d.%06d\n", slope.val1, slope.val2);
}