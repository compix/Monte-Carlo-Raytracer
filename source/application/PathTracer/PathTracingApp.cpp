#include "PathTracingApp.h"
#include <engine/util/Random.h>
#include <engine/util/Timer.h>
#include <engine/rendering/util/GLUtil.h>
#include <engine/Engine.h>
#include <engine/resource/ResourceManager.h>
#include <engine/rendering/architecture/RenderPipeline.h>
#include <engine/ecs/ecs.h>
#include "engine/rendering/debug/DebugRenderer.h"
#include "engine/util/ECSUtil/ECSUtil.h"
#include "engine/rendering/renderPasses/ShadowMapPass.h"
#include "engine/util/commands/RotationCommand.h"
#include "engine/rendering/renderer/MeshRenderers.h"
#include "engine/rendering/Screen.h"
#include "engine/rendering/renderPasses/ForwardScenePass.h"
#include "engine/rendering/lights/DirectionalLight.h"
#include "engine/globals.h"
#include "engine/util/NamingConvention.h"
#include "GUI/PathTracingSettings.h"
#include "engine/rendering/settings/RenderingSettings.h"
#include "radeon_rays.h"
#include "radeon_rays_cl.h"
#include "CLW.h"
#include "engine/rendering/renderPasses/SceneGeometryPass.h"
#include "Raytracing/system/PlatformManager.h"
#include "engine/camera/FreeCameraViewController.h"
#include "engine/util/file.h"
#include "SOIL2.h"
#include "Raytracing/system/KernelManager.h"
#include "../../engine/util/colors.h"
#include "Raytracing/scene/HostScene.h"
#include "Raytracing/scene/RTScene.h"
#include "Raytracing/util/CLHelper.h"
#include "Raytracing/textures/RTTextures.h"
#include "Raytracing/rt_globals.h"
#include "Raytracing/kernels/RTNoiseGenerationKernel.h"
#include "Raytracing/renderPasses/RTPrimaryRaysPass.h"
#include "Raytracing/renderPasses/RTPathTracingPass.h"
#include "Raytracing/sampling/sobol.h"
#include <engine/rendering/lights/PointLight.h>
#include "Raytracing/renderPasses/RTBDPTPass.h"
#include "../../engine/util/ECSUtil/EntityCreator.h"
#include "../../engine/rendering/lights/DiskAreaLight.h"
#include "Raytracing/renderPasses/RTDisplayPass.h"
#include "Raytracing/renderPasses/RTDenoisePass.h"
#include "Raytracing/renderPasses/RTToneMappingPass.h"
#include "../../engine/rendering/renderPasses/SceneVisualizationPass.h"
#include "raytracing/renderPasses/RTReconstructionPass.h"
#include "raytracing/sampling/sampling.h"

void PathTracingApp::initRadeonRaysAPI()
{
	if (m_clInitialized)
		return;

	try 
	{
		g_clContext = PlatformManager::createCLContextWithGLInterop();

		// To make sure the kernels compile just fine:
		auto program = KernelManager::getProgram("PathTracing", g_clContext);
		auto bdptProgram = KernelManager::getProgram("BDPT", g_clContext);
		auto tonemappingProgram = KernelManager::getProgram("ToneMapping", g_clContext);
		auto reconstructionProgram = KernelManager::getProgram("Reconstruction", g_clContext);
		auto denoiseProgram = KernelManager::getProgram("Denoise", g_clContext);
	}
	catch (std::exception e)
	{
		LOG_ERROR(e.what());
		return;
	}

	// Create API with active device
	cl_device_id deviceId = PlatformManager::getActiveDevice()->GetID();
	cl_command_queue queue = g_clContext.GetCommandQueue(0);

	// Create intersection API
	g_isectApi = RadeonRays::CreateFromOpenClContext(g_clContext, deviceId, queue);

	m_clInitialized = true;
}

