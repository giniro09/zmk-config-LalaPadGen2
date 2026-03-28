#pragma once

#include <stdint.h>

uint16_t zmk_zip_dynamic_scaler_get_scale_x10(uint8_t group);
int zmk_zip_dynamic_scaler_set_scale_x10(uint8_t group, uint16_t scale_x10);
int zmk_zip_dynamic_scaler_step(uint8_t group, int delta);
int zmk_zip_dynamic_scaler_reset(uint8_t group);
