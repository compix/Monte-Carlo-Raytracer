#pragma once
#include "event/event.h"
#include "event/QuitEvent.h"
#include "input/Input.h"
#include "ecs/ECS.h"

class CameraComponent;
class Application;

class Engine : public Receiver<QuitEvent>, InputHandler
{
public:
    Engine();

    void init(Application* app);

    void update();

    void receive(const QuitEvent&) override;

    bool running() const { return m_running; }

    void shutdown();

    /**
    * Register camera for automatic resize on window resize.
    */
    void registerCamera(ComponentPtr<CameraComponent> camera);

    void unregisterCamera(ComponentPtr<CameraComponent> camera);

    void requestScreenshot() { m_screenshotRequest = true; }

    uint32_t getFrameNumber() const { return m_frameCounter; }

    void setAllowPause(bool allow) { m_allowPause = allow; }
protected:
    void onQuit() override;
    void onWindowEvent(const SDL_WindowEvent& windowEvent) override;
    void onSDLEvent(SDL_Event& sdlEvent) override;
private:
    void resize(int width, int height);

    void takeScreenshot();

private:
    bool m_running;
    bool m_paused{false};
    bool m_initialized;
    bool m_allowPause{ true };
	bool m_wasAppInitializing = true;

    Application* m_app{nullptr};
    std::vector<ComponentPtr<CameraComponent>> m_cameras;

    bool m_screenshotRequest{ false };
    int m_screenshotCounter{ 0 };
    uint32_t m_frameCounter{ 0 };
};
