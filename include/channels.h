#ifndef SMART_SCALE_CHANNELS_H
#define SMART_SCALE_CHANNELS_H

#include <zephyr/kernel.h>
#include <zephyr/zbus/zbus.h>
#include <zephyr/drivers/sensor.h>


// Message Struct Definitions (What data is transferred)
struct weight_msg {
    struct sensor_value weight_g;
};

struct button_msg {
    bool tare_request;
};

// Channel Declarations (Allow other files to use the channel)
extern const struct zbus_channel weight_channel;
extern const struct zbus_channel button_channel;

// Logging
void zbus_print_channels_and_observers(void);

#endif // SMART_SCALE_CHANNELS_H