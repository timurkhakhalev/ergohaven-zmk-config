#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>

#include <zmk/keymap.h>
#include <zmk/ble.h>
#include <zmk/event_manager.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/events/ble_active_profile_changed.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

/* UUIDs matching zmk-battery-center app */
#define ZMK_STATE_SERVICE_UUID_VAL \
    BT_UUID_128_ENCODE(0x1D442E49, 0x9D0E, 0x4C0C, 0x8E0E, 0x8B73B7E42C2A)
#define ZMK_STATE_LAYER_UUID_VAL \
    BT_UUID_128_ENCODE(0x1D442E49, 0x9D0E, 0x4C0C, 0x8E0E, 0x8B73B7E42C2B)
#define ZMK_STATE_PROFILE_UUID_VAL \
    BT_UUID_128_ENCODE(0x1D442E49, 0x9D0E, 0x4C0C, 0x8E0E, 0x8B73B7E42C2C)

#define ZMK_STATE_SERVICE_UUID  BT_UUID_DECLARE_128(ZMK_STATE_SERVICE_UUID_VAL)
#define ZMK_STATE_LAYER_UUID    BT_UUID_DECLARE_128(ZMK_STATE_LAYER_UUID_VAL)
#define ZMK_STATE_PROFILE_UUID  BT_UUID_DECLARE_128(ZMK_STATE_PROFILE_UUID_VAL)

static uint8_t current_layer = 0;
static uint8_t current_profile = 0;
static bool layer_notify_enabled = false;
static bool profile_notify_enabled = false;

static ssize_t read_layer(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                          void *buf, uint16_t len, uint16_t offset)
{
    current_layer = zmk_keymap_highest_layer_active();
    return bt_gatt_attr_read(conn, attr, buf, len, offset, &current_layer, sizeof(current_layer));
}

static ssize_t read_profile(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                            void *buf, uint16_t len, uint16_t offset)
{
    current_profile = zmk_ble_active_profile_index();
    return bt_gatt_attr_read(conn, attr, buf, len, offset, &current_profile, sizeof(current_profile));
}

static void layer_ccc_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
    layer_notify_enabled = (value == BT_GATT_CCC_NOTIFY);
}

static void profile_ccc_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
    profile_notify_enabled = (value == BT_GATT_CCC_NOTIFY);
}

BT_GATT_SERVICE_DEFINE(zmk_state_svc,
    BT_GATT_PRIMARY_SERVICE(ZMK_STATE_SERVICE_UUID),
    BT_GATT_CHARACTERISTIC(ZMK_STATE_LAYER_UUID,
                           BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
                           BT_GATT_PERM_READ,
                           read_layer, NULL, &current_layer),
    BT_GATT_CCC(layer_ccc_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
    BT_GATT_CHARACTERISTIC(ZMK_STATE_PROFILE_UUID,
                           BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
                           BT_GATT_PERM_READ,
                           read_profile, NULL, &current_profile),
    BT_GATT_CCC(profile_ccc_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
);

static const struct bt_gatt_attr *get_layer_attr(void) { return &zmk_state_svc.attrs[2]; }
static const struct bt_gatt_attr *get_profile_attr(void) { return &zmk_state_svc.attrs[5]; }

static void notify_layer_change(void)
{
    if (!layer_notify_enabled) return;
    uint8_t layer = zmk_keymap_highest_layer_active();
    if (layer != current_layer) {
        current_layer = layer;
        bt_gatt_notify(NULL, get_layer_attr(), &current_layer, sizeof(current_layer));
    }
}

static void notify_profile_change(void)
{
    if (!profile_notify_enabled) return;
    uint8_t profile = zmk_ble_active_profile_index();
    if (profile != current_profile) {
        current_profile = profile;
        bt_gatt_notify(NULL, get_profile_attr(), &current_profile, sizeof(current_profile));
    }
}

static int state_service_layer_listener(const zmk_event_t *eh)
{
    notify_layer_change();
    return ZMK_EV_EVENT_BUBBLE;
}

static int state_service_profile_listener(const zmk_event_t *eh)
{
    notify_profile_change();
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(state_service_layer, state_service_layer_listener);
ZMK_SUBSCRIPTION(state_service_layer, zmk_layer_state_changed);

ZMK_LISTENER(state_service_profile, state_service_profile_listener);
ZMK_SUBSCRIPTION(state_service_profile, zmk_ble_active_profile_changed);
