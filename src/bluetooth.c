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
#include "bluetooth_protocol.h"

#include <string.h>

#define BLE_TX_DEBUG_LOG_INTERVAL_MS    5000

/* Advertising parameters for a single connectable central. */
#define ADV_PARAM BT_LE_ADV_PARAM(BT_LE_ADV_OPT_CONN | BT_LE_ADV_OPT_USE_IDENTITY, \
				  BT_GAP_ADV_FAST_INT_MIN_2, \
				  BT_GAP_ADV_FAST_INT_MAX_2, NULL)

/* Varia-compatible UUIDs exposed by this scale implementation. */
static struct bt_uuid_16 scale_service_uuid = BT_UUID_INIT_16(SCALE_SERVICE_UUID);
static struct bt_uuid_16 scale_weight_char_uuid = BT_UUID_INIT_16(SCALE_WEIGHT_CHAR_UUID);
static struct bt_uuid_16 scale_command_char_uuid = BT_UUID_INIT_16(SCALE_COMMAND_CHAR_UUID);

/* Runtime state for the active BLE session. */
static bool weight_stream_enabled;
static struct bt_conn *active_conn;

/* Cached payload for the current weight notification. */
static uint8_t weight_frame[SCALE_WEIGHT_FRAME_LEN] = {
	SCALE_SYSTEM_MESSAGE_ID,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00,
};
/* Pre-encoded Varia tare packet used for byte-exact command matching. */
static uint8_t tare_command[SCALE_TARE_PACKET_LEN];

/* GATT callback: weight streaming is enabled when the client subscribes for notifications. */
static void ccc_cfg_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
	ARG_UNUSED(attr);

	weight_stream_enabled = (value & BT_GATT_CCC_NOTIFY) != 0U;

	LOG_INF("Weight stream %s (notify)",
		weight_stream_enabled ? "enabled" : "disabled");
}

/* GATT callback: tare requests are forwarded onto zbus instead of touching the sensor directly. */
static ssize_t cmd_write_cb(struct bt_conn *conn,
				   const struct bt_gatt_attr *attr,
				   const void *buf,
				   uint16_t len,
				   uint16_t offset,
				   uint8_t flags)
{
	ARG_UNUSED(conn);
	ARG_UNUSED(attr);
	ARG_UNUSED(offset);
	ARG_UNUSED(flags);

	if (len != SCALE_TARE_PACKET_LEN) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
	}

	const uint8_t *command = buf;

	/* Varia clients send a fixed 5-byte tare packet on FFF2. */
	if (memcmp(command, tare_command, SCALE_TARE_PACKET_LEN) == 0) {
		struct button_msg msg = {
			.tare_request = true,
		};

		int err = zbus_chan_pub(&button_channel, &msg, K_NO_WAIT);
		if (err) {
			LOG_ERR("Failed to publish tare request (err %d)", err);
			return BT_GATT_ERR(BT_ATT_ERR_UNLIKELY);
		}

		LOG_INF("Received tare command");
	} else {
		LOG_WRN("Unsupported command byte 0x%02x", command[0]);
	}

	return len;
}

/* GATT service layout: one weight stream characteristic and one command characteristic. */
BT_GATT_SERVICE_DEFINE(weight_svc,
	BT_GATT_PRIMARY_SERVICE(&scale_service_uuid),
	BT_GATT_CHARACTERISTIC(&scale_weight_char_uuid.uuid,
		BT_GATT_CHRC_NOTIFY,
		BT_GATT_PERM_NONE,
		NULL, NULL, weight_frame),
	BT_GATT_CCC(ccc_cfg_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
	BT_GATT_CHARACTERISTIC(&scale_command_char_uuid.uuid,
		BT_GATT_CHRC_WRITE | BT_GATT_CHRC_WRITE_WITHOUT_RESP,
		BT_GATT_PERM_WRITE,
		NULL, cmd_write_cb, NULL)
);

/* Advertising payload: name + service UUID so the client can discover and classify this scale. */
static struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR),
	BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME, sizeof(CONFIG_BT_DEVICE_NAME) - 1),
	BT_DATA_BYTES(BT_DATA_UUID16_ALL, BT_UUID_16_ENCODE(SCALE_SERVICE_UUID))
};

static bool adv_restart_pending;

/* Advertising must be restarted after disconnect because connectable advertising stops on connect. */
static void try_start_advertising(void)
{
	int err = bt_le_adv_start(ADV_PARAM, ad, ARRAY_SIZE(ad), NULL, 0);

	if (err == -EALREADY) {
		adv_restart_pending = false;
		return;
	}

	if (err == -ENOMEM) {
		/* No free connection objects yet; retry from recycled callback. */
		adv_restart_pending = true;
		LOG_WRN("Advertising restart deferred (no free conn objects)");
		return;
	}

	if (err) {
		adv_restart_pending = true;
		LOG_ERR("Advertising failed to start (err %d)", err);
		return;
	}

	adv_restart_pending = false;
	LOG_INF("Advertising started");
}

