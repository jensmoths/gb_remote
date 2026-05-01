#include "settings_registry.h"

#include "ble.h"
#include "lcd.h"
#include "nvs.h"
#include "power.h"
#include "sdkconfig.h"
#include "throttle.h"
#include "ui_updater.h"
#include "vesc_config.h"
#include "viber.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define NVS_NAMESPACE_LCD "lcd_cfg"
#define NVS_KEY_BACKLIGHT "backlight"

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

static esp_err_t get_speed_unit(char *out, size_t out_len);
static esp_err_t set_speed_unit(const char *value, char *out, size_t out_len);
static esp_err_t get_backlight(char *out, size_t out_len);
static esp_err_t set_backlight(const char *value, char *out, size_t out_len);
static esp_err_t get_haptic_intensity(char *out, size_t out_len);
static esp_err_t set_haptic_intensity(const char *value, char *out,
                                      size_t out_len);
static esp_err_t get_ble_trim(char *out, size_t out_len);
static esp_err_t set_ble_trim(const char *value, char *out, size_t out_len);
static esp_err_t get_invert_throttle(char *out, size_t out_len);
static esp_err_t set_invert_throttle(const char *value, char *out,
                                     size_t out_len);
static esp_err_t get_aux_output(char *out, size_t out_len);
static esp_err_t set_aux_output(const char *value, char *out, size_t out_len);

typedef esp_err_t (*setting_getter_t)(char *out, size_t out_len);
typedef esp_err_t (*setting_setter_t)(const char *value, char *out,
                                      size_t out_len);

typedef struct {
  const char *name;
  const char *help;
  setting_getter_t get;
  setting_setter_t set;
} setting_entry_t;

static const setting_entry_t settings[] = {
    {"speed_unit", "Speed unit: kmh or mph", get_speed_unit, set_speed_unit},
    {"backlight", "LCD backlight percent: 0..100", get_backlight,
     set_backlight},
    {"haptic_intensity", "Haptic vibration intensity percent: 0..100",
     get_haptic_intensity, set_haptic_intensity},
    {"ble_trim", "BLE neutral trim offset: -127..127", get_ble_trim,
     set_ble_trim},
    {"invert_throttle", "Lite only. Invert throttle: on/off",
     get_invert_throttle, set_invert_throttle},
    {"aux_output", "Remembered auxiliary output state: on/off", get_aux_output,
     set_aux_output},
};

static bool str_ieq(const char *a, const char *b) {
  if (!a || !b) {
    return false;
  }
  while (*a && *b) {
    if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) {
      return false;
    }
    a++;
    b++;
  }
  return *a == '\0' && *b == '\0';
}

static bool parse_i32(const char *value, int32_t min, int32_t max,
                      int32_t *out) {
  if (!value || !*value || !out) {
    return false;
  }
  char *end = NULL;
  long parsed = strtol(value, &end, 10);
  if (end == value || *end != '\0' || parsed < min || parsed > max) {
    return false;
  }
  *out = (int32_t)parsed;
  return true;
}

static bool parse_bool(const char *value, bool *out) {
  if (!value || !out) {
    return false;
  }
  if (str_ieq(value, "1") || str_ieq(value, "on") || str_ieq(value, "true") ||
      str_ieq(value, "yes") || str_ieq(value, "enabled")) {
    *out = true;
    return true;
  }
  if (str_ieq(value, "0") || str_ieq(value, "off") || str_ieq(value, "false") ||
      str_ieq(value, "no") || str_ieq(value, "disabled")) {
    *out = false;
    return true;
  }
  return false;
}

static const setting_entry_t *find_entry(const char *name) {
  if (!name) {
    return NULL;
  }
  for (size_t i = 0; i < ARRAY_SIZE(settings); i++) {
    if (str_ieq(name, settings[i].name)) {
      return &settings[i];
    }
  }
  return NULL;
}

static bool contains_case_insensitive(const char *haystack,
                                      const char *needle) {
  if (!haystack || !needle) {
    return false;
  }
  size_t needle_len = strlen(needle);
  if (needle_len == 0) {
    return true;
  }
  for (const char *p = haystack; *p; p++) {
    if (strncasecmp(p, needle, needle_len) == 0) {
      return true;
    }
  }
  return false;
}

static bool pattern_matches(const char *pattern, const char *name) {
  if (!pattern || !*pattern) {
    return true;
  }
  size_t len = strlen(pattern);
  if (len > 0 && pattern[len - 1] == '*') {
    return strncasecmp(pattern, name, len - 1) == 0;
  }
  return contains_case_insensitive(name, pattern);
}

