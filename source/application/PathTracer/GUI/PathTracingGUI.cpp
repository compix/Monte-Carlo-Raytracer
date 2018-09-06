#include <imgui/imgui.h>
#include <engine/rendering/Screen.h>
#include <engine/util/Timer.h>
#include <algorithm>
#include <sstream>
#include <engine/ecs/ECS.h>
#include <engine/geometry/Transform.h>
#include <engine/rendering/debug/DebugRenderer.h>
#include <engine/input/Input.h>
#include "engine/util/Random.h"
#include "engine/gui/GUI.h"
#include "engine/util/ECSUtil/EntityCreator.h"
#include "engine/rendering/lights/DirectionalLight.h"
#include "PathTracingGUI.h"
#include "engine/globals.h"
#include "engine/gui/StatsWindow.h"
#include "engine/rendering/settings/RenderingSettings.h"
#include "PathTracingSettings.h"
#include "engine/util/file.h"
#include "engine/util/ECSUtil/ECSUtil.h"
#include "../../../engine/gui/ModelImportGUI.h"
#include "../../../engine/rendering/lights/PointLight.h"
#include "../../../engine/rendering/lights/DiskAreaLight.h"
#include "../../../engine/rendering/lights/TriangleMeshAreaLight.h"
#include "../Raytracing/rt_globals.h"
#include "../Raytracing/material/RTUberMaterialComponent.h"
#include "../Raytracing/system/RTBufferManager.h"
#include "../Raytracing/system/PlatformManager.h"
#include "../../../engine/resource/ResourceManager.h"
#include "../raytracing/scene/RTScene.h"

PathTracingGUI::PathTracingGUI(RenderPipeline* renderPipeline, PathTracingApp* pathTracingApp)
    : m_renderPipeline(renderPipeline), m_pathTracingApp(pathTracingApp)
{
    m_mainWindow.flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize;
    m_mainWindow.pos = ImVec2(0.0f, 20.0f);
    m_mainWindow.maxSize = ImVec2(FLT_MAX, Screen::getHeight() - 20.0f);

    m_fpsWindow.flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse;

    m_controlWindow.flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse;
    m_controlWindow.pos.y = Screen::getHeight() - m_controlWindow.size.y;
    m_controlWindow.backgroundColor = ImColor(0.0f, 0.0f, 0.0f, 0.0f);

    m_helpWindow.flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove;
    m_helpWindow.size = ImVec2(float(Screen::getWidth()), float(Screen::getHeight()) - 20.0f);
    m_helpWindow.backgroundColor = ImColor(0, 0, 0, 255);

    m_entityWindow.flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize;
    m_entityWindow.minSize = ImVec2(300.0f, 0.0f);

    m_entityPicker = std::make_unique<EntityPicker>();
    m_statsWindow = std::make_unique<StatsWindow>();

    Input::subscribe(this);
}

