#pragma once

#include "MatrixOS.h"
#include "UI/UI.h"

class BrightnessControl : public UI {
 public:
  // string name = "Setting";
  // Color nameColor = Color(0x00FFFF);

  uint8_t* brightness_map;
  uint16_t brightness_map_length;
  uint16_t threshold;

  const Point origin = Point((Device::x_size - 1) / 2, (Device::y_size - 1) / 2);

  void Start();
};
