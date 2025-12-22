#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
  HID_REPORT_TYPE_INPUT = 1,
  HID_REPORT_TYPE_OUTPUT = 2,
  HID_REPORT_TYPE_FEATURE = 3
} hid_report_type_t;

enum
{
  REPORT_ID_KEYBOARD = 1,
  REPORT_ID_MOUSE,
  REPORT_ID_CONSUMER_CONTROL,
  REPORT_ID_GAMEPAD,
  REPORT_ID_VENDER
};

void tusb_init(void);
void tud_task(void);
bool tud_disconnect(void);
bool tud_connect(void);
bool tud_ready(void);

bool tud_cdc_n_connected(uint8_t itf);
uint32_t tud_cdc_n_available(uint8_t itf);
uint32_t tud_cdc_n_write_available(uint8_t itf);
uint32_t tud_cdc_n_write_char(uint8_t itf, char c);
void tud_cdc_n_write_flush(uint8_t itf);
int32_t tud_cdc_n_read_char(uint8_t itf);
uint32_t tud_cdc_n_read(uint8_t itf, void* buffer, uint32_t length);

bool tud_hid_ready(void);
bool tud_hid_report(uint8_t report_id, const void* report, uint16_t len);
bool tud_hid_n_report(uint8_t itf, uint8_t report_id, const void* report, uint16_t len);
bool tud_hid_n_ready(uint8_t itf);

bool tud_suspended(void);
bool tud_remote_wakeup(void);

uint32_t tud_midi_stream_write(uint8_t cable_num, uint8_t const* buffer, uint32_t bufsize);
bool tud_midi_n_packet_read(uint8_t itf, uint8_t packet[4]);

#ifdef __cplusplus
}
#endif
