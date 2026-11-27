#pragma once

// Tracy instrumentation; compiles to nothing unless built with -DVOXETERRA_TRACY=ON.
#include <vulkan/vulkan.h>

#ifdef TRACY_ENABLE
#include <tracy/Tracy.hpp>
#include <tracy/TracyVulkan.hpp>
#else
#define ZoneScoped
#define ZoneScopedN(name)
#define FrameMark
#define TracyPlot(name, value)
#define TracyAllocN(ptr, size, name)
#define TracyFreeN(ptr, name)
#define TracyVkZone(ctx, cmdbuf, name)
#define TracyVkCollect(ctx, cmdbuf)
using TracyVkCtx = void*;
#endif
