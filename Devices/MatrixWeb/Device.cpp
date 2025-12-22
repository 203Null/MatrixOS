#include "Device.h"
#include "MatrixOS.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <unordered_map>

#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#endif

namespace
{
  auto g_start_time = std::chrono::steady_clock::now();
  std::recursive_mutex g_nvs_mutex;
  std::unordered_map<uint32_t, std::vector<char>> g_nvs;

  std::recursive_mutex g_led_mutex;
  std::vector<Color> g_last_frame;
  std::vector<Color> g_framebuffer;
  std::chrono::steady_clock::time_point g_last_print;

  bool FrameChanged(Color* frameBuffer, uint16_t count)
  {
    if (g_last_frame.size() != count)
    {
      return true;
    }
    for (uint16_t i = 0; i < count; i++)
    {
      if (g_last_frame[i] != frameBuffer[i])
      {
        return true;
      }
    }
    return false;
  }

  void UpdateFrameBuffer(Color* frameBuffer, uint16_t count)
  {
    if (g_framebuffer.size() != count)
    {
      g_framebuffer.assign(frameBuffer, frameBuffer + count);
      return;
    }
    std::memcpy(g_framebuffer.data(), frameBuffer, sizeof(Color) * count);
  }
}

namespace Device
{
  string serial_number = "WEB-0000";

  void DeviceInit()
  {
    KeyPad::Clear();
  }

  void DeviceStart()
  {
  }

  void Reboot()
  {
    std::fprintf(stderr, "[MatrixWeb] Reboot requested\n");
  }

  void Bootloader()
  {
    std::fprintf(stderr, "[MatrixWeb] Bootloader requested\n");
  }

  void ErrorHandler()
  {
    std::fprintf(stderr, "[MatrixWeb] Error handler invoked\n");
  }

  uint64_t Micros()
  {
    auto now = std::chrono::steady_clock::now();
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(now - g_start_time).count();
    return static_cast<uint64_t>(us);
  }

  void DeviceSettings()
  {
  }

  void Log(string &format, va_list &valst)
  {
    std::vprintf(format.c_str(), valst);
    std::fflush(stdout);
  }

  string GetSerial()
  {
    return serial_number;
  }

  namespace LED
  {
    void Update(Color* frameBuffer, vector<uint8_t>& brightness)
    {
      (void)brightness;

      if (!frameBuffer || count == 0)
      {
        return;
      }

      std::lock_guard<std::recursive_mutex> lock(g_led_mutex);

      UpdateFrameBuffer(frameBuffer, count);

#if defined(__EMSCRIPTEN__)
      return;
#endif

      if (!FrameChanged(frameBuffer, count))
      {
        return;
      }

      auto now = std::chrono::steady_clock::now();
      if (now - g_last_print < std::chrono::milliseconds(100))
      {
        return;
      }
      g_last_print = now;

      g_last_frame.assign(frameBuffer, frameBuffer + count);

      std::printf("\n");
      for (int16_t y = 0; y < Y_SIZE; y++)
      {
        for (int16_t x = 0; x < X_SIZE; x++)
        {
          uint16_t index = XY2Index(Point(x, y));
          const Color& color = frameBuffer[index];
          char pixel = (color.R || color.G || color.B || color.W) ? '#' : '.';
          std::printf("%c", pixel);
        }
        std::printf("\n");
      }
      std::printf("\n");
      std::fflush(stdout);
    }

    uint16_t XY2Index(Point xy)
    {
      if (!xy)
      {
        return UINT16_MAX;
      }
      if (xy.x < 0 || xy.y < 0 || xy.x >= X_SIZE || xy.y >= Y_SIZE)
      {
        return UINT16_MAX;
      }
      return static_cast<uint16_t>(xy.y * X_SIZE + xy.x);
    }

    uint16_t ID2Index(uint16_t ledID)
    {
      if (ledID >= (X_SIZE * Y_SIZE))
      {
        return UINT16_MAX;
      }
      return ledID;
    }

    Point Index2XY(uint16_t index)
    {
      if (index >= (X_SIZE * Y_SIZE))
      {
        return Point::Invalid();
      }
      return Point(static_cast<int16_t>(index % X_SIZE), static_cast<int16_t>(index / X_SIZE));
    }
  }

  namespace KeyPad
  {
    static KeyInfo fnState;
    static KeyInfo keypadState[X_SIZE][Y_SIZE];
    static KeyInfo invalidKey;
    static bool keypadPressed[X_SIZE][Y_SIZE] = {};
    static bool fnPressed = false;
    static KeyConfig binary_config = {
      .apply_curve = false,
      .low_threshold = 0,
      .high_threshold = 65535,
      .activation_offset = 0,
      .debounce = 0,
    };

    static bool NotifyOS(uint16_t keyID, KeyInfo* keyInfo)
    {
      KeyEvent keyEvent;
      keyEvent.id = keyID;
      keyEvent.info = *keyInfo;
#if defined(__EMSCRIPTEN__)
      std::printf("[MatrixWeb] Key event id=%u state=%u\n", keyID, static_cast<unsigned>(keyInfo->State()));
      std::fflush(stdout);
#endif
      return MatrixOS::KeyPad::NewEvent(&keyEvent);
    }

    static void UpdateKey(uint8_t x, uint8_t y, bool pressed)
    {
      if (x >= X_SIZE || y >= Y_SIZE)
      {
        return;
      }

      keypadPressed[x][y] = pressed;
      Fract16 value = pressed ? Fract16(FRACT16_MAX) : Fract16(0);
      if (keypadState[x][y].Update(binary_config, value))
      {
        uint16_t keyID = XY2ID(Point(x, y));
        if (keyID != UINT16_MAX)
        {
          NotifyOS(keyID, &keypadState[x][y]);
        }
      }
    }