/* Connection lifecycle callbacks keep track of the single active central connection. */
static void connected_cb(struct bt_conn *conn, uint8_t err)
{
	if (err) {
		LOG_WRN("Connection failed (err 0x%02x)", err);
		return;
	}

	if (active_conn != NULL) {
		bt_conn_unref(active_conn);
	}

	active_conn = bt_conn_ref(conn);

	LOG_INF("Central connected");
}

static void disconnected_cb(struct bt_conn *conn, uint8_t reason)
{
	if (active_conn == conn) {
		bt_conn_unref(active_conn);
		active_conn = NULL;
	}

	LOG_INF("Central disconnected (reason 0x%02x)", reason);
	adv_restart_pending = true;
	try_start_advertising();
}

/* Retry advertising when the Bluetooth stack recycles the connection object. */
static void recycled_cb(void)
{
	if (adv_restart_pending) {
		try_start_advertising();
	}
}

BT_CONN_CB_DEFINE(bt_conn_callbacks) = {
	.connected = connected_cb,
	.disconnected = disconnected_cb,
	.recycled = recycled_cb,
};

/* Bluetooth stack initialization and one-time wiring. */
static void bt_ready(int err)
{
	if (err) {
		LOG_ERR("Bluetooth init failed (err %d)", err);
		return;
	}

	LOG_INF("Bluetooth initialized");

	/* Start advertising */
	try_start_advertising();
	scale_protocol_encode_tare_command(tare_command);

	/* Now start listening for weight updates */
	zbus_chan_add_obs(&weight_channel, &bluetooth_sub, K_NO_WAIT);
}

/* Optional external antenna support kept here for later board-specific use. */
/*
#define RFSW_REGULATOR_NODE DT_NODELABEL(rfsw_ctl)
static const struct gpio_dt_spec rfsw_gpio = {
    .port = DEVICE_DT_GET(DT_GPIO_CTLR(RFSW_REGULATOR_NODE, enable_gpios)),
    .pin = DT_GPIO_PIN(RFSW_REGULATOR_NODE, enable_gpios),
    .dt_flags = DT_GPIO_FLAGS(RFSW_REGULATOR_NODE, enable_gpios),
};
*/

int bt_init(void)
{

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

	/* Initialize the Bluetooth subsystem. */
	int err = bt_enable(bt_ready);
	if (err) {
		LOG_ERR("Bluetooth init failed (err %d)", err);
		return -1;
	}

	return 0;
}

/* zbus subscriber thread: encode current weight and push it over the active BLE notify stream. */
static void subscriber_task(void)
{
	const struct zbus_channel *chan;
	uint32_t tx_ok_count = 0U;
	uint32_t tx_fail_count = 0U;
	int64_t last_tx_log_ms = 0;

	while (!zbus_sub_wait(&bluetooth_sub, &chan, K_FOREVER)) {

		if (&weight_channel == chan) {
			struct weight_msg msg;

			zbus_chan_read(&weight_channel, &msg, K_MSEC(500));

			scale_protocol_encode_weight_frame(weight_frame, msg.weight_cg);

			if (weight_stream_enabled && active_conn != NULL) {
				int err;

				err = bt_gatt_notify(active_conn, &weight_svc.attrs[2],
						     weight_frame, sizeof(weight_frame));
				if (err) {
					tx_fail_count++;
					LOG_ERR("Failed to notify weight (err %d)", err);
				} else {
					tx_ok_count++;
				}

				int64_t now_ms = k_uptime_get();
				if ((now_ms - last_tx_log_ms) >= BLE_TX_DEBUG_LOG_INTERVAL_MS) {
					LOG_DBG("BLE TX mode=notify hdr=%02x weight_cg=%u frame=[%02x %02x %02x %02x] ok=%u fail=%u",
						weight_frame[0],
						msg.weight_cg < 0 ? 0 : (uint32_t)msg.weight_cg,
						weight_frame[3],
						weight_frame[4],
						weight_frame[5],
						weight_frame[6],
						tx_ok_count,
						tx_fail_count);
					last_tx_log_ms = now_ms;
				}
			}
		}
	}
}

K_THREAD_DEFINE(subscriber_bt_id, CONFIG_MAIN_STACK_SIZE, subscriber_task, NULL, NULL, NULL, 5, 0, 0);