/*
 * Dynamic scaler input processor with settings-backed scale (x10).
 */

#define DT_DRV_COMPAT zmk_input_processor_dynamic_scaler

#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/atomic.h>
#include <drivers/input_processor.h>

#if IS_ENABLED(CONFIG_SETTINGS)
#include <zephyr/settings/settings.h>
#endif

#include <zmk/zip_dynamic_scaler.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#define ZIP_DYN_SCALE_SETTINGS_PATH "zip_dynamic_scaler"
#define ZIP_DYN_SCALE_SETTINGS_KEY_0 "scale_x10_0"
#define ZIP_DYN_SCALE_SETTINGS_KEY_1 "scale_x10_1"
#define ZIP_DYN_SCALE_GROUPS 2

struct dyn_scaler_config {
    uint8_t type;
    uint8_t scale_group;
    size_t codes_len;
    uint16_t codes[];
};

static atomic_t scale_x10[ZIP_DYN_SCALE_GROUPS] = {
    ATOMIC_INIT(CONFIG_ZMK_INPUT_PROCESSOR_DYNAMIC_SCALER_DEFAULT_SCALE_X10),
    ATOMIC_INIT(CONFIG_ZMK_INPUT_PROCESSOR_DYNAMIC_SCALER_DEFAULT_SCALE_X10),
};

static uint16_t clamp_scale_x10(int32_t value) {
    if (value < CONFIG_ZMK_INPUT_PROCESSOR_DYNAMIC_SCALER_MIN_SCALE_X10) {
        return CONFIG_ZMK_INPUT_PROCESSOR_DYNAMIC_SCALER_MIN_SCALE_X10;
    }
    if (value > CONFIG_ZMK_INPUT_PROCESSOR_DYNAMIC_SCALER_MAX_SCALE_X10) {
        return CONFIG_ZMK_INPUT_PROCESSOR_DYNAMIC_SCALER_MAX_SCALE_X10;
    }
    return (uint16_t)value;
}

static bool scale_group_valid(uint8_t group) { return group < ZIP_DYN_SCALE_GROUPS; }

static int apply_scale_x10(struct input_event *event, uint16_t scale,
                           struct zmk_input_processor_state *state) {
    int32_t value_mul = (int32_t)event->value * (int32_t)scale;

    if (state && state->remainder) {
        value_mul += *state->remainder;
    }

    int32_t scaled = value_mul / 10;

    if (state && state->remainder) {
        *state->remainder = (int16_t)(value_mul - (scaled * 10));
    }

    event->value = (int32_t)scaled;
    return 0;
}

#if IS_ENABLED(CONFIG_SETTINGS)
static void save_scale_cb(struct k_work *work) {
    ARG_UNUSED(work);
    uint16_t val0 = zmk_zip_dynamic_scaler_get_scale_x10(0);
    uint16_t val1 = zmk_zip_dynamic_scaler_get_scale_x10(1);
    settings_save_one(ZIP_DYN_SCALE_SETTINGS_PATH "/" ZIP_DYN_SCALE_SETTINGS_KEY_0, &val0,
                      sizeof(val0));
    settings_save_one(ZIP_DYN_SCALE_SETTINGS_PATH "/" ZIP_DYN_SCALE_SETTINGS_KEY_1, &val1,
                      sizeof(val1));
}

#if defined(CONFIG_ZMK_SETTINGS_SAVE_DEBOUNCE)
K_WORK_DELAYABLE_DEFINE(scale_save_work, save_scale_cb);
#endif

static int schedule_save(void) {
#if defined(CONFIG_ZMK_SETTINGS_SAVE_DEBOUNCE)
    return k_work_reschedule(&scale_save_work, K_MSEC(CONFIG_ZMK_SETTINGS_SAVE_DEBOUNCE));
#else
    uint16_t val0 = zmk_zip_dynamic_scaler_get_scale_x10(0);
    uint16_t val1 = zmk_zip_dynamic_scaler_get_scale_x10(1);
    int rc = settings_save_one(ZIP_DYN_SCALE_SETTINGS_PATH "/" ZIP_DYN_SCALE_SETTINGS_KEY_0, &val0,
                               sizeof(val0));
    if (rc < 0) {
        return rc;
    }
    return settings_save_one(ZIP_DYN_SCALE_SETTINGS_PATH "/" ZIP_DYN_SCALE_SETTINGS_KEY_1, &val1,
                             sizeof(val1));
#endif
}
#endif

