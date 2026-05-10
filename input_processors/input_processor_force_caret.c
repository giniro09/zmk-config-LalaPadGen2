/*
 * Shared Force/Caret input processor.
 *
 * The IQS9151 driver still emits Force as BTN0 down/up. This processor sits
 * before the button-to-position processor on the central side, watches BTN0
 * holds from either trackpad, and decides globally whether the hold becomes a
 * drag or a caret session.
 */

#define DT_DRV_COMPAT zmk_input_processor_force_caret

#include <zephyr/device.h>
#include <zephyr/dt-bindings/input/input-event-codes.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>
#include <drivers/input_processor.h>

#include <zmk/behavior.h>
#include <zmk/keymap.h>
#include <zmk/virtual_key_position.h>

#define FORCE_CARET_HOLD_MS CONFIG_ZMK_INPUT_PROCESSOR_FORCE_CARET_HOLD_MS
#define FORCE_CARET_DEADZONE CONFIG_ZMK_INPUT_PROCESSOR_FORCE_CARET_DEADZONE
#define FORCE_CARET_MOVE_THRESHOLD CONFIG_ZMK_INPUT_PROCESSOR_FORCE_CARET_MOVE_THRESHOLD
#define FORCE_CARET_MAX_STEPS_PER_EVENT 4

struct force_caret_config {
    uint8_t index;
    uint8_t side;
    struct zmk_behavior_binding primary;
    struct zmk_behavior_binding left;
    struct zmk_behavior_binding right;
    struct zmk_behavior_binding up;
    struct zmk_behavior_binding down;
    struct zmk_behavior_binding entry_haptic;
    struct zmk_behavior_binding step_haptic;
};

struct force_caret_state {
    struct k_mutex lock;
    struct k_work_delayable hold_work;
    bool initialized;
    bool candidate;
    bool caret_active;
    bool primary_released;
    uint8_t side;
    int32_t dx;
    int32_t dy;
    const struct force_caret_config *cfg;
    struct zmk_behavior_binding_event event;
};

static struct force_caret_state shared;

static void force_caret_reset_locked(void) {
    shared.candidate = false;
    shared.caret_active = false;
    shared.primary_released = false;
    shared.side = 0U;
    shared.dx = 0;
    shared.dy = 0;
    shared.cfg = NULL;
}

static struct zmk_behavior_binding_event force_caret_event_now(void) {
    struct zmk_behavior_binding_event event = shared.event;

    event.timestamp = k_uptime_get();
    return event;
}

static int force_caret_invoke(const struct zmk_behavior_binding *binding, int32_t value) {
    return zmk_behavior_invoke_binding(binding, force_caret_event_now(), value);
}

static int force_caret_tap(const struct zmk_behavior_binding *binding) {
    int ret = force_caret_invoke(binding, 1);

    if (ret < 0) {
        return ret;
    }
    return force_caret_invoke(binding, 0);
}

static void force_caret_play_haptic(const struct zmk_behavior_binding *binding) {
    (void)force_caret_invoke(binding, 1);
    (void)force_caret_invoke(binding, 0);
}

static void force_caret_hold_work_cb(struct k_work *work) {
    ARG_UNUSED(work);

    k_mutex_lock(&shared.lock, K_FOREVER);
    if (!shared.candidate || shared.caret_active || shared.cfg == NULL) {
        k_mutex_unlock(&shared.lock);
        return;
    }

    (void)force_caret_invoke(&shared.cfg->primary, 0);
    shared.primary_released = true;
    shared.candidate = false;
    shared.caret_active = true;
    shared.dx = 0;
    shared.dy = 0;
    force_caret_play_haptic(&shared.cfg->entry_haptic);
    k_mutex_unlock(&shared.lock);
}

static int force_caret_ensure_init(void) {
    if (shared.initialized) {
        return 0;
    }

    k_mutex_init(&shared.lock);
    k_work_init_delayable(&shared.hold_work, force_caret_hold_work_cb);
    force_caret_reset_locked();
    shared.initialized = true;
    return 0;
}

static void force_caret_consume(struct input_event *event) {
    event->type = INPUT_EV_KEY;
    event->code = INPUT_BTN_8;
    event->value = 0;
}

static int force_caret_emit_steps_locked(const struct force_caret_config *motion_cfg) {
    int ret = 0;
    uint8_t steps = 0U;

    while (steps < FORCE_CARET_MAX_STEPS_PER_EVENT) {
        const int32_t abs_dx = shared.dx < 0 ? -shared.dx : shared.dx;
        const int32_t abs_dy = shared.dy < 0 ? -shared.dy : shared.dy;
        const struct zmk_behavior_binding *binding;

        if (MAX(abs_dx, abs_dy) < FORCE_CARET_DEADZONE) {
            break;
        }

        if (abs_dx >= abs_dy) {
            binding = shared.dx < 0 ? &shared.cfg->left : &shared.cfg->right;
            shared.dx += shared.dx < 0 ? FORCE_CARET_DEADZONE : -FORCE_CARET_DEADZONE;
        } else {
            binding = shared.dy < 0 ? &shared.cfg->up : &shared.cfg->down;
            shared.dy += shared.dy < 0 ? FORCE_CARET_DEADZONE : -FORCE_CARET_DEADZONE;
        }

        ret = force_caret_tap(binding);
        if (ret < 0) {
            return ret;
        }
        force_caret_play_haptic(&motion_cfg->step_haptic);
        steps++;
    }

    return 0;
}

static int32_t force_caret_abs32(int32_t value) {
    return value < 0 ? -value : value;
}