static void print_line(settings_registry_print_cb_t print_cb, void *user_data,
                       const char *line) {
  if (print_cb) {
    print_cb(line, user_data);
  }
}

static void print_setting(const setting_entry_t *entry,
                          settings_registry_print_cb_t print_cb,
                          void *user_data, bool dump_format) {
  char value[64];
  char line[160];
  esp_err_t err = entry->get(value, sizeof(value));
  if (err == ESP_OK) {
    if (dump_format) {
      snprintf(line, sizeof(line), "set %s %s\r\n", entry->name, value);
    } else {
      snprintf(line, sizeof(line), "%-18s = %-10s ; %s\r\n", entry->name,
               value, entry->help);
    }
  } else {
    snprintf(line, sizeof(line), "%-18s = <err:%s> ; %s\r\n", entry->name,
             esp_err_to_name(err), entry->help);
  }
  print_line(print_cb, user_data, line);
}

esp_err_t settings_registry_get(const char *name, char *out, size_t out_len) {
  const setting_entry_t *entry = find_entry(name);
  if (!entry || !entry->get) {
    return ESP_ERR_NOT_FOUND;
  }
  return entry->get(out, out_len);
}

esp_err_t settings_registry_set(const char *name, const char *value, char *out,
                                size_t out_len) {
  const setting_entry_t *entry = find_entry(name);
  if (!entry || !entry->set) {
    return ESP_ERR_NOT_FOUND;
  }
  return entry->set(value, out, out_len);
}

void settings_registry_print_all(settings_registry_print_cb_t print_cb,
                                 void *user_data) {
  for (size_t i = 0; i < ARRAY_SIZE(settings); i++) {
    print_setting(&settings[i], print_cb, user_data, false);
  }
}

void settings_registry_print_dump(settings_registry_print_cb_t print_cb,
                                  void *user_data) {
  for (size_t i = 0; i < ARRAY_SIZE(settings); i++) {
    print_setting(&settings[i], print_cb, user_data, true);
  }
}

void settings_registry_print_matching(const char *pattern,
                                      settings_registry_print_cb_t print_cb,
                                      void *user_data) {
  bool any = false;
  for (size_t i = 0; i < ARRAY_SIZE(settings); i++) {
    if (pattern_matches(pattern, settings[i].name)) {
      print_setting(&settings[i], print_cb, user_data, false);
      any = true;
    }
  }
  if (!any) {
    print_line(print_cb, user_data, "No matching settings\r\n");
  }
}

const setting_info_t *settings_registry_find(const char *name) {
  static setting_info_t info;
  const setting_entry_t *entry = find_entry(name);
  if (!entry) {
    return NULL;
  }
  info.name = entry->name;
  info.help = entry->help;
  return &info;
}

static esp_err_t get_speed_unit(char *out, size_t out_len) {
  vesc_config_t config;
  esp_err_t err = vesc_config_load(&config);
  if (err != ESP_OK) {
    return err;
  }
  snprintf(out, out_len, "%s", config.speed_unit_mph ? "mph" : "kmh");
  return ESP_OK;
}

static esp_err_t set_speed_unit(const char *value, char *out, size_t out_len) {
  vesc_config_t config;
  esp_err_t err = vesc_config_load(&config);
  if (err != ESP_OK) {
    return err;
  }
  if (str_ieq(value, "mph") || str_ieq(value, "1")) {
    config.speed_unit_mph = true;
  } else if (str_ieq(value, "kmh") || str_ieq(value, "km/h") ||
             str_ieq(value, "0")) {
    config.speed_unit_mph = false;
  } else {
    return ESP_ERR_INVALID_ARG;
  }
  err = vesc_config_save(&config);
  if (err == ESP_OK) {
    ui_update_speed_unit(config.speed_unit_mph);
    ui_force_config_reload();
    snprintf(out, out_len, "%s", config.speed_unit_mph ? "mph" : "kmh");
  }
  return err;
}

static esp_err_t get_backlight(char *out, size_t out_len) {
  uint8_t brightness = LCD_BACKLIGHT_DEFAULT;
  nvs_handle_t nvs_handle;
  if (nvs_open(NVS_NAMESPACE_LCD, NVS_READONLY, &nvs_handle) == ESP_OK) {
    nvs_get_u8(nvs_handle, NVS_KEY_BACKLIGHT, &brightness);
    nvs_close(nvs_handle);
  }
  if (brightness > LCD_BACKLIGHT_MAX) {
    brightness = LCD_BACKLIGHT_DEFAULT;
  }
  snprintf(out, out_len, "%u", (unsigned)brightness);
  return ESP_OK;
}

