#pragma once
#include "Window.h"
#include <memory>
#include <glm/glm.hpp>
#include "vector"
#include "functional"
#include "unordered_map"

class Screen
{
public:
    static void init(int width, int height, bool enableVsync);

    static int getWidth() { return m_window->getWidth(); }

    static int getHeight() { return m_window->getHeight(); }

	static glm::ivec2 getCenter() { return glm::ivec2(getWidth() / 2, getHeight() / 2); }

    static void update() { m_window->flip(); }

    static void setTitle(const std::string& title) { m_window->setTitle(title); }

    static void setVsync(bool enableVsync) { m_window->setVsync(enableVsync); }

    static void resize(int width, int height) 
	{ 
		m_window->resize(width, height); 

		for (auto& pair : m_resizeListeners)
			pair.second();
	}

    static void showMessageBox(const std::string& title, const std::string& message, Uint32 flags = SDL_MESSAGEBOX_INFORMATION)
    {
        m_window->showMessageBox(title, message, flags);
    }

    static void showErrorBox(const std::string& title, const std::string& message) { m_window->showErrorBox(title, message); }

    static SDL_Window* getSDLWindow() { return m_window->getSDLWindow(); }

	/**
	* @return Handle to listener. Use it to remove the listener.
	*/
	static int addResizeListener(std::function<void()> listener) 
	{ 
		m_resizeListeners.emplace(std::pair<int, std::function<void()>>(m_resizeListenersCounter, listener));
		m_resizeListenersCounter++;
		return m_resizeListenersCounter - 1;
	}

	static void removeResizeListener(int handle)
	{
		m_resizeListeners.erase(handle);
	}
private:
    static std::unique_ptr<Window> m_window;

	static std::unordered_map<int, std::function<void()>> m_resizeListeners;
	static int m_resizeListenersCounter;
};
