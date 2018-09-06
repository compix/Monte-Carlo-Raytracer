#pragma once
#include <CLW.h>
#include <radeon_rays.h>

extern CLWContext g_clContext;
extern RadeonRays::IntersectionApi* g_isectApi;
extern int g_frameIndex;
extern float g_totalRenderTime;
extern bool g_requestedPause;