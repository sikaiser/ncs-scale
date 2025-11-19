#include <zephyr/kernel.h>

/* ZBUS*/
#include <zephyr/zbus/zbus.h>
#include "channels.h"

ZBUS_SUBSCRIBER_DEFINE(display_sub, 4);

/* LOGGING */
#include <zephyr/logging/log.h>

#define LOG_MODULE_NAME display
LOG_MODULE_REGISTER(display, CONFIG_LOG_DEFAULT_LEVEL);

/* DISPLAY*/
#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <zephyr/display/cfb.h>

static const struct device *display;

// 8x8 icon definitions (1 = white pixel)
static const uint8_t icon_bluetooth[] = {
    0b00010000,
    0b00110000,
    0b01010100,
    0b00111000,
    0b00111000,
    0b01010100,
    0b00110000,
    0b00010000,
};

static const uint8_t icon_wifi[] = {
    0b00000000,
    0b01111110,
    0b10000001,
    0b00111100,
    0b01000010,
    0b00011000,
    0b00100100,
    0b00000000,
};

static const uint8_t icon_battery_full[] = {
    0b00111111,
    0b00100001,
    0b01111101,
    0b01111101,
    0b01111101,
    0b01111101,
    0b00100001,
    0b00111111,
};

static const uint8_t icon_battery_low[] = {
    0b00111111,
    0b00100001,
    0b01100001,
    0b01100001,
    0b01100001,
    0b01100001,
    0b00100001,
    0b00111111,
};

void draw_icon(const struct device *display, const uint8_t *icon, uint16_t x_start, uint16_t y_start)
{
	// Iterate over the 8 rows (y-axis)
	for (int y = 0; y < 8; y++) {
		// The icon data is one byte per row
		uint8_t row_data = icon[y];

		// Iterate over the 8 columns (x-axis)
		for (int x = 0; x < 8; x++) {
			// Check if the pixel bit is set (from MSB to LSB: bit 7 to bit 0)
			if (row_data & (0b10000000 >> x)) {
				// Draw a point at the calculated screen coordinates
				struct cfb_position pos;
				pos.x = x_start + x;
				pos.y = y_start + y;
				cfb_draw_point(display, &pos);
			}
		}
	}
}

void display_weight(const struct device *display, float weight_g, 
                   bool bt_connected, bool wifi_connected, uint8_t battery_pct)
{
    char weight_str[16];
    
    // Clear display
    cfb_framebuffer_clear(display, false);
    
    // Display weight in large text (center of screen)
    snprintf(weight_str, sizeof(weight_str), "%5.1f", weight_g);
    cfb_print(display, weight_str, 0, 0);
    
    // Draw status icons at top right
    uint16_t icon_x = 128 - 8;  // Start from right edge
    
    // Battery icon
    if (battery_pct > 20) {
        draw_icon(display, icon_battery_full, icon_x, 0);
    } else {
        draw_icon(display, icon_battery_low, icon_x, 0);
    }
    icon_x -= 12;  // Space between icons
    
    // WiFi icon
    if (wifi_connected) {
        draw_icon(display, icon_wifi, icon_x, 0);
        icon_x -= 12;
    }
    
    // Bluetooth icon
    if (bt_connected) {
        draw_icon(display, icon_bluetooth, icon_x, 0);
    }
    
    cfb_framebuffer_finalize(display);
}


int display_init(void) {

	display = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
    
    if (!device_is_ready(display)) {
        LOG_ERR("Display not ready");
        return 0;
    }
    
    display_blanking_off(display);
    
    // Initialize CFB
    if (cfb_framebuffer_init(display)) {
        LOG_ERR("CFB init failed");
        return 0;
    }
    
    cfb_framebuffer_set_font(display, 2); // Use font index 2 (larger font)
	cfb_framebuffer_invert(display);

	/* Now start listening for weight updates */
	zbus_chan_add_obs(&weight_channel, &display_sub, K_NO_WAIT);

    return 0;
}

/* ZBUS SUBSCRIBER*/
static void subscriber_task(void)
{
	const struct zbus_channel *chan;

	while (!zbus_sub_wait(&display_sub, &chan, K_FOREVER)) {

		if (&weight_channel == chan) {
			struct weight_msg weight_data;

			zbus_chan_read(&weight_channel, &weight_data, K_MSEC(500));

			int weight_cg = weight_data.weight_cg;

			LOG_INF("From display subscriber -> Weight=%d cg", weight_cg);

			float weight_g = (float)weight_cg / 100.0f;
			bool bt_conn = true;
			bool wifi_conn = false;
			uint8_t batt = 85;

			display_weight(display, weight_g, bt_conn, wifi_conn, batt);

		}
	}
}

K_THREAD_DEFINE(subscriber_display_id, CONFIG_MAIN_STACK_SIZE, subscriber_task, NULL, NULL, NULL, 5, 0, 0);