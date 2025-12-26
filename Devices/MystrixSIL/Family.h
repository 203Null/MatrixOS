// Declare family-specific settings for the host/web build.
#pragma once

#include "Device.h"
#include "Framework.h"

// Family-specific defines
#define GRID_TYPE_8x8
#define FAMILY_MYSTRIXSIL
#define MULTIPRESS 10

#define DEVICE_SAVED_VAR_SCOPE "Device"

struct DeviceInfo {
  char Model[4];
  char Revision[4];
  uint8_t ProductionYear;
  uint8_t ProductionMonth;
};

namespace Device
{
  inline DeviceInfo deviceInfo = {{'M', 'X', '1', 'S'}, {'W', 'E', 'B', '0'}, 25, 1};

  namespace KeyPad
  {
    inline bool velocity_sensitivity = false;
  }
}
