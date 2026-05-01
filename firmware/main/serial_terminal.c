#include "serial_terminal.h"

#include "ble.h"
#include "driver/usb_serial_jtag.h"
#include "esp_err.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "power.h"
#include "settings_registry.h"
#include "target_config.h"
#include "throttle.h"
#include "version.h"
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#define CLI_LINE_MAX 160
#define CLI_PROMPT "gb> "

static bool cli_active = false;
static char line_buf[CLI_LINE_MAX];
static size_t line_len = 0;
static bool last_was_cr = false;

static void cli_write(const char *text) {
  if (!text) {
    return;
  }
  usb_serial_jtag_write_bytes(text, strlen(text), pdMS_TO_TICKS(100));
}

void serial_terminal_printf(const char *fmt, ...) {
  char buffer[256];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buffer, sizeof(buffer), fmt, args);
  va_end(args);
  cli_write(buffer);
}

static void print_cb(const char *text, void *user_data) {
  (void)user_data;
  cli_write(text);
}

static char *trim(char *s) {
  while (*s == ' ' || *s == '\t') {
    s++;
  }
  char *end = s + strlen(s);
  while (end > s && (end[-1] == ' ' || end[-1] == '\t')) {
    *--end = '\0';
  }
  return s;
}

static bool streq(const char *a, const char *b) {
  return a && b && strcasecmp(a, b) == 0;
}

static const char *cal_error_name(calibration_result_t result) {
  switch (result) {
  case CAL_OK:
    return "OK";
  case CAL_FAIL_THROTTLE_RANGE:
    return "throttle range too small";
  case CAL_FAIL_THROTTLE_NO_READINGS:
    return "no throttle readings";
  case CAL_FAIL_BRAKE_RANGE:
    return "brake range too small";
  case CAL_FAIL_BRAKE_NO_READINGS:
    return "no brake readings";
  case CAL_FAIL_SAVE:
    return "save failed";
  default:
    return "unknown error";
  }
}

static void print_help(void) {
  cli_write("Commands:\r\n");
  cli_write("  help                         Show this help\r\n");
  cli_write("  status                       Show device status\r\n");
  cli_write("  version                      Show firmware version\r\n");
  cli_write("  get [name|pattern]           Show settings (* suffix allowed)\r\n");
  cli_write("  set <name> <value>           Set and save a setting\r\n");
  cli_write("  dump                         Print replayable set commands\r\n");
  cli_write("  bms                          Show latest parsed BMS getter values\r\n");
  cli_write("  calibration show             Show calibration values\r\n");
  cli_write("  calibrate                    Run throttle/brake calibration\r\n");
  cli_write("  boot                         Leave charging mode and boot full app\r\n");
  cli_write("  reboot                       Restart the controller\r\n");
  cli_write("  exit                         Leave CLI mode\r\n");
}

static void print_status(void) {
  serial_terminal_printf("target: %s\r\n", TARGET_NAME);
  serial_terminal_printf("firmware: %s\r\n", FW_VERSION);
  serial_terminal_printf("mode: %s\r\n",
                         power_get_mode() == POWER_MODE_CHARGING ? "charging"
                                                                 : "full");
  serial_terminal_printf("ble: %s\r\n", ble_is_connected() ? "connected"
                                                          : "disconnected");
  serial_terminal_printf("calibrated: %s\r\n",
                         throttle_is_calibrated() ? "yes" : "no");
  settings_registry_print_all(print_cb, NULL);
}

static void print_bms_diagnostics(void) {
  uint8_t cells = get_bms_num_cells();
  float first_cell = cells > 0 ? get_bms_cell_voltage(0) : 0.0f;
  float last_cell = cells > 0 ? get_bms_cell_voltage(cells - 1) : 0.0f;
  serial_terminal_printf(
      "BMS getters: total=%.2fV current=%.2fA rem=%.2fAh nominal=%.2fAh "
      "cells=%u first=%.3f last=%.3f pct=%d\r\n",
      get_bms_total_voltage(), get_bms_current(), get_bms_remaining_capacity(),
      get_bms_nominal_capacity(), (unsigned)cells, first_cell, last_cell,
      get_bms_battery_percentage());
  serial_terminal_printf(
      "VESC/receiver fallback: vesc_voltage=%.2fV ble=%s connected_trip=%.2fkm\r\n",
      get_latest_voltage(), ble_is_connected() ? "connected" : "disconnected",
      ble_get_latest_trip_km());
}

static void print_calibration(void) {
  if (!throttle_is_calibrated()) {
    cli_write("calibrated: no\r\n");
    return;
  }

  uint32_t throttle_min = 0;
  uint32_t throttle_max = 0;
  throttle_get_calibration_values(&throttle_min, &throttle_max);
  serial_terminal_printf("calibrated: yes\r\n");
  serial_terminal_printf("throttle_min: %lu\r\n", throttle_min);
  serial_terminal_printf("throttle_max: %lu\r\n", throttle_max);
#ifdef CONFIG_TARGET_DUAL_THROTTLE
  uint32_t brake_min = 0;
  uint32_t brake_max = 0;
  brake_get_calibration_values(&brake_min, &brake_max);
  serial_terminal_printf("brake_min: %lu\r\n", brake_min);
  serial_terminal_printf("brake_max: %lu\r\n", brake_max);
#endif
}

