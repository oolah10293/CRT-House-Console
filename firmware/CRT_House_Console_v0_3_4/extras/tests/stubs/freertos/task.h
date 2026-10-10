#pragma once
#include "FreeRTOS.h"
using TaskHandle_t=void*;
inline int xTaskCreatePinnedToCore(void(*)(void*),const char*,int,void*,int,void**,int){return 0;}
inline void vTaskDelay(unsigned){}