void PathTracingGUI::update()
{
	for (auto& updateable : m_updateables)
	{
		updateable->update();
	}

	if (m_modelImportGUI)
	{
		m_modelImportGUI->update();
	}

    if (m_entityPickRequest.requested)
    {
        m_entityPicker->update();
        m_selectedEntity = m_entityPicker->pick(m_entityPickRequest.x, m_entityPickRequest.y);
        m_entityPickRequest.requested = false;
    }

    guiShowFPS();

    ImGui::PushStyleColor(ImGuiCol_MenuBarBg, ImColor(0.0f, 0.0f, 0.0f, 0.8f));
    if (ImGui::BeginMainMenuBar())
    {
        showViewMenu();
        showSceneMenu();
        showEditorMenu();
        showHelpMenu();

		if (ResourceManager::progress < 1.0f)
		{
			float progressBarWidth = 60.0f;
			ResourceManager::progress.size = ImVec2(progressBarWidth, 15.0f);
			ImGui::SameLine(Screen::getWidth() - progressBarWidth - 2.0f);
			ImGui::SetCursorPosY(2.0f);
			ResourceManager::progress.begin();
		}

        ImGui::EndMainMenuBar();
    }
    ImGui::PopStyleColor(1);

    showControlWindow();

    if (m_showSettings)
    {
        m_mainWindow.begin();

		if (ImGui::TreeNode("Info"))
		{
			m_platformGUI.update();

			std::stringstream ss;
			ss << "~~~ Used CL-GPU Memory Info ~~~\n";
			ss << "Used Memory: " << RTBufferManager::getTotalAllocatedSize() / 1024 / 1024 << "mb\n";
			ss << "Application Max Single Allocation Size: " << RTBufferManager::getMaxAllocatedSize() / 1024 / 1024 << "mb\n";
			ImGui::Text(ss.str().c_str());

			ImGui::Text("spp: %d", g_frameIndex);
			ImGui::Text("Render Time: %f", g_totalRenderTime);
			ImGui::TreePop();
		}
        showEntityTree();

        if (m_selectedEntity)
        {
            showEntityWindow(m_selectedEntity);

            // Entity will be unselected if window is closed so check again if entity is selected!
            if (m_selectedEntity)
            {
                auto transform = m_selectedEntity.getComponent<Transform>();
                if (transform)
                {
                    DebugRenderInfo info(MainCamera->view(), MainCamera->proj(), MainCamera->getPosition());
                    DebugRenderer::begin(info);
                    if (m_showObjectCoordinateSystem)
                        DebugRenderer::drawCoordinateSystem(transform);

                    if (m_showObjectBBox)
                        DebugRenderer::drawNonFilledCube(transform->getBBox().center(), transform->getBBox().scale(), glm::vec3(0.0f, 1.0f, 0.0f));

                    DebugRenderer::end();
                }
            }
        }

        m_cameraMoveCommand(Time::deltaTime());

		showSettings("Intersection API Settings", &PathTracerSettings::INTERSECTION_API);
		showSettings("Pipeline Settings", &PathTracerSettings::PIPELINE);
        showSettings("GI Settings", &PathTracerSettings::GI);
        showSettings("Rendering Settings", &RENDERING_SETTINGS);
		showSettings("Scene Visualization Settings", &SCENE_VISUALIZATION_SETTINGS);
        showSettings("Demo Settings", &PathTracerSettings::DEMO);
        
        m_mainWindow.end();
    }

    if (m_statsWindow->open())
        m_statsWindow->update();

	m_gBufferGUI.update();
}

void PathTracingGUI::showEntityTree()
{
    m_treeNodeIdx = 0;

    if (ImGui::TreeNode("Entities"))
    {
        auto entities = ECS::getEntitiesWithComponentsIncludeInactive<Transform>();
        std::vector<ComponentPtr<Transform>> rootSet;

        for (auto entity : entities)
        {
            auto transform = entity.getComponent<Transform>();
            auto root = transform->getRoot();
            bool contains = false;

            // Make sure it's not yet in the set
            for (auto& t : rootSet)
                if (t == root)
                {
                    contains = true;
                    break;
                }

            if (!contains)
                rootSet.push_back(root);
        }

        for (auto& transform : rootSet) { subTree(transform); }

        ImGui::TreePop();
    }
}

void PathTracingGUI::moveCameraToEntity(ComponentPtr<CameraComponent>& camera, Entity& entity)
{
    auto camTransform = camera->getComponent<Transform>();
    auto entityTransform = entity.getComponent<Transform>();

    if (camTransform && entityTransform)
        m_cameraMoveCommand = MoveCommand(camTransform, camTransform->getPosition(), entityTransform->getPosition() - camTransform->getForward() * 3.0f, 0.5f);
}

