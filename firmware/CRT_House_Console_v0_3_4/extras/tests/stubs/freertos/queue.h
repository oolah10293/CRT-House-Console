#pragma once
#include "FreeRTOS.h"
struct FakeQueue{size_t size;bool ready;void* data;};using QueueHandle_t=FakeQueue*;
inline QueueHandle_t xQueueCreate(int,size_t n){auto q=new FakeQueue{n,false,malloc(n)};return q;}
inline int xQueueOverwrite(QueueHandle_t q,const void* d){if(q){memcpy(q->data,d,q->size);q->ready=true;}return 1;}
inline int xQueueReceive(QueueHandle_t q,void* d,int){if(q&&q->ready){memcpy(d,q->data,q->size);q->ready=false;return 1;}return 0;}
