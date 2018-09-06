#pragma once
#include <vector>
#include "engine/gui/GUIElements.h"
#include "engine/gui/GUISettings.h"

struct ShadowSettings : GUISettings
{
    ShadowSettings() { guiElements.insert(guiElements.end(), {&usePoissonFilter, &depthBias}); }

    CheckBox usePoissonFilter{"Use Poisson Filter", true};
    uint32_t shadowMapResolution{4096};
    SliderFloat depthBias{"Depth Bias", 0.013f, 0.0001f, 0.1f, "%.6f"};
};

struct RenderingSettings : GUISettings
{
    RenderingSettings() { guiElements.insert(guiElements.end(), {&wireFrame, &cullBackFaces, &brdfMode }); }

    CheckBox wireFrame{"Wireframe", false};
    CheckBox cullBackFaces{ "Cull Back Faces", false };
    ComboBox brdfMode = ComboBox("BRDF", { "Blinn-Phong", "Cook-Torrance"}, 1);
};

enum class SceneVisualizationMode
{
	DefaultLit,
	DiffuseTextureDisplay,
	NormalMapDisplay,
	SpecularMapDisplay,
	EmissionMapDisplay,
	DepthTextureDisplay,
	GeometryNormalDisplay,
	UVMapDisplay
};

struct SceneVisualizationSettings : GUISettings
{
	SceneVisualizationSettings()
	{
		guiElements.insert(guiElements.end(), { &visualizationMode });
	}

	ComboBoxEnum<SceneVisualizationMode> visualizationMode{"Visualization Mode", 
		{"DefaultLit", "DiffuseTextureDisplay", "NormalMapDisplay", 
		 "SpecularMapDisplay", "EmissionMapDisplay", "DepthTextureDisplay", "GeometryNormalDisplay", "UVMapDisplay"}, 0};
};

extern ShadowSettings SHADOW_SETTINGS;
extern RenderingSettings RENDERING_SETTINGS;
extern SceneVisualizationSettings SCENE_VISUALIZATION_SETTINGS;
