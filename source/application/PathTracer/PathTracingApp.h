#pragma once
#include <engine/Application.h>
#include <engine/input/Input.h>
#include "engine/util/Timer.h"
#include "engine/ecs/EntityManager.h"
#include "engine/rendering/Texture2D.h"
#include "engine/rendering/renderer/SimpleMeshRenderer.h"
#include <memory>
#include "engine/rendering/shader/Shader.h"
#include "GUI/PathTracingGUI.h"
#include "engine/rendering/architecture/RenderPipeline.h"
#include "radeon_rays.h"
#include "radeon_rays_cl.h"
#include "CLW.h"
#include "GUI/PathTracingSettings.h"

#define SCENE_COUNT 5

class PathTracingApp : public Application, InputHandler
{
    struct SceneDesc
    {
        std::string path;
        std::string texturePath;
        float scale{0.0f};
        glm::vec3 cameraPos;
        glm::vec3 cameraEulerAngles;
        glm::vec3 dirLightEulerAngles;
		bool isRelativePath = true;
		bool isRelativeTexPath = true;
		bool isEmpty = false;

        SceneDesc() {}
        SceneDesc(const std::string& path, const std::string& texPath, float scale, glm::vec3 camPos, glm::vec3 camEuler, glm::vec3 dirLightEuler)
            :path(path), texturePath(texPath), scale(scale), cameraPos(camPos), cameraEulerAngles(camEuler), dirLightEulerAngles(dirLightEuler) {}
    };
public:
	PathTracingApp();

    void update() override;
    void initUpdate() override;

    void startPathTracing();
protected:
    void onKeyDown(SDL_Keycode keyCode) override;


	virtual void onMousewheel(float delta) override;

private:
	void resize(int width, int height);
    void createDemoScene();

    void loadScene(const SceneDesc& sceneDesc);
protected:
	void quit() override;
	void onWindowEvent(const SDL_WindowEvent& windowEvent) override;

	void showRNGAndSobol();
private:
	void initRadeonRaysAPI();
	void refreshPipeline();

	EPathTracerPipeline m_currentPipelineType = PathTracerSettings::PIPELINE.pipeline.getEnumValue();
    std::unique_ptr<RenderPipeline> m_rasterRenderPipeline;
	std::unique_ptr<RenderPipeline> m_pathTracerRenderPipeline;
	std::unique_ptr<RenderPipeline> m_bdptRenderPipeline;

	std::shared_ptr<SimpleMeshRenderer> m_fullscreenQuadRenderer;
	std::shared_ptr<Shader> m_fullscreenQuadShader;
	std::unique_ptr<PathTracingGUI> m_gui;
    ComponentPtr<Transform> m_sceneRoot;
    bool m_guiEnabled{ true };
    std::shared_ptr<Shader> m_forwardShader;

    bool m_allowPause{ false };
    int m_screenTexDiv{ 1 };
    int m_selectedSceneIdx{ 0 };
    SceneDesc m_scenes[SCENE_COUNT];

	bool m_clInitialized = false;

	std::shared_ptr<AsyncFuture<std::shared_ptr<Model>>> m_modelFuture;
	std::vector<std::function<ProgressState()>> m_parallelCommands;
};
