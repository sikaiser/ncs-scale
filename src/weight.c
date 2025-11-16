
#include "channels.h"
#include <zephyr/kernel.h>
#include <zephyr/zbus/zbus.h>
// ... include HX711 driver headers ...

#define LOG_MODULE_NAME weight
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(weight, CONFIG_LOG_DEFAULT_LEVEL);

// Define the thread stack and object
#define WEIGHT_THREAD_STACK_SIZE 1024
#define WEIGHT_THREAD_SLEEP_MS 1000 

void weight_thread_entry(void *p1, void *p2, void *p3)
{
    int weight_cg = 0;
    
    // 1. Initial setup and calibration (Tare here or during startup)
    
    while (1) {
        // 2. Read the Sensor
        // Replace this with your actual HX711 driver API calls
        weight_cg = weight_cg + 2130; // Dummy increment for illustration
        if (weight_cg > 50000) {
            weight_cg = 860; // Reset for demo purposes
        }
        
        // 3. Create the ZBUS message
        struct weight_msg msg = {
            .weight_cg = weight_cg
        };

        LOG_INF("Publishing weight: %d cg", msg.weight_cg);

        // 4. Publish the Weight
        // Publish reliably to the channel. 
        // Subscribers (Display, Broadcast) will wake up asynchronously.
        int ret = zbus_chan_pub(&weight_channel, &msg, K_MSEC(500)); 
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