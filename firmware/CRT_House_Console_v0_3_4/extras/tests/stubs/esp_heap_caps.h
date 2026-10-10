#pragma once
#include <cstdint>
#include <cstddef>
constexpr uint32_t MALLOC_CAP_8BIT=4;
static size_t mockHeapFree=40000,mockHeapBlock=20000;
using MockAllocHook=void(*)(size_t,uint32_t,const char*);
static MockAllocHook mockAllocHook=nullptr;
inline size_t heap_caps_get_free_size(uint32_t){return mockHeapFree;}
inline size_t heap_caps_get_largest_free_block(uint32_t){return mockHeapBlock;}
inline int heap_caps_register_failed_alloc_callback(MockAllocHook cb){mockAllocHook=cb;return 0;}
