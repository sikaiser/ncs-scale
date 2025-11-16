#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/zbus/zbus.h>

#include "channels.h"

#define LOG_MODULE_NAME bluetooth
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(bluetooth, CONFIG_LOG_DEFAULT_LEVEL);

ZBUS_SUBSCRIBER_DEFINE(bluetooth_sub, 4);

static void subscriber_task(void)
{
	const struct zbus_channel *chan;

	while (!zbus_sub_wait(&bluetooth_sub, &chan, K_FOREVER)) {
		struct weight_msg weight_data;

		if (&weight_channel == chan) {
			zbus_chan_read(&weight_channel, &weight_data, K_MSEC(500));

			LOG_INF("From subscriber -> Weight=%d mg", weight_data.weight_mg);
		}
	}
}

K_THREAD_DEFINE(subscriber_task_id, CONFIG_MAIN_STACK_SIZE, subscriber_task, NULL, NULL, NULL, 5, 0, 0);