static int force_caret_handle_key(const struct force_caret_config *cfg,
                                  struct input_event *event,
                                  struct zmk_input_processor_state *state) {
    int ret = ZMK_INPUT_PROC_CONTINUE;

    if (event->code != INPUT_BTN_0) {
        return ZMK_INPUT_PROC_CONTINUE;
    }

    k_mutex_lock(&shared.lock, K_FOREVER);

    if (event->value != 0) {
        if (!shared.candidate && !shared.caret_active) {
            if (state == NULL) {
                k_mutex_unlock(&shared.lock);
                return ZMK_INPUT_PROC_CONTINUE;
            }
            shared.candidate = true;
            shared.caret_active = false;
            shared.primary_released = false;
            shared.side = cfg->side;
            shared.dx = 0;
            shared.dy = 0;
            shared.cfg = cfg;
            shared.event = (struct zmk_behavior_binding_event) {
                .position = ZMK_VIRTUAL_KEY_POSITION_BEHAVIOR_INPUT_PROCESSOR(
                    state->input_device_index, cfg->index),
                .timestamp = k_uptime_get(),
#if IS_ENABLED(CONFIG_ZMK_SPLIT)
                .source = ZMK_POSITION_STATE_CHANGE_SOURCE_LOCAL,
#endif
            };
            (void)k_work_reschedule(&shared.hold_work, K_MSEC(FORCE_CARET_HOLD_MS));
        }
        k_mutex_unlock(&shared.lock);
        return ret;
    }

    if ((shared.candidate || shared.caret_active || shared.primary_released) &&
        shared.side == cfg->side) {
        const bool consume_release = shared.primary_released || shared.caret_active;

        (void)k_work_cancel_delayable(&shared.hold_work);
        force_caret_reset_locked();
        if (consume_release) {
            force_caret_consume(event);
        }
    }

    k_mutex_unlock(&shared.lock);
    return ret;
}

static int force_caret_handle_rel(const struct force_caret_config *cfg, struct input_event *event) {
    int ret = ZMK_INPUT_PROC_CONTINUE;

    if (event->code != INPUT_REL_X && event->code != INPUT_REL_Y) {
        return ZMK_INPUT_PROC_CONTINUE;
    }
    if (event->value == 0) {
        return ZMK_INPUT_PROC_CONTINUE;
    }

    k_mutex_lock(&shared.lock, K_FOREVER);

    if (shared.candidate) {
        if (force_caret_abs32(event->value) >= FORCE_CARET_MOVE_THRESHOLD) {
            (void)k_work_cancel_delayable(&shared.hold_work);
            force_caret_reset_locked();
        }
        k_mutex_unlock(&shared.lock);
        return ret;
    }

    if (shared.caret_active && shared.cfg != NULL) {
        if (event->code == INPUT_REL_X) {
            shared.dx += event->value;
        } else {
            shared.dy += event->value;
        }
        ret = force_caret_emit_steps_locked(cfg);
        event->value = 0;
    }

    k_mutex_unlock(&shared.lock);
    return ret;
}

static int force_caret_handle_event(const struct device *dev, struct input_event *event,
                                    uint32_t param1, uint32_t param2,
                                    struct zmk_input_processor_state *state) {
    ARG_UNUSED(dev);
    ARG_UNUSED(param1);
    ARG_UNUSED(param2);

    const struct force_caret_config *cfg = dev->config;

    if (event->type == INPUT_EV_KEY) {
        return force_caret_handle_key(cfg, event, state);
    }
    if (event->type == INPUT_EV_REL) {
        return force_caret_handle_rel(cfg, event);
    }

    return ZMK_INPUT_PROC_CONTINUE;
}

static struct zmk_input_processor_driver_api force_caret_driver_api = {
    .handle_event = force_caret_handle_event,
};

static int force_caret_init(const struct device *dev) {
    ARG_UNUSED(dev);
    return force_caret_ensure_init();
}

#define FORCE_CARET_INST(n)                                                                        \
    BUILD_ASSERT(DT_INST_PROP_LEN(n, bindings) == 7,                                               \
                 "force-caret bindings must be primary, left, right, up, down, entry haptic, "    \
                 "step haptic");                                                                 \
    static const struct force_caret_config force_caret_config_##n = {                              \
        .index = n,                                                                                \
        .side = DT_INST_PROP(n, side),                                                             \
        .primary = ZMK_KEYMAP_EXTRACT_BINDING(0, DT_DRV_INST(n)),                                  \
        .left = ZMK_KEYMAP_EXTRACT_BINDING(1, DT_DRV_INST(n)),                                     \
        .right = ZMK_KEYMAP_EXTRACT_BINDING(2, DT_DRV_INST(n)),                                    \
        .up = ZMK_KEYMAP_EXTRACT_BINDING(3, DT_DRV_INST(n)),                                       \
        .down = ZMK_KEYMAP_EXTRACT_BINDING(4, DT_DRV_INST(n)),                                     \
        .entry_haptic = ZMK_KEYMAP_EXTRACT_BINDING(5, DT_DRV_INST(n)),                             \
        .step_haptic = ZMK_KEYMAP_EXTRACT_BINDING(6, DT_DRV_INST(n)),                              \
    };                                                                                             \
    DEVICE_DT_INST_DEFINE(n, &force_caret_init, NULL, NULL, &force_caret_config_##n, POST_KERNEL,  \
                          CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &force_caret_driver_api);

DT_INST_FOREACH_STATUS_OKAY(FORCE_CARET_INST)
