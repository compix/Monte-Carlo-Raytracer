#pragma once
#include "glm/glm.hpp"

namespace VCT
{
    struct VisualizationSettings;
    struct GISettings;
    struct DebugSettings;
    struct DemoSettings;
}

#define VOXEL_RESOLUTION 128
#define CLIP_REGION_COUNT 6
#define FACE_COUNT 6
#define MAX_DIR_LIGHT_COUNT 3

const glm::vec3 MAIN_AXES[]
{
    glm::vec3(1.0f, 0.0f, 0.0f),
    glm::vec3(0.0f, 1.0f, 0.0f),
    glm::vec3(0.0f, 0.0f, 1.0f)
};

namespace VCT
{
    extern VisualizationSettings VISUALIZATION_SETTINGS;
    extern GISettings GI_SETTINGS;
    extern DebugSettings DEBUG_SETTINGS;
    extern DemoSettings DEMO_SETTINGS;
}
