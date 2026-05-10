#pragma once

#include <stdbool.h>

struct device;

int iqs9151_toggle_cursor_inertia(const struct device *dev);
bool iqs9151_get_cursor_inertia_enabled(const struct device *dev);
bool iqs9151_get_precision_pointer_active(const struct device *dev);
uint16_t iqs9151_get_precision_pointer_force_delta(const struct device *dev);