    static void UpdateFn(bool pressed)
    {
      fnPressed = pressed;
      Fract16 value = pressed ? Fract16(FRACT16_MAX) : Fract16(0);
      if (fnState.Update(binary_config, value))
      {
        NotifyOS(0, &fnState);
      }
    }

    static void TickPressedKeys()
    {
      if (fnPressed)
      {
        UpdateFn(true);
      }

      for (uint8_t x = 0; x < X_SIZE; x++)
      {
        for (uint8_t y = 0; y < Y_SIZE; y++)
        {
          if (!keypadPressed[x][y])
          {
            continue;
          }
          UpdateKey(x, y, true);
        }
      }
    }

    KeyInfo* GetKey(uint16_t keyID)
    {
      uint8_t keyClass = keyID >> 12;
      switch (keyClass)
      {
        case 0:
        {
          uint16_t index = keyID & 0x0FFF;
          if (index == 0)
          {
            return &fnState;
          }
          break;
        }
        case 1:
        {
          int16_t x = (keyID & 0b0000111111000000) >> 6;
          int16_t y = keyID & 0b0000000000111111;
          if (x < X_SIZE && y < Y_SIZE)
          {
            return &keypadState[x][y];
          }
          break;
        }
      }
      return &invalidKey;
    }

    void Clear()
    {
      fnState.Clear();
      fnPressed = false;
      for (uint16_t x = 0; x < X_SIZE; x++)
      {
        for (uint16_t y = 0; y < Y_SIZE; y++)
        {
          keypadState[x][y].Clear();
          keypadPressed[x][y] = false;
        }
      }
    }

    uint16_t XY2ID(Point xy)
    {
      if (xy.x >= 0 && xy.x < X_SIZE && xy.y >= 0 && xy.y < Y_SIZE)
      {
        return static_cast<uint16_t>((1 << 12) + (xy.x << 6) + xy.y);
      }
      return UINT16_MAX;
    }

    Point ID2XY(uint16_t keyID)
    {
      uint8_t keyClass = keyID >> 12;
      if (keyClass == 1)
      {
        int16_t x = (keyID & 0b0000111111000000) >> 6;
        int16_t y = keyID & 0b0000000000111111;
        if (x < X_SIZE && y < Y_SIZE)
        {
          return Point(x, y);
        }
      }
      return Point::Invalid();
    }
  }

  namespace NVS
  {
    size_t Size(uint32_t hash)
    {
      std::lock_guard<std::recursive_mutex> lock(g_nvs_mutex);
      auto it = g_nvs.find(hash);
      if (it == g_nvs.end())
      {
        return 0;
      }
      return it->second.size();
    }

    vector<char> Read(uint32_t hash)
    {
      std::lock_guard<std::recursive_mutex> lock(g_nvs_mutex);
      auto it = g_nvs.find(hash);
      if (it == g_nvs.end())
      {
        return {};
      }
      return it->second;
    }

    bool Write(uint32_t hash, void* pointer, uint16_t length)
    {
      std::lock_guard<std::recursive_mutex> lock(g_nvs_mutex);
      std::vector<char> data(length);
      if (length > 0 && pointer)
      {
        std::memcpy(data.data(), pointer, length);
      }

      auto it = g_nvs.find(hash);
      if (it != g_nvs.end() && it->second == data)
      {
        return true;
      }

      g_nvs[hash] = std::move(data);
      return true;
    }

    bool Delete(uint32_t hash)
    {
      std::lock_guard<std::recursive_mutex> lock(g_nvs_mutex);
      return g_nvs.erase(hash) > 0;
    }

    void Clear()
    {
      std::lock_guard<std::recursive_mutex> lock(g_nvs_mutex);
      g_nvs.clear();
    }
  }
}

#if defined(__EMSCRIPTEN__)
extern "C" {
EMSCRIPTEN_KEEPALIVE uintptr_t MatrixOS_Wasm_GetFrameBuffer()
{
  std::lock_guard<std::recursive_mutex> lock(g_led_mutex);
  if (g_framebuffer.empty())
  {
    return 0;
  }
  return reinterpret_cast<uintptr_t>(g_framebuffer.data());
}

EMSCRIPTEN_KEEPALIVE uint32_t MatrixOS_Wasm_GetFrameBufferByteLength()
{
  std::lock_guard<std::recursive_mutex> lock(g_led_mutex);
  return static_cast<uint32_t>(g_framebuffer.size() * sizeof(Color));
}

EMSCRIPTEN_KEEPALIVE uint32_t MatrixOS_Wasm_GetWidth()
{
  return X_SIZE;
}

EMSCRIPTEN_KEEPALIVE uint32_t MatrixOS_Wasm_GetHeight()
{
  return Y_SIZE;
}

EMSCRIPTEN_KEEPALIVE void MatrixOS_Wasm_KeyEvent(uint8_t x, uint8_t y, uint8_t pressed)
{
  Device::KeyPad::UpdateKey(x, y, pressed != 0);
}

EMSCRIPTEN_KEEPALIVE void MatrixOS_Wasm_FnEvent(uint8_t pressed)
{
  Device::KeyPad::UpdateFn(pressed != 0);
}

EMSCRIPTEN_KEEPALIVE void MatrixOS_Wasm_KeypadTick()
{
  Device::KeyPad::TickPressedKeys();
}
}
#endif
