#include <zephyr/kernel.h>

/* ZBUS*/
#include <zephyr/zbus/zbus.h>
#include "channels.h"

ZBUS_SUBSCRIBER_DEFINE(bluetooth_sub, 4);

/* LOGGING */
#include <zephyr/logging/log.h>

#define LOG_MODULE_NAME bluetooth
LOG_MODULE_REGISTER(bluetooth, CONFIG_LOG_DEFAULT_LEVEL);

/* BLUETOOTH */
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/uuid.h>

#define SERVICE_DATA_LEN        8           /* Length of service data for BTHome service */
#define SERVICE_UUID            0xfcd2      /* BTHome service UUID */
#define IDX_MASS_LOW            4           
#define IDX_MASS_HIGH           5           
#define IDX_BATTERY             7           

#define ADV_PARAM BT_LE_ADV_PARAM(BT_LE_ADV_OPT_USE_IDENTITY, \
				  BT_GAP_ADV_FAST_INT_MIN_2, \
				  BT_GAP_ADV_FAST_INT_MAX_2, NULL)


static uint8_t service_data[SERVICE_DATA_LEN] = {
	BT_UUID_16_ENCODE(SERVICE_UUID),
	0x40,
	0x06,	/* Object ID Mass (kg) */
	0x00,	/* Low byte */
	0x00,   /* High byte */
	0x01,   /* Object ID Battery level */
	0x00,   /* Single byte */
};

static struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR), // Flags
	BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME, sizeof(CONFIG_BT_DEVICE_NAME) - 1), // Device Name
	BT_DATA(BT_DATA_SVC_DATA16, service_data, ARRAY_SIZE(service_data)) // BTHome Data
};

static void bt_ready(int err)
{
	if (err) {
		LOG_ERR("Bluetooth init failed (err %d)", err);
		return;
	}

	LOG_INF("Bluetooth initialized\n");

	/* Start advertising */
	err = bt_le_adv_start(ADV_PARAM, ad, ARRAY_SIZE(ad), NULL, 0);
	if (err) {
		LOG_ERR("Advertising failed to start (err %d)", err);
		return;
	}

	/* Now start listening for weight updates */
	zbus_chan_add_obs(&weight_channel, &bluetooth_sub, K_NO_WAIT);
}

// for switch to external antenna
/*
#define RFSW_REGULATOR_NODE DT_NODELABEL(rfsw_ctl)
static const struct gpio_dt_spec rfsw_gpio = {
    .port = DEVICE_DT_GET(DT_GPIO_CTLR(RFSW_REGULATOR_NODE, enable_gpios)),
    .pin = DT_GPIO_PIN(RFSW_REGULATOR_NODE, enable_gpios),
    .dt_flags = DT_GPIO_FLAGS(RFSW_REGULATOR_NODE, enable_gpios),
};
*/

int bt_init(void) {

    /*
    // Configure antenna switch
    if (!gpio_is_ready_dt(&rfsw_gpio)) {
        LOG_ERR("RF switch GPIO not ready");
    } else {
        ret = gpio_pin_configure_dt(&rfsw_gpio, GPIO_OUTPUT_INACTIVE);
        if (ret == 0) {
            // Set to 0 for external antenna (physical HIGH due to active-low)
            gpio_pin_set_dt(&rfsw_gpio, 0);
            LOG_INF("External antenna enabled");
        }
    }
    */

    // Initialize the Bluetooth Subsystem
	int err = bt_enable(bt_ready);
	if (err) {
		LOG_ERR("Bluetooth init failed (err %d)", err);
		return -1;
	}
    return 0;
}

/* ZBUS SUBSCRIBER*/
static void subscriber_task(void)
{
	const struct zbus_channel *chan;

	while (!zbus_sub_wait(&bluetooth_sub, &chan, K_FOREVER)) {

		if (&weight_channel == chan) {
			struct weight_msg msg;

			zbus_chan_read(&weight_channel, &msg, K_MSEC(500));

			LOG_INF("From bluetooth subscriber -> Weight= %d.%06d grams", msg.weight_g.val1, msg.weight_g.val2);

			// Bthome protocol doesn't support negative values for mass.
			// If interactive taring is introduced in the future, it might make sense to switch to a data type that supports negative values.

			// Convert to decigrams for BTHome
			// 1. Convert val1 (grams) to tenths of a gram: val1 * 10
			int32_t scaled_val1 = msg.weight_g.val1 * 10;

			// 2. Convert val2 (micro-units, 10^-6 g) to tenths of a gram (10^-1 g):
			//    (10^-6) / (10^-1) = 10^-5. Divide val2 by 100,000.
			//    (This captures the first decimal place, val2 / 100000)
			int32_t scaled_val2 = msg.weight_g.val2 / 100000;

			// 3. Combine to get the final scaled 16-bit integer
			int16_t decigrams = (int16_t)(scaled_val1 + scaled_val2);

			// Split into high and low bytes
			service_data[IDX_MASS_HIGH] = (decigrams >> 8) & 0xFF; // High byte
			service_data[IDX_MASS_LOW] = decigrams & 0xFF;         // Low byte
			service_data[IDX_BATTERY] = (uint8_t)67;
			for (int i = 0; i < 1; i++) {
				int err = bt_le_adv_update_data(ad, ARRAY_SIZE(ad), NULL, 0);
				if (err) {
					LOG_ERR("Failed to update advertising data (err %d)\n", err);
				}
			}
		}
	}
}

K_THREAD_DEFINE(subscriber_bt_id, CONFIG_MAIN_STACK_SIZE, subscriber_task, NULL, NULL, NULL, 5, 0, 0);