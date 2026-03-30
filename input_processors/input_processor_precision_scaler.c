/*
 * Input processor that applies a fixed XY scale while IQS9151 precision mode
 * is active.
 */

#define DT_DRV_COMPAT zmk_input_processor_precision_scaler

#include <zephyr/device.h>
#include <zephyr/dt-bindings/input/input-event-codes.h>
#include <drivers/input_processor.h>

#include <zmk/iqs9151_runtime.h>

static int ip_precision_scaler_handle_event(const struct device *dev, struct input_event *event,
                                            uint32_t param1, uint32_t param2,
                                            struct zmk_input_processor_state *state) {
    ARG_UNUSED(dev);
    ARG_UNUSED(param1);
    ARG_UNUSED(param2);

    if (event->type != INPUT_EV_REL) {
        return ZMK_INPUT_PROC_CONTINUE;
    }

    if (event->code != INPUT_REL_X && event->code != INPUT_REL_Y) {
        return ZMK_INPUT_PROC_CONTINUE;
    }

#if DT_HAS_COMPAT_STATUS_OKAY(azoteq_iqs9151)
    const struct device *trackpad = DEVICE_DT_GET(DT_COMPAT_GET_ANY_STATUS_OKAY(azoteq_iqs9151));

    if (!device_is_ready(trackpad) || !iqs9151_get_precision_pointer_active(trackpad)) {
        return ZMK_INPUT_PROC_CONTINUE;
    }
#else
    return ZMK_INPUT_PROC_CONTINUE;
#endif

    int32_t scaled = (int32_t)event->value * CONFIG_ZMK_INPUT_PROCESSOR_PRECISION_SCALER_SCALE_X10;
    if (state != NULL && state->remainder != NULL) {
        scaled += *state->remainder;
    }

    event->value = scaled / 10;

    if (state != NULL && state->remainder != NULL) {
        *state->remainder = (int16_t)(scaled - ((int32_t)event->value * 10));
    }

    return ZMK_INPUT_PROC_CONTINUE;
}

static struct zmk_input_processor_driver_api ip_precision_scaler_driver_api = {
    .handle_event = ip_precision_scaler_handle_event,
};

#define IP_PRECISION_SCALER_INST(n)                                                                \
    DEVICE_DT_INST_DEFINE(n, NULL, NULL, NULL, NULL, POST_KERNEL,                                 \
                          CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,                                     \
                          &ip_precision_scaler_driver_api);

DT_INST_FOREACH_STATUS_OKAY(IP_PRECISION_SCALER_INST)
