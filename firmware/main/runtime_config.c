#include "runtime_config.h"

#include "esp_log.h"
#include "nvs.h"
#include <stdbool.h>

#define TAG "RUNTIME_CFG"
#define RUNTIME_NVS_NAMESPACE "runtime_cfg"

#define KEY_SHUTDOWN_HOLD_MS "sd_hold_ms"
#define KEY_BUTTON_DOUBLE_MS "dbl_ms"
#define KEY_SHUTDOWN_ARM_MS "sd_arm_ms"
#define KEY_SHUTDOWN_ANIM_MS "sd_anim_ms"
#define KEY_SHUTDOWN_FB_MS "sd_fb_ms"
#define KEY_AUTO_OFF_S "auto_off_s"

static uint32_t shutdown_hold_ms = BUTTON_LONG_PRESS_TIME_MS;
static uint32_t button_double_press_ms = BUTTON_DOUBLE_PRESS_TIME_MS;
static uint32_t shutdown_arm_window_ms = POWER_OFF_ARM_WINDOW_MS;
static uint32_t shutdown_animation_ms = SHUTDOWN_ANIMATION_TIME;
static uint32_t shutdown_feedback_ms = SHUTDOWN_FEEDBACK_DELAY_MS;
static uint32_t auto_off_timeout_s = INACTIVITY_TIMEOUT_MS / 1000;

static bool in_range(uint32_t value, uint32_t min, uint32_t max) {
  return value >= min && value <= max;
}

static void load_u32(nvs_handle_t handle, const char *key, uint32_t *value,
                     uint32_t min, uint32_t max) {
  uint32_t stored = 0;
  esp_err_t err = nvs_get_u32(handle, key, &stored);
  if (err == ESP_OK) {
    if (in_range(stored, min, max)) {
      *value = stored;
    } else {
      ESP_LOGW(TAG, "Ignoring out-of-range %s=%lu", key, stored);
    }
  }
}

static esp_err_t save_u32(const char *key, uint32_t value, uint32_t min,
                          uint32_t max) {
  if (!in_range(value, min, max)) {
    return ESP_ERR_INVALID_ARG;
  }

  nvs_handle_t handle;
  esp_err_t err = nvs_open(RUNTIME_NVS_NAMESPACE, NVS_READWRITE, &handle);
  if (err != ESP_OK) {
    return err;
  }

  err = nvs_set_u32(handle, key, value);
  if (err == ESP_OK) {
    err = nvs_commit(handle);
  }
  nvs_close(handle);
  return err;
}

esp_err_t runtime_config_init(void) {
  nvs_handle_t handle;
  esp_err_t err = nvs_open(RUNTIME_NVS_NAMESPACE, NVS_READONLY, &handle);
  if (err == ESP_ERR_NVS_NOT_FOUND) {
    return ESP_OK;
  }
  if (err != ESP_OK) {
    return err;
  }

  load_u32(handle, KEY_SHUTDOWN_HOLD_MS, &shutdown_hold_ms,
           RUNTIME_CONFIG_SHUTDOWN_HOLD_MIN_MS,
           RUNTIME_CONFIG_SHUTDOWN_HOLD_MAX_MS);
  load_u32(handle, KEY_BUTTON_DOUBLE_MS, &button_double_press_ms,
           RUNTIME_CONFIG_BUTTON_DOUBLE_PRESS_MIN_MS,
           RUNTIME_CONFIG_BUTTON_DOUBLE_PRESS_MAX_MS);
  load_u32(handle, KEY_SHUTDOWN_ARM_MS, &shutdown_arm_window_ms,
           RUNTIME_CONFIG_SHUTDOWN_ARM_WINDOW_MIN_MS,
           RUNTIME_CONFIG_SHUTDOWN_ARM_WINDOW_MAX_MS);
  load_u32(handle, KEY_SHUTDOWN_ANIM_MS, &shutdown_animation_ms,
           RUNTIME_CONFIG_SHUTDOWN_ANIMATION_MIN_MS,
           RUNTIME_CONFIG_SHUTDOWN_ANIMATION_MAX_MS);
  load_u32(handle, KEY_SHUTDOWN_FB_MS, &shutdown_feedback_ms,
           RUNTIME_CONFIG_SHUTDOWN_FEEDBACK_MIN_MS,
           RUNTIME_CONFIG_SHUTDOWN_FEEDBACK_MAX_MS);
  load_u32(handle, KEY_AUTO_OFF_S, &auto_off_timeout_s,
           RUNTIME_CONFIG_AUTO_OFF_MIN_S, RUNTIME_CONFIG_AUTO_OFF_MAX_S);

  nvs_close(handle);
  ESP_LOGI(TAG,
           "Loaded runtime config: shutdown_hold=%lums double=%lums "
           "arm=%lums anim=%lums feedback=%lums auto_off=%lus",
           shutdown_hold_ms, button_double_press_ms, shutdown_arm_window_ms,
           shutdown_animation_ms, shutdown_feedback_ms, auto_off_timeout_s);
  return ESP_OK;
}