static int set_scale_x10_internal(uint8_t group, uint16_t new_scale, bool do_save) {
    if (!scale_group_valid(group)) {
        return -EINVAL;
    }
    uint16_t clamped = clamp_scale_x10(new_scale);
    atomic_set(&scale_x10[group], (atomic_val_t)clamped);

#if IS_ENABLED(CONFIG_SETTINGS)
    if (do_save) {
        return schedule_save();
    }
#else
    ARG_UNUSED(do_save);
#endif

    return 0;
}

uint16_t zmk_zip_dynamic_scaler_get_scale_x10(uint8_t group) {
    if (!scale_group_valid(group)) {
        return CONFIG_ZMK_INPUT_PROCESSOR_DYNAMIC_SCALER_DEFAULT_SCALE_X10;
    }
    return (uint16_t)atomic_get(&scale_x10[group]);
}

int zmk_zip_dynamic_scaler_set_scale_x10(uint8_t group, uint16_t scale) {
    return set_scale_x10_internal(group, scale, true);
}

int zmk_zip_dynamic_scaler_step(uint8_t group, int delta) {
    if (!scale_group_valid(group)) {
        return -EINVAL;
    }
    int32_t next = (int32_t)zmk_zip_dynamic_scaler_get_scale_x10(group) + delta;
    return set_scale_x10_internal(group, (uint16_t)next, true);
}

int zmk_zip_dynamic_scaler_reset(uint8_t group) {
    return set_scale_x10_internal(group,
                                  CONFIG_ZMK_INPUT_PROCESSOR_DYNAMIC_SCALER_DEFAULT_SCALE_X10,
                                  true);
}

static int dyn_scaler_handle_event(const struct device *dev, struct input_event *event,
                                   uint32_t param1, uint32_t param2,
                                   struct zmk_input_processor_state *state) {
    ARG_UNUSED(param1);
    ARG_UNUSED(param2);

    const struct dyn_scaler_config *cfg = dev->config;

    if (event->type != cfg->type) {
        return ZMK_INPUT_PROC_CONTINUE;
    }

    for (int i = 0; i < cfg->codes_len; i++) {
        if (cfg->codes[i] == event->code) {
            return apply_scale_x10(event, zmk_zip_dynamic_scaler_get_scale_x10(cfg->scale_group),
                                   state);
        }
    }

    return ZMK_INPUT_PROC_CONTINUE;
}

static struct zmk_input_processor_driver_api dyn_scaler_driver_api = {
    .handle_event = dyn_scaler_handle_event,
};

#define DYN_SCALER_INST(n)                                                                         \
    static const struct dyn_scaler_config dyn_scaler_config_##n = {                                \
        .type = DT_INST_PROP_OR(n, type, INPUT_EV_REL),                                            \
        .scale_group = DT_INST_PROP_OR(n, scale_group, 0),                                         \
        .codes_len = DT_INST_PROP_LEN(n, codes),                                                   \
        .codes = DT_INST_PROP(n, codes),                                                           \
    };                                                                                             \
    DEVICE_DT_INST_DEFINE(n, NULL, NULL, NULL, &dyn_scaler_config_##n, POST_KERNEL,                \
                          CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &dyn_scaler_driver_api);

DT_INST_FOREACH_STATUS_OKAY(DYN_SCALER_INST)

#if IS_ENABLED(CONFIG_SETTINGS)
static int dyn_scaler_settings_set(const char *name, size_t len, settings_read_cb read_cb,
                                   void *cb_arg) {
    const char *next;

    uint8_t group = 0;
    if (settings_name_steq(name, ZIP_DYN_SCALE_SETTINGS_KEY_0, &next) && !next) {
        group = 0;
    } else if (settings_name_steq(name, ZIP_DYN_SCALE_SETTINGS_KEY_1, &next) && !next) {
        group = 1;
    } else {
        return -ENOENT;
    }

    if (len != sizeof(uint16_t)) {
        return -EINVAL;
    }

    uint16_t val = 0;
    int rc = read_cb(cb_arg, &val, sizeof(val));
    if (rc < 0) {
        return rc;
    }

    return set_scale_x10_internal(group, val, false);
}

SETTINGS_STATIC_HANDLER_DEFINE(zip_dynamic_scaler, ZIP_DYN_SCALE_SETTINGS_PATH, NULL,
                               dyn_scaler_settings_set, NULL, NULL);
#endif
