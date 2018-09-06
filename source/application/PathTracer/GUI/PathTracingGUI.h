#pragma once
#include "engine/input/Input.h"
#include "engine/gui/GUIElements.h"
#include "engine/util/commands/MoveCommand.h"
#include "engine/rendering/architecture/RenderPipeline.h"
#include "engine/util/ECSUtil/EntityPicker.h"
#include "engine/gui/GUITexture.h"
#include "engine/gui/StatsWindow.h"
#include "engine/gui/GeometryBufferGUI.h"
#include "../Raytracing/PlatformGUI.h"
#include "engine/interfaces/IUpdateable.h"
#include "../../../engine/gui/ModelImportGUI.h"

class PathTracingApp;
struct GUISettings;

class PathTracingGUI : public InputHandler
{
    struct EntityPickRequest
    {
        EntityPickRequest() {}
        EntityPickRequest(int x, int y)
            :x(x), y(y) {}

        int x{ 0 };
        int y{ 0 };

        bool requested{ true };
    };
public:
    PathTracingGUI::PathTracingGUI(RenderPipeline* renderPipeline, PathTracingApp* pathTracingApp);
    void update();

    void showEntityTree();

    void moveCameraToEntity(ComponentPtr<CameraComponent>& camera, Entity& entity);

	void setDefaultShader(std::shared_ptr<Shader> shader) { m_defaultShader = shader; }
private:
    void subTree(const ComponentPtr<Transform>& transform);
    void guiShowFPS();
    void showComponents(const Entity& entity) const;
    void showEntityWindow(Entity entity);
    void onEntityClicked(Entity& clickedEntity);
    void showSettings(const std::string& label, GUISettings* settings) const;
    void showTextures(const ImVec2& canvasSize, std::initializer_list<GUITexture> textures) const;
    void showViewMenu();
    void showSceneMenu();
    void showEditorMenu();
    void showHelpMenu() const;
    void showControlWindow();

protected:
    void onMouseDown(const SDL_MouseButtonEvent& e) override;
    void onWindowEvent(const SDL_WindowEvent& windowEvent) override;

private:
    Entity m_selectedEntity;
    int m_treeNodeIdx{ 0 };
    MoveCommand m_cameraMoveCommand;
    RenderPipeline* m_renderPipeline;
    PathTracingApp* m_pathTracingApp;

    bool m_showGBuffers{ false };
    GUIWindow m_mainWindow{ "Main Window" };
    GUIWindow m_fpsWindow{ "FPS" };
    GUIWindow m_entityWindow{ "Entity Window" };
    GUIWindow m_controlWindow{ "Control Window" };
    GUIWindow m_helpWindow{ "Help Window" };

    std::unique_ptr<EntityPicker> m_entityPicker;
    std::unique_ptr<StatsWindow> m_statsWindow;
    bool m_showSettings{ true };

    EntityPickRequest m_entityPickRequest;

    bool m_visualizeTexture{ false };

    bool m_showObjectCoordinateSystem{ true };
    bool m_showObjectBBox{ true };
	GeometryBufferGUI m_gBufferGUI;
	PlatformGUI m_platformGUI;
	std::shared_ptr<Shader> m_defaultShader;

	std::vector<std::unique_ptr<IUpdateable>> m_updateables;
	std::unique_ptr<ModelImportGUI> m_modelImportGUI;
};