uint32_t runtime_config_get_shutdown_hold_ms(void) {
  return shutdown_hold_ms;
}

esp_err_t runtime_config_set_shutdown_hold_ms(uint32_t value) {
  esp_err_t err = save_u32(KEY_SHUTDOWN_HOLD_MS, value,
                           RUNTIME_CONFIG_SHUTDOWN_HOLD_MIN_MS,
                           RUNTIME_CONFIG_SHUTDOWN_HOLD_MAX_MS);
  if (err == ESP_OK) {
    shutdown_hold_ms = value;
  }
  return err;
}

uint32_t runtime_config_get_button_double_press_ms(void) {
  return button_double_press_ms;
}

esp_err_t runtime_config_set_button_double_press_ms(uint32_t value) {
  esp_err_t err = save_u32(KEY_BUTTON_DOUBLE_MS, value,
                           RUNTIME_CONFIG_BUTTON_DOUBLE_PRESS_MIN_MS,
                           RUNTIME_CONFIG_BUTTON_DOUBLE_PRESS_MAX_MS);
  if (err == ESP_OK) {
    button_double_press_ms = value;
  }
  return err;
}

uint32_t runtime_config_get_shutdown_arm_window_ms(void) {
  return shutdown_arm_window_ms;
}

esp_err_t runtime_config_set_shutdown_arm_window_ms(uint32_t value) {
  esp_err_t err = save_u32(KEY_SHUTDOWN_ARM_MS, value,
                           RUNTIME_CONFIG_SHUTDOWN_ARM_WINDOW_MIN_MS,
                           RUNTIME_CONFIG_SHUTDOWN_ARM_WINDOW_MAX_MS);
  if (err == ESP_OK) {
    shutdown_arm_window_ms = value;
  }
  return err;
}

uint32_t runtime_config_get_shutdown_animation_ms(void) {
  return shutdown_animation_ms;
}

esp_err_t runtime_config_set_shutdown_animation_ms(uint32_t value) {
  esp_err_t err = save_u32(KEY_SHUTDOWN_ANIM_MS, value,
                           RUNTIME_CONFIG_SHUTDOWN_ANIMATION_MIN_MS,
                           RUNTIME_CONFIG_SHUTDOWN_ANIMATION_MAX_MS);
  if (err == ESP_OK) {
    shutdown_animation_ms = value;
  }
  return err;
}

uint32_t runtime_config_get_shutdown_feedback_ms(void) {
  return shutdown_feedback_ms;
}

esp_err_t runtime_config_set_shutdown_feedback_ms(uint32_t value) {
  esp_err_t err = save_u32(KEY_SHUTDOWN_FB_MS, value,
                           RUNTIME_CONFIG_SHUTDOWN_FEEDBACK_MIN_MS,
                           RUNTIME_CONFIG_SHUTDOWN_FEEDBACK_MAX_MS);
  if (err == ESP_OK) {
    shutdown_feedback_ms = value;
  }
  return err;
}

uint32_t runtime_config_get_auto_off_timeout_s(void) {
  return auto_off_timeout_s;
}

esp_err_t runtime_config_set_auto_off_timeout_s(uint32_t value) {
  esp_err_t err = save_u32(KEY_AUTO_OFF_S, value, RUNTIME_CONFIG_AUTO_OFF_MIN_S,
                           RUNTIME_CONFIG_AUTO_OFF_MAX_S);
  if (err == ESP_OK) {
    auto_off_timeout_s = value;
  }
  return err;
}
