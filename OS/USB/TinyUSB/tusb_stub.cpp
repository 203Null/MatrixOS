#include "tusb_stub.h"

namespace
{
  bool g_connected = true;
}

extern "C" {

void tusb_init(void)
{
}

void tud_task(void)
{
}

bool tud_disconnect(void)
{
  g_connected = false;
  return true;
}

bool tud_connect(void)
{
  g_connected = true;
  return true;
}

bool tud_ready(void)
{
  return g_connected;
}

bool tud_cdc_n_connected(uint8_t itf)
{
  (void)itf;
  return g_connected;
}

uint32_t tud_cdc_n_available(uint8_t itf)
{
  (void)itf;
  return 0;
}

uint32_t tud_cdc_n_write_available(uint8_t itf)
{
  (void)itf;
  return 64;
}

uint32_t tud_cdc_n_write_char(uint8_t itf, char c)
{
  (void)itf;
  (void)c;
  return 1;
}

void tud_cdc_n_write_flush(uint8_t itf)
{
  (void)itf;
}

int32_t tud_cdc_n_read_char(uint8_t itf)
{
  (void)itf;
  return -1;
}

uint32_t tud_cdc_n_read(uint8_t itf, void* buffer, uint32_t length)
{
  (void)itf;
  (void)buffer;
  (void)length;
  return 0;
}

bool tud_hid_ready(void)
{
  return g_connected;
}

bool tud_hid_report(uint8_t report_id, const void* report, uint16_t len)
{
  (void)report_id;
  (void)report;
  (void)len;
  return true;
}

bool tud_hid_n_report(uint8_t itf, uint8_t report_id, const void* report, uint16_t len)
{
  (void)itf;
  (void)report_id;
  (void)report;
  (void)len;
  return true;
}

bool tud_hid_n_ready(uint8_t itf)
{
  (void)itf;
  return g_connected;
}

bool tud_suspended(void)
{
  return false;
}

bool tud_remote_wakeup(void)
{
  return true;
}

uint32_t tud_midi_stream_write(uint8_t cable_num, uint8_t const* buffer, uint32_t bufsize)
{
  (void)cable_num;
  (void)buffer;
  return bufsize;
}

bool tud_midi_n_packet_read(uint8_t itf, uint8_t packet[4])
{
  (void)itf;
  (void)packet;
  return false;
}

}  // extern "C"
