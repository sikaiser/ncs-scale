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
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/uuid.h>

#define WSS_SERVICE_UUID        0x181D
#define WEIGHT_CHAR_UUID        0x2A9D

#define ADV_PARAM BT_LE_ADV_PARAM(BT_LE_ADV_OPT_CONN | BT_LE_ADV_OPT_USE_IDENTITY, \
				  BT_GAP_ADV_FAST_INT_MIN_2, \
				  BT_GAP_ADV_FAST_INT_MAX_2, NULL)

static struct bt_uuid_16 wss_service_uuid = BT_UUID_INIT_16(WSS_SERVICE_UUID);
static struct bt_uuid_16 weight_char_uuid = BT_UUID_INIT_16(WEIGHT_CHAR_UUID);
static bool notify_enabled;
static int16_t weight_deci_g;

static void ccc_cfg_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
	notify_enabled = (value == BT_GATT_CCC_NOTIFY);
	LOG_INF("Weight notifications %s", notify_enabled ? "enabled" : "disabled");
}

BT_GATT_SERVICE_DEFINE(weight_svc,
	BT_GATT_PRIMARY_SERVICE(&wss_service_uuid),
	BT_GATT_CHARACTERISTIC(&weight_char_uuid.uuid,
		BT_GATT_CHRC_NOTIFY,
		BT_GATT_PERM_NONE,
		NULL, NULL, &weight_deci_g),
	BT_GATT_CCC(ccc_cfg_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE)
);

static struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR),
	BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME, sizeof(CONFIG_BT_DEVICE_NAME) - 1),
	BT_DATA_BYTES(BT_DATA_UUID16_ALL, BT_UUID_16_ENCODE(WSS_SERVICE_UUID))
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

			//LOG_INF("From bluetooth subscriber -> Weight= %d.%06d grams", msg.weight_g.val1, msg.weight_g.val2);

			/* Convert grams (sensor_value) to deci-grams for a compact 16-bit payload. */
			int32_t scaled_val1 = msg.weight_g.val1 * 10;
			int32_t scaled_val2 = msg.weight_g.val2 / 100000;

			weight_deci_g = (int16_t)(scaled_val1 + scaled_val2);

			if (notify_enabled) {
				int err = bt_gatt_notify(NULL, &weight_svc.attrs[2], &weight_deci_g,
							 sizeof(weight_deci_g));
				if (err) {
					LOG_ERR("Failed to notify weight (err %d)", err);
				}
			}
		}
	}
}

K_THREAD_DEFINE(subscriber_bt_id, CONFIG_MAIN_STACK_SIZE, subscriber_task, NULL, NULL, NULL, 5, 0, 0);