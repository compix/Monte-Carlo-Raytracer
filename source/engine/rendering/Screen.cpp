#include "Screen.h"

std::unique_ptr<Window> Screen::m_window;

std::unordered_map<int, std::function<void()>> Screen::m_resizeListeners;

int Screen::m_resizeListenersCounter = 0;

void Screen::init(int width, int height, bool enableVsync)
{
	if (!Window::init())
		return;

    m_window = std::make_unique<Window>(width, height, enableVsync);
}
