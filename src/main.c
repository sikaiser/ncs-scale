#include <zephyr/kernel.h>

#include "channels.h"
#include "bluetooth.h"
#include "display.h"

#define LOG_MODULE_NAME main
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(main, CONFIG_LOG_DEFAULT_LEVEL);

int main(void)
{
        zbus_print_channels_and_observers();
        bt_init();
        display_init();

        while (1) {
                log_uptime();
                k_msleep(10000); 
        }

	return 0;
}

void log_uptime(void) {
        int64_t uptime_ms = k_uptime_get();

        // Convert milliseconds to seconds, minutes, and hours
        uint32_t seconds = uptime_ms / 1000U;
        uint32_t minutes = seconds / 60U;
        uint32_t hours = minutes / 60U;
        
        // Format the log message as HH:MM:SS
        LOG_INF("Uptime: %02d:%02d:%02d", 
                hours, 
                minutes % 60U, // Minutes remainder
                seconds % 60U  // Seconds remainder
        );
}