void PathTracingGUI::subTree(const ComponentPtr<Transform>& transform)
{
    ImGuiTreeNodeFlags nodeFlags = ImGuiTreeNodeFlags_OpenOnArrow;

    Entity owner = transform->getOwner();
    std::string entityName = owner.getName();

    ImGuiTreeNodeFlags selectedFlag = m_selectedEntity == owner ? ImGuiTreeNodeFlags_Selected : 0;

    if (transform->hasChildren())
    {
        bool nodeOpen = ImGui::TreeNodeEx((void*)(intptr_t)m_treeNodeIdx, nodeFlags | selectedFlag, entityName.c_str());
        ++m_treeNodeIdx;
        if (ImGui::IsItemClicked())
            onEntityClicked(owner);

        if (nodeOpen)
        {
            for (auto& child : transform->getChildren())
                subTree(child);

            ImGui::TreePop();
        }
    }
    else
    {
        ImGui::TreeNodeEx((void*)(intptr_t)m_treeNodeIdx, nodeFlags | ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | selectedFlag, entityName.c_str());
        if (ImGui::IsItemClicked())
            onEntityClicked(owner);

        ++m_treeNodeIdx;
    }
}

void PathTracingGUI::guiShowFPS()
{
    m_fpsWindow.begin();
    m_fpsWindow.pos.x = Screen::getWidth() * 0.5f - m_fpsWindow.size.x * 0.5f;
    m_fpsWindow.pos.y = 20.0f;

    static Seconds left = 1.f;
    static uint32_t fps = 0;
    static uint32_t fpsCounter = 0;
    static Seconds frameTime = 0.f;
    left -= Time::deltaTime();

    if (left <= 0.f)
    {
        frameTime = Seconds(1000) / std::max(fpsCounter, 1u);
        left += 1.f;
        fps = fpsCounter;
        fpsCounter = 0;
    }

    std::stringstream ss;
    ss.precision(2);

    ss << "FPS: " << fps;
    ss << " Frame time: " << std::fixed << frameTime << "ms";
    ss << " Frame Num: " << PathTracerSettings::DEMO.frameNum;
    ss << " Total time: " << int(Time::totalTime());

    ImGui::Text(ss.str().c_str());

    fpsCounter++;
    m_fpsWindow.end();
}

void PathTracingGUI::showComponents(const Entity& entity) const
{
    auto components = entity.getAllComponents();

    int treeNodeIdx = 0;

    for (auto& component : components)
    {
        if (ImGui::TreeNode((void*)(intptr_t)treeNodeIdx, component->getName().c_str()))
        {
            component->onShowInEditor();
            ImGui::TreePop();
        }

        ++treeNodeIdx;
    }
}

template<class T>
struct AddComponentMenuItem
{
	void operator()(const std::string& name, Entity e)
	{
		if (ImGui::MenuItem(name.c_str()))
		{
			auto component = e.getComponent<T>();
			if (!component)
			{
				e.addComponent<T>();
			}
		}
	}
};

void PathTracingGUI::showEntityWindow(Entity entity)
{
    m_entityWindow.open = true;

    m_entityWindow.begin();
    m_entityWindow.pos.x = Screen::getWidth() - m_entityWindow.size.x;
    m_entityWindow.pos.y = 20.0f;
    m_entityWindow.label = entity.getName();

    bool active = entity.isActive();
    ImGui::Checkbox("Active", &active);
    entity.setActive(active);

    showComponents(entity);

	if (ImGui::BeginPopupContextWindow(true, "Test"))
	{
		if (ImGui::BeginMenu("Add Component"))
		{
			AddComponentMenuItem<PointLight>()("Point Light", entity);
			AddComponentMenuItem<DirectionalLight>()("Directional Light", entity);
			AddComponentMenuItem<DiskAreaLight>()("Disk Area Light", entity);
			AddComponentMenuItem<TriangleMeshAreaLight>()("Triangle Mesh Area Light", entity);
			AddComponentMenuItem<RTUberMaterialComponent>()("RT Uber Material", entity);

			ImGui::EndMenu();
		}

		ImGui::EndPopup();
	}

    if (!m_entityWindow.open)
        m_selectedEntity = Entity();

    m_entityWindow.end();
}