void PathTracingApp::refreshPipeline()
{
	if (m_currentPipelineType != PathTracerSettings::PIPELINE.pipeline.getEnumValue())
	{
		m_currentPipelineType = PathTracerSettings::PIPELINE.pipeline.getEnumValue();

		if (m_currentPipelineType != EPathTracerPipeline::Rasterization && !m_clInitialized)
		{
			startPathTracing();
		}
		
		switch (m_currentPipelineType)
		{
		case EPathTracerPipeline::Rasterization:
			CurRenderPipeline = m_rasterRenderPipeline.get();
			break;
		case EPathTracerPipeline::RegularPathTracer:
			CurRenderPipeline = m_pathTracerRenderPipeline.get();
			break;
		case EPathTracerPipeline::BidirectionalPathTracer:
			CurRenderPipeline = m_bdptRenderPipeline.get();
			break;
		default:
			break;
		}
	}
}

PathTracingApp::PathTracingApp()
{
    Input::subscribe(this);

    Random::randomize();
}

void PathTracingApp::initUpdate()
{
	PathTracerSettings::GI.imageResolution.value.x = Screen::getWidth();
	PathTracerSettings::GI.imageResolution.value.y = Screen::getHeight();

	// Initialize the PlatformManager by creating the platforms 
	PlatformManager::init();

	g_frameIndex = 0;

	ResourceManager::setShaderIncludePath("shaders");
	m_fullscreenQuadRenderer = MeshRenderers::fullscreenQuad();
	m_fullscreenQuadShader = ResourceManager::getShader("shaders/simple/fullscreenQuad.vert", "shaders/simple/fullscreenQuad.frag");

	createDemoScene();

	DebugRenderer::init();
	m_engine->setAllowPause(m_allowPause);

	// Create the raster pipeline for simple scene visualization at start-up.
	// The user can switch to a GI pipeline in the GUI.
	m_rasterRenderPipeline = std::make_unique<RenderPipeline>(MainCamera);
	CurRenderPipeline = m_rasterRenderPipeline.get();

	m_rasterRenderPipeline->addRenderPasses(
		std::make_shared<SceneGeometryPass>(),
		std::make_shared<ShadowMapPass>(SHADOW_SETTINGS.shadowMapResolution),
		std::make_shared<SceneVisualizationPass>());

	m_gui = std::make_unique<PathTracingGUI>(m_rasterRenderPipeline.get(), this);
	m_gui->setDefaultShader(m_forwardShader);

	Screen::addResizeListener([]() {
		PathTracerSettings::GI.imageResolution.value = glm::ivec2(Screen::getWidth(), Screen::getHeight());
	});

	m_initializing = false;
}

void PathTracingApp::resize(int width, int height)
{

}


void PathTracingApp::onWindowEvent(const SDL_WindowEvent& windowEvent)
{
	switch (windowEvent.event)
	{
	case SDL_WINDOWEVENT_RESIZED:
		resize(windowEvent.data1, windowEvent.data2);
		break;
	default: break;
	}
}

void PathTracingApp::quit()
{
}

void PathTracingApp::update()
{
	refreshPipeline();

	for (int i = static_cast<int>(m_parallelCommands.size()) - 1; i >= 0; --i)
	{
		if (m_parallelCommands[i]() != ProgressState::InProgress)
			m_parallelCommands.erase(m_parallelCommands.begin() + i);
	}

	MainCamera->getComponent<FreeCameraViewController>()->movementSpeed = PathTracerSettings::DEMO.cameraSpeed;

    glFrontFace(GL_CW);

    GL::setViewport(MainCamera->getViewport());

    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    DebugRenderInfo info(MainCamera->view(), MainCamera->proj(), MainCamera->getPosition());
    DebugRenderer::begin(info);

    DebugRenderer::update();

    glEnable(GL_DEPTH_TEST);

    ECS::lateUpdate();

    DebugRenderer::end();

	// Jitter for TAA
	if (PathTracerSettings::GI.useTAA && !MainCamera->getComponent<FreeCameraViewController>()->bMovedInLastUpdate)
	{
		glm::vec2 r(PathTracerSettings::GI.filterSettings.radius);
		glm::vec2 pixelOffset(math::lerp(-r.x, r.x, Sampler::sobolSample(g_frameIndex, 0, 0)),
			math::lerp(-r.y, r.y, Sampler::sobolSample(g_frameIndex, 1, 0)));
		PathTracerSettings::GI.filterSettings.curPixelOffset = pixelOffset;
	}
	
	if (CurRenderPipeline)
		CurRenderPipeline->update();

    if (m_guiEnabled)
	    m_gui->update();
}

