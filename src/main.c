#include <zephyr/kernel.h>

#include "channels.h"

#define LOG_MODULE_NAME main
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(main, CONFIG_LOG_DEFAULT_LEVEL);

int main(void)
{
        zbus_print_channels_and_observers();

        while (1) {
                LOG_INF("Main thread heartbeat");
                k_msleep(2000); 
        }

	return 0;
}