void PathTracingGUI::onEntityClicked(Entity& clickedEntity)
{
    m_selectedEntity = clickedEntity;

    if (ImGui::IsMouseDoubleClicked(0))
    {
        if (MainCamera)
            moveCameraToEntity(MainCamera, m_selectedEntity);
    }
}

void PathTracingGUI::showSettings(const std::string& label, GUISettings* settings) const
{
    if (ImGui::TreeNode(label.c_str()))
    {
		settings->begin();
		settings->end();

        ImGui::TreePop();
    }
}

void PathTracingGUI::showTextures(const ImVec2& canvasSize, std::initializer_list<GUITexture> textures) const
{
    size_t count = textures.size();
    size_t nColumns = size_t(ceil(sqrtf(float(count))));
    size_t nRows = size_t(ceil(float(count) / nColumns));

    auto panelSize = ImVec2(canvasSize.x / nColumns, canvasSize.y / nRows - 25.0f);
    auto textureSize = ImVec2(canvasSize.x / nColumns, canvasSize.y / nRows - 50.0f);

    size_t i = 0;

    for (auto& tex : textures)
    {
        if (i % nColumns > 0)
            ImGui::SameLine();

        ImGui::BeginChild(tex.label.c_str(), panelSize);
        ImGui::Text(tex.label.c_str());
        ImGui::Image(ImTextureID(uintptr_t(tex.texID)), textureSize, ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));
        ImGui::EndChild();

        ++i;
    }
}

void PathTracingGUI::showViewMenu()
{
    if (ImGui::BeginMenu("View"))
    {
		if (ImGui::MenuItem("G-Buffers", nullptr, &m_gBufferGUI.window.open))
			m_gBufferGUI.window.pos = ImVec2(Screen::getWidth() * 0.5f - m_gBufferGUI.window.size.x * 0.5f,
										     Screen::getHeight() * 0.5f - m_gBufferGUI.window.size.y * 0.5f);

        ImGui::MenuItem("Stats", nullptr, &m_statsWindow->open());
        ImGui::MenuItem("Settings", nullptr, &m_showSettings);
        ImGui::EndMenu();
    }
}

void PathTracingGUI::showSceneMenu()
{
    if (ImGui::BeginMenu("Scene"))
    {
        if (ImGui::BeginMenu("Add"))
        {
			glm::vec3 spawnPos = MainCamera->getPosition() + MainCamera->getForward() * 1.5f;

			if (ImGui::MenuItem("Load..."))
			{
				std::string outPath;
				if (file::openFileDialog(outPath))
				{
					m_modelImportGUI = std::make_unique<ModelImportGUI>(outPath);

					m_modelImportGUI->addOnImportFunction([this, spawnPos](ComponentPtr<Transform> rootTransform) {
						rootTransform->setPosition(spawnPos);
						m_selectedEntity = rootTransform->getOwner();
					});

					m_modelImportGUI->addOnCancelFunction([this]() {
						m_modelImportGUI = nullptr;
					});
				}
			}

            if (ImGui::MenuItem("Directional Light"))
            {

                m_selectedEntity = EntityCreator::createDefaultDirLight();
            }

			if (ImGui::MenuItem("Point Light"))
			{

				m_selectedEntity = EntityCreator::createDefaulPointLight();
				m_selectedEntity.getComponent<Transform>()->setPosition(spawnPos);
			}

            if (ImGui::MenuItem("Box"))
            {
                m_selectedEntity = EntityCreator::createDefaultBox(spawnPos);
            }

            if (ImGui::MenuItem("Cylinder"))
            {
                m_selectedEntity = EntityCreator::createDefaultCylinder(spawnPos);
            }

            if (ImGui::MenuItem("Sphere"))
            {
                m_selectedEntity = EntityCreator::createDefaultSphere(spawnPos);
            }

			if (ImGui::MenuItem("Disk"))
			{
				m_selectedEntity = EntityCreator::createDefaultDisk(spawnPos);
			}

			if (ImGui::MenuItem("Rectangle"))
			{
				m_selectedEntity = EntityCreator::createDefaultRectangle(spawnPos);
			}

			if (ImGui::MenuItem("Triangle"))
			{
				m_selectedEntity = EntityCreator::createDefaultTriangle(spawnPos);
			}

			if (ImGui::MenuItem("Entity"))
			{
				m_selectedEntity = ECS::createEntity();
				m_selectedEntity.addComponent<Transform>();
				m_selectedEntity.getComponent<Transform>()->setPosition(spawnPos);
			}

            ImGui::EndMenu();
        }

        ImGui::EndMenu();
    }
}

