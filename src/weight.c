
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
    int32_t current_weight;
    
    // 1. Initial setup and calibration (Tare here or during startup)
    current_weight = 0;
    
    while (1) {
        // 2. Read the Sensor
        // Replace this with your actual HX711 driver API calls
        current_weight = current_weight + 21300; // Dummy increment for illustration
        if (current_weight > 500000) {
            current_weight = 8600; // Reset for demo purposes
        }
        
        // 3. Create the ZBUS message
        struct weight_msg msg = {
            .weight_mg = current_weight
        };

        LOG_INF("Publishing weight: %d mg", msg.weight_mg);

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