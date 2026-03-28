#define DT_DRV_COMPAT zmk_behavior_iqs9151_cursor_inertia_toggle

#include <zephyr/device.h>
#include <zephyr/devicetree.h>

#include <drivers/behavior.h>
#include <zmk/behavior.h>
#include <zmk/iqs9151_runtime.h>

#include <stdint.h>

#define IQS9151_SIDE_PERIPHERAL 0
#define IQS9151_SIDE_CENTRAL 1
#define IS_SPLIT_PERIPHERAL (IS_ENABLED(CONFIG_ZMK_SPLIT) && !IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL))

struct behavior_iqs9151_cursor_inertia_toggle_config {
    uint8_t target_side;
};

static bool behavior_iqs9151_current_side_matches(uint8_t target_side) {
    const uint8_t current_side = IS_SPLIT_PERIPHERAL ? IQS9151_SIDE_PERIPHERAL : IQS9151_SIDE_CENTRAL;
    return target_side == current_side;
}

static const struct device *behavior_iqs9151_get_device(void) {
#if DT_HAS_COMPAT_STATUS_OKAY(azoteq_iqs9151)
    const struct device *dev = DEVICE_DT_GET(DT_COMPAT_GET_ANY_STATUS_OKAY(azoteq_iqs9151));
    return device_is_ready(dev) ? dev : NULL;
#else
    return NULL;
#endif
}

static int behavior_iqs9151_cursor_inertia_toggle_binding_pressed(
    struct zmk_behavior_binding *binding, struct zmk_behavior_binding_event event) {
    ARG_UNUSED(event);

    const struct behavior_iqs9151_cursor_inertia_toggle_config *cfg =
        zmk_behavior_get_binding(binding->behavior_dev)->config;

    if (!behavior_iqs9151_current_side_matches(cfg->target_side)) {
        return ZMK_BEHAVIOR_OPAQUE;
    }

    const struct device *dev = behavior_iqs9151_get_device();
    if (dev == NULL) {
        return -ENODEV;
    }

    int ret = iqs9151_toggle_cursor_inertia(dev);
    return ret < 0 ? ret : ZMK_BEHAVIOR_OPAQUE;
}

static int behavior_iqs9151_cursor_inertia_toggle_binding_released(
    struct zmk_behavior_binding *binding, struct zmk_behavior_binding_event event) {
    ARG_UNUSED(binding);
    ARG_UNUSED(event);
    return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api behavior_iqs9151_cursor_inertia_toggle_driver_api = {
    .locality = BEHAVIOR_LOCALITY_GLOBAL,
    .binding_pressed = behavior_iqs9151_cursor_inertia_toggle_binding_pressed,
    .binding_released = behavior_iqs9151_cursor_inertia_toggle_binding_released,
};

#define IQS9151_CURSOR_INERTIA_TOGGLE_INST(n)                                                      \
    static const struct behavior_iqs9151_cursor_inertia_toggle_config                              \
        behavior_iqs9151_cursor_inertia_toggle_config_##n = {                                      \
            .target_side = DT_INST_PROP(n, target_side),                                           \
    };                                                                                             \
    BEHAVIOR_DT_INST_DEFINE(n, NULL, NULL, NULL,                                                   \
                            &behavior_iqs9151_cursor_inertia_toggle_config_##n, POST_KERNEL,       \
                            CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,                                   \
                            &behavior_iqs9151_cursor_inertia_toggle_driver_api);

DT_INST_FOREACH_STATUS_OKAY(IQS9151_CURSOR_INERTIA_TOGGLE_INST)