static void execute_line(char *line) {
  char *cmd = trim(line);
  if (*cmd == '\0') {
    cli_write(CLI_PROMPT);
    return;
  }

  char *arg1 = cmd;
  while (*arg1 && *arg1 != ' ' && *arg1 != '\t') {
    arg1++;
  }
  if (*arg1) {
    *arg1++ = '\0';
    arg1 = trim(arg1);
  }

  if (streq(cmd, "help") || streq(cmd, "?")) {
    print_help();
  } else if (streq(cmd, "exit") || streq(cmd, "quit")) {
    cli_active = false;
    cli_write("Leaving CLI mode\r\n");
    return;
  } else if (streq(cmd, "version")) {
    serial_terminal_printf("GB Remote %s (%s %s) target=%s idf=%s\r\n",
                           FW_VERSION, BUILD_DATE, BUILD_TIME, TARGET_NAME,
                           esp_get_idf_version());
  } else if (streq(cmd, "status")) {
    print_status();
  } else if (streq(cmd, "dump")) {
    settings_registry_print_dump(print_cb, NULL);
  } else if (streq(cmd, "bms")) {
    print_bms_diagnostics();
  } else if (streq(cmd, "get")) {
    if (arg1 && *arg1) {
      settings_registry_print_matching(arg1, print_cb, NULL);
    } else {
      settings_registry_print_all(print_cb, NULL);
    }
  } else if (streq(cmd, "set")) {
    char *name = arg1;
    while (*arg1 && *arg1 != ' ' && *arg1 != '\t') {
      arg1++;
    }
    if (*arg1) {
      *arg1++ = '\0';
      arg1 = trim(arg1);
    }
    if (!name || !*name || !arg1 || !*arg1) {
      cli_write("ERR: usage: set <name> <value>\r\n");
    } else {
      char value[64];
      esp_err_t err = settings_registry_set(name, arg1, value, sizeof(value));
      if (err == ESP_OK) {
        serial_terminal_printf("%s = %s\r\nOK\r\n", name, value);
      } else {
        serial_terminal_printf("ERR: %s\r\n", esp_err_to_name(err));
      }
    }
  } else if (streq(cmd, "boot")) {
    if (power_get_mode() == POWER_MODE_CHARGING) {
      cli_write("Booting full app...\r\n");
      power_request_full_boot();
    } else {
      cli_write("Already in full mode\r\n");
    }
  } else if (streq(cmd, "reboot") || streq(cmd, "restart")) {
    cli_write("Restarting...\r\n");
    vTaskDelay(pdMS_TO_TICKS(100));
    esp_restart();
  } else if (streq(cmd, "calibration")) {
    if (streq(arg1, "show")) {
      print_calibration();
    } else {
      cli_write("ERR: usage: calibration show\r\n");
    }
  } else if (streq(cmd, "calibrate")) {
    if (power_get_mode() == POWER_MODE_CHARGING) {
      cli_write("ERR: not available in charging mode; use 'boot' first\r\n");
    } else {
      cli_write("Move throttle/brake through full range until calibration finishes...\r\n");
      calibration_result_t result = throttle_calibrate(NULL);
      if (result == CAL_OK) {
        cli_write("OK\r\n");
        print_calibration();
      } else {
        serial_terminal_printf("ERR: %s\r\n", cal_error_name(result));
      }
    }
  } else if (streq(cmd, "save")) {
    cli_write("OK: settings are saved immediately by 'set'\r\n");
  } else {
    serial_terminal_printf("ERR: unknown command '%s'\r\n", cmd);
  }

  if (cli_active) {
    cli_write(CLI_PROMPT);
  }
}

static void enter_cli(void) {
  cli_active = true;
  cli_write("\r\nGB Remote CLI\r\nType 'help' for commands.\r\n" CLI_PROMPT);
}

bool serial_terminal_is_active(void) { return cli_active; }

void serial_terminal_process_byte(uint8_t byte) {
  if (byte == '\r' || byte == '\n') {
    if (byte == '\n' && last_was_cr) {
      last_was_cr = false;
      return;
    }
    last_was_cr = (byte == '\r');
    line_buf[line_len] = '\0';
    line_len = 0;

    if (!cli_active) {
      char *line = trim(line_buf);
      if (streq(line, "cli") || streq(line, "terminal")) {
        enter_cli();
      }
      return;
    }

    cli_write("\r\n");
    execute_line(line_buf);
    return;
  }
  last_was_cr = false;

  if (byte == 0x08 || byte == 0x7F) {
    if (line_len > 0) {
      line_len--;
      if (cli_active) {
        cli_write("\b \b");
      }
    }
    return;
  }

  if (byte < 0x20 || byte > 0x7E) {
    return;
  }

  if (line_len < CLI_LINE_MAX - 1) {
    line_buf[line_len++] = (char)byte;
    if (cli_active) {
      char echo[2] = {(char)byte, '\0'};
      cli_write(echo);
    }
  } else if (cli_active) {
    cli_write("\r\nERR: line too long\r\n" CLI_PROMPT);
    line_len = 0;
  } else {
    line_len = 0;
  }
}
