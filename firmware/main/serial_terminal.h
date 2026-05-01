#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

bool serial_terminal_is_active(void);
void serial_terminal_process_byte(uint8_t byte);
void serial_terminal_printf(const char *fmt, ...);

#ifdef __cplusplus
}
#endif
