#pragma once
#include <SDL.h>
#include <string>

class Window
{
	friend class Screen;
public:
    Window(int width, int height, bool enableVsync = true);
    ~Window();

    void flip();

    void setTitle(const std::string& title) const { SDL_SetWindowTitle(m_window, title.c_str()); }

    SDL_Window* getSDLWindow() const { return m_window; }

    void setVsync(bool enableVsync);
    void resize(int width, int height);

    int getWidth() const noexcept { return m_width; }

    int getHeight() const noexcept { return m_height; }

    static void showMessageBox(const std::string& title, const std::string& message, Uint32 flags = SDL_MESSAGEBOX_INFORMATION);
    static void showErrorBox(const std::string& title, const std::string& message);

	SDL_GLContext getGLContext() const { return m_context; }

	void makeCurrent();
private:
    static bool init(int oglMajorVersion = 3, int oglMinorVersion = 3);

	bool createWindow(bool enableVsync);

private:
    SDL_Window* m_window;
    SDL_GLContext m_context;

    int m_width;
    int m_height;

	static int m_oglMinorVersion;
	static int m_oglMajorVersion;
};