void PathTracingApp::startPathTracing()
{
	if (!m_clInitialized)
	{
		initRadeonRaysAPI();

		if (!m_clInitialized)
			return;

		ECS::addSystem<RTScene>(g_isectApi, g_clContext);

		// Create shared pipeline passes
		auto denoisePass = std::make_shared<RTDenoisePass>();
		auto toneMapPass = std::make_shared<RTToneMappingPass>();
		auto displayPass = std::make_shared<RTDisplayPass>();

		m_bdptRenderPipeline = std::make_unique<RenderPipeline>(MainCamera);
		m_bdptRenderPipeline->addRenderPasses(
			std::make_shared<RTBDPTPass>(),
			std::make_shared<RTReconstructionPass>(),
			denoisePass,
			toneMapPass,
			displayPass);

		m_pathTracerRenderPipeline = std::make_unique<RenderPipeline>(MainCamera);
		m_pathTracerRenderPipeline->addRenderPasses(
			std::make_shared<RTPrimaryRaysPass>(),
			std::make_shared<RTPathTracingPass>(),
			std::make_shared<RTReconstructionPass>(),
			denoisePass,
			toneMapPass,
			displayPass);
	}
}

void PathTracingApp::onKeyDown(SDL_Keycode keyCode)
{
    switch (keyCode)
    {
    case SDLK_F2:
		if (m_clInitialized)
		{
			KernelManager::recompilePrograms(g_clContext);
			m_bdptRenderPipeline->getRenderPass<RTReconstructionPass>()->clearFrameTextures();
			m_pathTracerRenderPipeline->getRenderPass<RTReconstructionPass>()->clearFrameTextures();
		}
        break;
    case SDLK_F3:
        m_guiEnabled = !m_guiEnabled;
        break;
    case SDLK_F5:
        m_engine->requestScreenshot();
        break;
    case SDLK_j:
		PathTracerSettings::GI.useTAA.value = !PathTracerSettings::GI.useTAA.value;
        break;
	case SDLK_c:
		CurRenderPipeline->getCamera()->getComponent<FreeCameraViewController>()->bMovedInLastUpdate = true;
		break;
	case SDLK_r:
		PathTracerSettings::GI.useDenoise.value = !PathTracerSettings::GI.useDenoise.value;
		break;
	case SDLK_1:
		if (Input::isKeyDown(SDL_SCANCODE_LCTRL))
			PathTracerSettings::PIPELINE.pipeline = EPathTracerPipeline::RegularPathTracer;
		break;
	case SDLK_2:
			if (Input::isKeyDown(SDL_SCANCODE_LCTRL))
		PathTracerSettings::PIPELINE.pipeline = EPathTracerPipeline::BidirectionalPathTracer;
		break;
	case SDLK_3:
		if (Input::isKeyDown(SDL_SCANCODE_LCTRL))
			PathTracerSettings::PIPELINE.pipeline = EPathTracerPipeline::Rasterization;
		break;
    default: break;
    }
}

void PathTracingApp::onMousewheel(float delta)
{
	if (MainCamera->getComponent<FreeCameraViewController>()->bMovedInLastUpdate)
	{
		auto& camSpeed = PathTracerSettings::DEMO.cameraSpeed;
		camSpeed.value = math::clamp(camSpeed.value + delta, camSpeed.min, camSpeed.max);
	}
}

void PathTracingApp::loadScene(const SceneDesc& sceneDesc)
{
	if (sceneDesc.isEmpty)
		return;

    if (m_sceneRoot)
    {
        m_sceneRoot.getOwner().setActive(false);
    }

	ModelImportSettings importSettings;
	importSettings.scale.value = glm::vec3(sceneDesc.scale);
	if (sceneDesc.isRelativeTexPath)
		importSettings.texturesDirectory = std::string(ASSET_ROOT_FOLDER) + sceneDesc.texturePath;
	else
		importSettings.texturesDirectory = sceneDesc.texturePath;

	importSettings.isRelativeAssetPath.value = sceneDesc.isRelativePath;
	
	m_modelFuture = ResourceManager::getModelAsync(sceneDesc.path, importSettings);

	m_parallelCommands.push_back([this, importSettings]() -> ProgressState {
		if (m_modelFuture && m_modelFuture->isCompleted())
		{
			m_sceneRoot = ECSUtil::startStreamingEntitiesFromModel(m_modelFuture->getValue().get(), m_forwardShader, importSettings);

			if (m_sceneRoot)
				m_sceneRoot->setPosition(glm::vec3());

			m_modelFuture = nullptr;
			return ProgressState::Completed;
		}

		return ProgressState::InProgress;
	});
}