void PathTracingGUI::showEditorMenu()
{
    if (ImGui::BeginMenu("Editor"))
    {
        ImGui::MenuItem("Show Object Coordinate System", nullptr, &m_showObjectCoordinateSystem);
        ImGui::MenuItem("Show Object AABB", nullptr, &m_showObjectBBox);

        ImGui::EndMenu();
    }
}

void PathTracingGUI::showHelpMenu() const
{
    if (ImGui::BeginMenu("Help"))
    {
        if (ImGui::MenuItem("Help"))
        {
            std::stringstream ss;
			ss << "W,A,S,D: Move around.\n";
			ss << "Hold Right Mouse Button + Move Mouse: Look Around.\n";
			ss << "Hold Right Mouse Button + Scrollwheel: Increase/Decrease Movement Speed.\n";
            ss << "Ctrl+F1: Use unidirectional path tracer.\n";
            ss << "Ctrl+F2: Use bidirectional path tracer.\n";
			ss << "Ctrl+F3: Use rasterization pipeline.\n";
            ss << "F3: Enable/Disable GUI.\n";
            ss << "F5: Take a screenshot.\n";
            ss << "J: Toggle camera jitter for anti-aliasing.\n";
            ss << "R: Toggle bilateral denoise.";

            Screen::showMessageBox("Help", ss.str());
        }

        ImGui::EndMenu();
    }
}

void PathTracingGUI::showControlWindow()
{
    m_controlWindow.pos.x = Screen::getWidth() * 0.5f - m_controlWindow.size.x * 0.5f;
    m_controlWindow.pos.y = Screen::getHeight() - m_controlWindow.size.y;
    m_controlWindow.begin();
    ImGui::PushStyleColor(ImGuiCol_Button, ImColor(0, 0, 0, 200));
	if (!ResourceManager::isLoading())
	{
		if (g_isectApi)
		{
			if (ImGui::Button("Refresh Scene", ImVec2(200.0f, 30.0f)))
			{
				auto scene = ECS::getSystem<RTScene>();
				if (scene)
					scene->refresh();
			}
		}
		else
		{
			if (ImGui::Button("Path Trace", ImVec2(200.0f, 30.0f)))
			{
				PathTracerSettings::PIPELINE.pipeline = EPathTracerPipeline::RegularPathTracer;
			}
		}
	}

    ImGui::PopStyleColor(1);
    m_controlWindow.end();
}

void PathTracingGUI::onMouseDown(const SDL_MouseButtonEvent& e)
{
    switch (e.button)
    {
    case SDL_BUTTON_LEFT:
        if (!ImGui::IsMouseHoveringAnyWindow())
            m_entityPickRequest = EntityPickRequest(e.x, Screen::getHeight() - e.y);
        break;
    default: break;
    }
}

void PathTracingGUI::onWindowEvent(const SDL_WindowEvent& windowEvent)
{
    switch (windowEvent.event)
    {
    case SDL_WINDOWEVENT_RESIZED:
        m_mainWindow.maxSize = ImVec2(FLT_MAX, windowEvent.data2 - 20.0f);
        m_controlWindow.size = ImVec2(float(windowEvent.data1), float(windowEvent.data2));
        break;
    default: break;
    }
}
