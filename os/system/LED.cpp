#include "MatrixOS.h"

namespace MatrixOS::LED
{
    // static timer
    StaticTimer_t led_tmdef;
    TimerHandle_t led_tm;
    
    bool needUpate = false;

    void LEDTimerCallback( TimerHandle_t xTimer )
    {
        if(needUpate)
        {
            Device::LED::Update(frameBuffers[currentLayer], UserVar::brightness);
            needUpate = false;
        }
    }



    void Init()
    {
        Color* frameBuffer = (Color*)pvPortMalloc(Device::numsOfLED * sizeof(Color));
        for(uint32_t i = 0; i < Device::numsOfLED; i++)
        {
            frameBuffer[i] = Color(0);
        }

        currentLayer = 0;
        frameBuffers.push_back(frameBuffer);
        Update();

        led_tm = xTimerCreateStatic(NULL, configTICK_RATE_HZ / Device::fps, true, NULL, LEDTimerCallback, &led_tmdef);
        xTimerStart(led_tm, 0);
    }

    void SetColor(Point xy, Color color, uint8_t layer)
    {
        // MatrixOS::Logging::LogVerbose("LED", "Set Color %d %d", xy.x, xy.y);
        xy = xy.Rotate(UserVar::rotation, Point(Device::x_size, Device::y_size));
        if(layer > currentLayer)
        {
            MatrixOS::SYS::ErrorHandler("LED Layer Unavailable");
            return;
        }
        uint16_t index = Device::LED::XY2Index(xy);
        if(index == UINT16_MAX) return;
        frameBuffers[layer][index] = color;
    }

    void SetColor(uint16_t ID, Color color, uint8_t layer)
    {
        if(layer > currentLayer)
        {
            MatrixOS::SYS::ErrorHandler("LED Layer Unavailable");
                return;
            }
        uint16_t index = Device::LED::ID2Index(ID);
        if(index == UINT16_MAX) return;
        uint16_t bufferIndex = index + Device::numsOfLED * layer;
        frameBuffers[layer][bufferIndex] = color;

    }

    void Fill(Color color, uint8_t layer)
    {
        if(layer > currentLayer)
        {
            MatrixOS::SYS::ErrorHandler("LED Layer Unavailable");
            return;
        }
        for(uint16_t index = 0; index < Device::numsOfLED; index++)
        {
            frameBuffers[layer][index] = color;
        }
    }

    void Update(int8_t layer) 
    {
        if(layer == currentLayer)
            needUpate = true;
    }

    // void SwitchLayer(uint8_t layer)
    // {
    //     if(layer >= LED_LAYERS)
    //     {
    //         MatrixOS::SYS::ErrorHandler("LED Layer Unavailable");
    //         return;
    //     }
    //     currentLayer = layer;
    // }

    int8_t CreateLayer()
    {
        if(currentLayer >= MAX_LED_LAYERS - 1)
            return -1;
        Color* frameBuffer = (Color*)pvPortMalloc(Device::numsOfLED *sizeof(Color));
        for(uint32_t i = 0; i < Device::numsOfLED; i++)
        {
            frameBuffer[i] = Color(0);
        }
        currentLayer++;
        frameBuffers.push_back(frameBuffer);
        return currentLayer;
    }

    void DestoryLayer()
    {
        if(currentLayer > 0)
        {
            vPortFree(frameBuffers.back());
            frameBuffers.pop_back();
        }
        else
        {
            Fill(0);
        }
    }

    void ShiftCanvas(EDirection direction, int8_t distance, int8_t layer)
    {
        // Color[] tempBuffer;
    }

    void RotateCanvas(EDirection direction, int8_t layer)
    {
        
    }
}