static esp_err_t set_backlight(const char *value, char *out, size_t out_len) {
  int32_t brightness = 0;
  if (!parse_i32(value, LCD_BACKLIGHT_MIN, LCD_BACKLIGHT_MAX, &brightness)) {
    return ESP_ERR_INVALID_ARG;
  }

  uint8_t pwm_value = (uint8_t)((brightness * 255) / 100);
  lcd_fade_backlight(lcd_get_backlight(), pwm_value,
                     LCD_BACKLIGHT_FADE_DURATION_MS);

  nvs_handle_t nvs_handle;
  esp_err_t err = nvs_open(NVS_NAMESPACE_LCD, NVS_READWRITE, &nvs_handle);
  if (err == ESP_OK) {
    err = nvs_set_u8(nvs_handle, NVS_KEY_BACKLIGHT, (uint8_t)brightness);
    if (err == ESP_OK) {
      err = nvs_commit(nvs_handle);
    }
    nvs_close(nvs_handle);
  }
  if (err == ESP_OK) {
    snprintf(out, out_len, "%ld", (long)brightness);
  }
  return err;
}

static esp_err_t get_haptic_intensity(char *out, size_t out_len) {
  snprintf(out, out_len, "%u", (unsigned)viber_get_intensity());
  return ESP_OK;
}

static esp_err_t set_haptic_intensity(const char *value, char *out,
                                      size_t out_len) {
  int32_t intensity = 0;
  if (!parse_i32(value, 0, 100, &intensity)) {
    return ESP_ERR_INVALID_ARG;
  }
  esp_err_t err = viber_set_intensity((uint8_t)intensity);
  if (err == ESP_OK) {
    snprintf(out, out_len, "%ld", (long)intensity);
    if (intensity > 0) {
      viber_play_pattern(VIBER_PATTERN_SINGLE_SHORT);
    }
  }
  return err;
}

static esp_err_t get_ble_trim(char *out, size_t out_len) {
  snprintf(out, out_len, "%d", (int)ble_get_trim_offset());
  return ESP_OK;
}

static esp_err_t set_ble_trim(const char *value, char *out, size_t out_len) {
  if (power_get_mode() == POWER_MODE_CHARGING) {
    return ESP_ERR_NOT_SUPPORTED;
  }
  int32_t offset = 0;
  if (!parse_i32(value, -127, 127, &offset)) {
    return ESP_ERR_INVALID_ARG;
  }
  esp_err_t err = ble_set_trim_offset((int8_t)offset);
  if (err == ESP_OK) {
    snprintf(out, out_len, "%ld", (long)offset);
  }
  return err;
}

static esp_err_t get_invert_throttle(char *out, size_t out_len) {
#ifdef CONFIG_TARGET_LITE
  vesc_config_t config;
  esp_err_t err = vesc_config_load(&config);
  if (err != ESP_OK) {
    return err;
  }
  snprintf(out, out_len, "%s", config.invert_throttle ? "on" : "off");
#else
  snprintf(out, out_len, "unsupported");
#endif
  return ESP_OK;
}

static esp_err_t set_invert_throttle(const char *value, char *out,
                                     size_t out_len) {
#ifndef CONFIG_TARGET_LITE
  (void)value;
  (void)out;
  (void)out_len;
  return ESP_ERR_NOT_SUPPORTED;
#else
  if (power_get_mode() == POWER_MODE_CHARGING) {
    return ESP_ERR_NOT_SUPPORTED;
  }
  bool enabled = false;
  if (!parse_bool(value, &enabled)) {
    return ESP_ERR_INVALID_ARG;
  }
  vesc_config_t config;
  esp_err_t err = vesc_config_load(&config);
  if (err != ESP_OK) {
    return err;
  }
  config.invert_throttle = enabled;
  err = vesc_config_save(&config);
  if (err == ESP_OK) {
    ui_force_config_reload();
    snprintf(out, out_len, "%s", enabled ? "on" : "off");
  }
  return err;
#endif
}

static esp_err_t get_aux_output(char *out, size_t out_len) {
  snprintf(out, out_len, "%s", ble_get_aux_output_state() ? "on" : "off");
  return ESP_OK;
}

static esp_err_t set_aux_output(const char *value, char *out, size_t out_len) {
  bool enabled = false;
  if (!parse_bool(value, &enabled)) {
    return ESP_ERR_INVALID_ARG;
  }
  esp_err_t err = ble_set_aux_output_state(enabled);
  if (err == ESP_OK) {
    snprintf(out, out_len, "%s", enabled ? "on" : "off");
  }
  return err;
}
