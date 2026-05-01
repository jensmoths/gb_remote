#pragma once

#include "esp_err.h"
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*settings_registry_print_cb_t)(const char *text, void *user_data);

typedef struct {
  const char *name;
  const char *help;
} setting_info_t;

esp_err_t settings_registry_get(const char *name, char *out, size_t out_len);
esp_err_t settings_registry_set(const char *name, const char *value,
                                char *out, size_t out_len);
void settings_registry_print_all(settings_registry_print_cb_t print_cb,
                                 void *user_data);
void settings_registry_print_dump(settings_registry_print_cb_t print_cb,
                                  void *user_data);
void settings_registry_print_matching(const char *pattern,
                                      settings_registry_print_cb_t print_cb,
                                      void *user_data);
const setting_info_t *settings_registry_find(const char *name);

#ifdef __cplusplus
}
#endif
