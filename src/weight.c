
#include "channels.h"
#include <zephyr/kernel.h>
#include <zephyr/zbus/zbus.h>

ZBUS_SUBSCRIBER_DEFINE(weight_cmd_sub, 4);

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
#define WEIGHT_DEBUG_LOG_INTERVAL_MS 5000
#define HX711_COUNT 2


// SCALE
static const struct device *hx711_devices[HX711_COUNT];

static int32_t weight_sensor_value_to_centi_grams(const struct sensor_value *weight)
{
    if (weight == NULL) {
        return 0;
    }

    return (weight->val1 * 100) + (weight->val2 / 10000);
}

void weight_thread_entry(void *p1, void *p2, void *p3)
{    
    // 1. Initial setup and calibration (Tare here or during startup)
    hx711_devices[0] = DEVICE_DT_GET(DT_NODELABEL(hx711_drain));
    hx711_devices[1] = DEVICE_DT_GET(DT_NODELABEL(hx711_steam));

    for (int i = 0; i < HX711_COUNT; i++) {
        __ASSERT(hx711_devices[i] != NULL, "Failed to get HX711 device");
        if (!device_is_ready(hx711_devices[i])) {
            LOG_ERR("HX711 device %d is not ready", i);
            return;
        }
        LOG_INF("HX711 %d is %p, name is %s", i, hx711_devices[i], hx711_devices[i]->name);
    }

    // Let the HX711 settle? Seems to be necessary for successful tare
	k_msleep(100);

	for (int i = 0; i < HX711_COUNT; i++) {
		int offset = avia_hx711_tare(hx711_devices[i], 10);
		LOG_INF("HX711 %d tare offset: %d", i, offset);
	}

    zbus_chan_add_obs(&button_channel, &weight_cmd_sub, K_NO_WAIT);

	// Calibrate and identify slope using known weight
    //calibrateWKnownWeight(337, 0); // Calibrate HX711 0 (left) with 337g

    // Set slope using known value (from above calibration)
    const struct sensor_value slope = { .val1 = 0, .val2 = 920 };
	for (int i = 0; i < HX711_COUNT; i++) {
		int slope_ret = sensor_attr_set(hx711_devices[i],
						HX711_SENSOR_CHAN_WEIGHT,
						(enum sensor_attribute)HX711_SENSOR_ATTR_SLOPE,
						&slope);
		if (slope_ret != 0) {
			LOG_ERR("Failed to set HX711 %d slope: %d", i, slope_ret);
		}
    }
    
    while (1) {
        static int64_t last_weight_log_ms;
        const struct zbus_channel *chan;

        while (!zbus_sub_wait(&weight_cmd_sub, &chan, K_NO_WAIT)) {
            if (&button_channel == chan) {
                struct button_msg btn_msg;
                int rc = zbus_chan_read(&button_channel, &btn_msg, K_NO_WAIT);
                if (rc == 0 && btn_msg.tare_request) {
					for (int i = 0; i < HX711_COUNT; i++) {
						int tare_rc = avia_hx711_tare(hx711_devices[i], 10);
						LOG_INF("BLE tare for HX711 %d, new offset: %d", i, tare_rc);
					}
                }
            }
        }

        // 2. Read the Sensor
        int32_t total_weight_cg = 0;
		int ret = 0;
		bool measurement_valid = true;

		for (int i = 0; i < HX711_COUNT; i++) {
			struct sensor_value weight;
			ret = sensor_sample_fetch(hx711_devices[i]);
			if (ret == 0) {
				ret = sensor_channel_get(hx711_devices[i], HX711_SENSOR_CHAN_WEIGHT, &weight);
			}
			if (ret != 0) {
				LOG_ERR("Cannot read HX711 %d: %d", i, ret);
				measurement_valid = false;
				break;
			}
			total_weight_cg += weight_sensor_value_to_centi_grams(&weight);
		}

		if (measurement_valid) {

            int64_t now_ms = k_uptime_get();
            if ((now_ms - last_weight_log_ms) >= WEIGHT_DEBUG_LOG_INTERVAL_MS) {
				LOG_DBG("Weight sample: %d.%02d g", total_weight_cg / 100, abs(total_weight_cg % 100));
                last_weight_log_ms = now_ms;
            }

            // 3. Create the ZBUS message
            struct weight_msg msg = {
                .weight_cg = total_weight_cg
            };

            // 4. Publish the Weight
            ret = zbus_chan_pub(&weight_channel, &msg, K_MSEC(500));
            if (ret != 0) {
                LOG_ERR("Failed to publish weight data: %d", ret);
            }

        }

        // 5. Yield/Sleep
        k_msleep(WEIGHT_THREAD_SLEEP_MS);
    }
}

K_THREAD_DEFINE(weight_thread, WEIGHT_THREAD_STACK_SIZE,
                weight_thread_entry, NULL, NULL, NULL,
                3, 0, 0);


void calibrateWKnownWeight(int calibration_weight, int hx711_index) {
    if (hx711_index < 0 || hx711_index >= HX711_COUNT) {
        LOG_ERR("Invalid HX711 index for calibration: %d", hx711_index);
        return;
    }

	printk("Calibrating...\n");
    printk("Place known weight of %d on HX711 %d...\n",
        calibration_weight, hx711_index);

	for (int i = 5; i >= 0; i--) {
		printk(" %d..", i);
		k_msleep(1000);
	}

	printk("\nNow assuming that the weight was placed and calculating slope...\n");
	struct sensor_value slope = avia_hx711_calibrate(hx711_devices[hx711_index],
							 calibration_weight, 5);
	printk("HX711 %d slope set to : %d.%06d\n",
	       hx711_index, slope.val1, slope.val2);
}