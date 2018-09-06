#include "rt_globals.h"

CLWContext g_clContext;
RadeonRays::IntersectionApi* g_isectApi;
int g_frameIndex;
float g_totalRenderTime;
bool g_requestedPause;