void PathTracingApp::createDemoScene()
{
	ECS::addSystem<HostScene>();

    Entity camera = ECS::createEntity("Camera");
    camera.addComponent<Transform>();
    camera.addComponent<CameraComponent>();
	camera.addComponent<FreeCameraViewController>();

    auto camComponent = camera.getComponent<CameraComponent>();
    auto camTransform = camera.getComponent<Transform>();
    glm::vec3 camPosSponza(8.045f, 7.254f, -0.717f);
    glm::vec3 camPosCornell(-0.324f, 4.428f, -13.0f);
    glm::vec3 camPosDragon(-3.311f, 4.489f, -0.158f);
	glm::vec3 camPosSanMiguel(8.222f, 2.562f, -0.624f);

    m_forwardShader = ResourceManager::getShader("shaders/forwardShadingPass.vert", "shaders/forwardShadingPass.frag",
    { "in_pos", "in_normal", "in_tangent", "in_bitangent", "in_uv" });
	Shader::DefaultShader = m_forwardShader;

	m_scenes[0] = SceneDesc("meshes/sponza_obj/sponza.obj", "textures/sponza_textures/", 0.01f, camPosSponza,
		glm::vec3(18.936f, -72.501f, 0.0f) * math::TO_RAD, glm::vec3(math::toRadians(72.0f), 0.0f, 0.0f));

    m_scenes[1] = SceneDesc("meshes/cornell-box/CornellBox-Original.obj", "", 5.0f, camPosCornell,
        glm::vec3(0.0f, 0.0, 0.0f), glm::vec3(math::toRadians(46.0f), math::toRadians(-14.0f), 0.0f));

    m_scenes[2] = SceneDesc("meshes/dragon.obj", "", 5.0f, camPosDragon, glm::vec3(math::toRadians(45.538f), math::toRadians(87.0f), 0.0f),
        glm::vec3(math::toRadians(72.0f), 0.0f, 0.0f));

	m_scenes[3] = SceneDesc("C:/Users/compix/Desktop/Path-Tracer-Test-Models/San Miguel Scene/san-miguel.obj", 
		"C:/Users/compix/Desktop/Path-Tracer-Test-Models/San Miguel Scene/", 1.0f, camPosSanMiguel, 
		glm::vec3(math::toRadians(12.136f), math::toRadians(49.1f), 0.0f), glm::vec3(math::toRadians(72.0f), 0.0f, 0.0f));
	m_scenes[3].isRelativePath = false;
	m_scenes[3].isRelativeTexPath = false;

	m_scenes[4].isEmpty = true;

    MainCamera = camComponent;

    camComponent->setPerspective(45.0f, float(Screen::getWidth()), float(Screen::getHeight()), 0.3f, 30.0f);
	camTransform->setPosition(m_scenes[m_selectedSceneIdx].cameraPos);
	camTransform->setEulerAngles(m_scenes[m_selectedSceneIdx].cameraEulerAngles);

    m_engine->registerCamera(camComponent);

    loadScene(m_scenes[m_selectedSceneIdx]);

	Entity directionalLight = ECS::createEntity("Directional Light");
	directionalLight.addComponent<DirectionalLight>();
	directionalLight.addComponent<Transform>();
	directionalLight.getComponent<Transform>()->setPosition(glm::vec3(0.0f, 20.0f, 0.f));
	directionalLight.getComponent<Transform>()->setEulerAngles(m_scenes[m_selectedSceneIdx].dirLightEulerAngles);
	directionalLight.getComponent<DirectionalLight>()->intensity = 40.0f;
	directionalLight.getComponent<DirectionalLight>()->shadowsEnabled = false;

	// Set common settings
	PathTracerSettings::GI.useTonemapping.value = true;
	PathTracerSettings::GI.minLuminance.value = 5.0f;
	PathTracerSettings::DEMO.stopAtFrame.value = 10000;
}