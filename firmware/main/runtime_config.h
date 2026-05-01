#pragma once

#include "esp_err.h"
#include "button.h"
#include "power.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RUNTIME_CONFIG_SHUTDOWN_HOLD_MIN_MS 100
#define RUNTIME_CONFIG_SHUTDOWN_HOLD_MAX_MS 5000
#define RUNTIME_CONFIG_BUTTON_DOUBLE_PRESS_MIN_MS 50
#define RUNTIME_CONFIG_BUTTON_DOUBLE_PRESS_MAX_MS 1000
#define RUNTIME_CONFIG_SHUTDOWN_ARM_WINDOW_MIN_MS 300
#define RUNTIME_CONFIG_SHUTDOWN_ARM_WINDOW_MAX_MS 10000
#define RUNTIME_CONFIG_SHUTDOWN_ANIMATION_MIN_MS 100
#define RUNTIME_CONFIG_SHUTDOWN_ANIMATION_MAX_MS 10000
#define RUNTIME_CONFIG_SHUTDOWN_FEEDBACK_MIN_MS 0
#define RUNTIME_CONFIG_SHUTDOWN_FEEDBACK_MAX_MS 2000
#define RUNTIME_CONFIG_AUTO_OFF_MIN_S 0
#define RUNTIME_CONFIG_AUTO_OFF_MAX_S 3600

esp_err_t runtime_config_init(void);

uint32_t runtime_config_get_shutdown_hold_ms(void);
esp_err_t runtime_config_set_shutdown_hold_ms(uint32_t value);

uint32_t runtime_config_get_button_double_press_ms(void);
esp_err_t runtime_config_set_button_double_press_ms(uint32_t value);

uint32_t runtime_config_get_shutdown_arm_window_ms(void);
esp_err_t runtime_config_set_shutdown_arm_window_ms(uint32_t value);

uint32_t runtime_config_get_shutdown_animation_ms(void);
esp_err_t runtime_config_set_shutdown_animation_ms(uint32_t value);

uint32_t runtime_config_get_shutdown_feedback_ms(void);
esp_err_t runtime_config_set_shutdown_feedback_ms(uint32_t value);

uint32_t runtime_config_get_auto_off_timeout_s(void);
esp_err_t runtime_config_set_auto_off_timeout_s(uint32_t value);

#ifdef __cplusplus
}
#endif
