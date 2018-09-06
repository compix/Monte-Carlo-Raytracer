#pragma once
#include <string>
#include <imgui/imgui.h>
#include <vector>
#include <glm/glm.hpp>
#include "../util/file.h"

struct GUIElement
{
    GUIElement(const std::string& label)
        : label(label) {}

    virtual ~GUIElement() {}
    virtual void begin() = 0;
    virtual void end() {}

    std::string label;
};

template <class T>
struct Slider : GUIElement
{
    Slider(const std::string& label, T value, T min, T max, const std::string& displayFormat = "%.3f")
        : GUIElement(label), value(value), min(min), max(max), displayFormat(displayFormat) {}

    operator T() const { return value; }

    T value;
    T min;
    T max;
    std::string displayFormat;
};

/**
* Shows progress with progress value in [0,1].
*/
struct ProgressBar : GUIElement
{
	ProgressBar(const std::string& label, const ImVec2& size = ImVec2(-1,0), ImColor color = ImColor(0, 191, 255, 200))
		: GUIElement(label), size(size), color(color) {}

	void begin() override
	{
		ImGui::PushStyleColor(ImGuiCol_PlotHistogram, color);
		ImGui::ProgressBar(progress, size);
		ImGui::PopStyleColor(1);
	}

	operator float() { return progress; }
	ProgressBar& operator=(float p) { progress = p; return *this; }

	ImColor color;
	float progress = 0;
	ImVec2 size;
};

struct SliderFloat : Slider<float>
{
    SliderFloat(const std::string& label, float value, float min, float max, const std::string& displayFormat = "%.3f", float power = 1.0f)
        : Slider(label, value, min, max, displayFormat), power(power) {}

    void begin() override { ImGui::SliderFloat(label.c_str(), &value, min, max, displayFormat.c_str(), power); }

    float power;
};

struct SliderFloat2 : Slider<glm::vec2>
{
	SliderFloat2(const std::string& label, glm::vec2 value, float min, float max, const std::string& displayFormat = "%.3f", float power = 1.0f)
		: Slider(label, value, glm::vec2(min), glm::vec2(max), displayFormat), power(power) {}

	void begin() override { ImGui::SliderFloat2(label.c_str(), &value[0], min.x, max.x, displayFormat.c_str(), power); }

	float power;
};

struct SliderFloat3 : Slider<glm::vec3>
{
    SliderFloat3(const std::string& label, glm::vec3 value, float min, float max, const std::string& displayFormat = "%.3f", float power = 1.0f)
        : Slider(label, value, glm::vec3(min), glm::vec3(max), displayFormat), power(power) {}

    void begin() override { ImGui::SliderFloat3(label.c_str(), &value[0], min.x, max.x, displayFormat.c_str(), power); }

    float power;
};

struct SliderInt : Slider<int>
{
    SliderInt(const std::string& label, int value, int min, int max, const std::string& displayFormat = "%.0f")
        : Slider(label, value, min, max, displayFormat) {}

    void begin() override { ImGui::SliderInt(label.c_str(), &value, min, max, displayFormat.c_str()); }
};

struct SliderInt2 : Slider<glm::ivec2>
{
	SliderInt2(const std::string& label, glm::ivec2 value, int min, int max, const std::string& displayFormat = "%.0f")
		: Slider(label, value, glm::ivec2(min), glm::ivec2(max), displayFormat) {}

	void begin() override { ImGui::SliderInt2(label.c_str(), &value[0], min.x, max.x, displayFormat.c_str()); }
};

class DirectoryPickerGUIElement : public GUIElement
{
public:
	DirectoryPickerGUIElement(const std::string& label)
		:GUIElement(label) {}

	void begin() override
	{
		ImGui::Text(label.c_str());
		ImGui::SameLine();
		if (ImGui::Button(path == "" ? "..." : path.c_str()))
		{
			file::openDirectoryPickerDialog(path);
		}
	}

	operator std::string() const { return path; }
	DirectoryPickerGUIElement& operator =(const std::string& p) { path = p; return *this;}

	std::string path;
};

class FilePickerGUIElement : public GUIElement
{
public:
	FilePickerGUIElement(const std::string& label)
		:GUIElement(label) {}

	void begin() override
	{
		ImGui::Text(label.c_str());
		ImGui::SameLine();
		if (ImGui::Button(path == "" ? "..." : path.c_str()))
		{
			file::openFileDialog("", "", path);
		}
	}

	operator std::string() const { return path; }

	std::string path;
};

class Button : public GUIElement
{
public:
	Button(const std::string& label)
		:GUIElement(label), onButtonClick([](){}) {}

	Button(const std::string& label, std::function<void()> onButtonClickFunction)
		:GUIElement(label), onButtonClick(onButtonClickFunction) {}

	void begin() override
	{
		if (ImGui::Button(label.c_str()))
		{
			onButtonClick();
		}
	}

	std::function<void()> onButtonClick;
};

struct CheckBox : GUIElement
{
    CheckBox(const std::string& label, bool value)
        : GUIElement(label), value(value) {}

    void begin() override { ImGui::Checkbox(label.c_str(), &value); }

    operator bool() const { return value; }

    bool value;
};

struct ComboBox : GUIElement
{
	ComboBox(const std::string& label) : GUIElement(label) {}
    ComboBox(const std::string& label, const std::vector<const char*>& items, int curItem)
        : GUIElement(label), items(items), curItem(curItem) {}

    void begin() override { ImGui::Combo(label.c_str(), &curItem, &items[0], int(items.size())); }

    operator int() const { return curItem; }

    std::string asString() const { return items[curItem]; }

    std::vector<const char*> items;
    int curItem{0};
};

template<class TEnum>
struct ComboBoxEnum : GUIElement
{
	ComboBoxEnum(const std::string& label) : GUIElement(label) {}
	ComboBoxEnum(const std::string& label, const std::vector<const char*>& items, int curItem)
		: GUIElement(label), items(items), curItem(curItem) {}

	void begin() override { ImGui::Combo(label.c_str(), &curItem, &items[0], int(items.size())); }

	operator TEnum() const { return static_cast<TEnum>(curItem); }
	TEnum getEnumValue() const { return static_cast<TEnum>(curItem); }
	ComboBoxEnum& operator=(TEnum val) { curItem = static_cast<int>(val); return *this; }
	std::string asString() const { return items[curItem]; }

	std::vector<const char*> items;
	int curItem{ 0 };
};

struct ColorSelection : GUIElement
{
    ColorSelection(const std::string& label, const glm::vec4& color = glm::vec4(1.0f))
        : GUIElement(label), color(color) {}

    void begin() override { ImGui::ColorEdit4(label.c_str(), &color[0]); }

    operator glm::vec4() const { return color; }

    glm::vec4 color;
};

class EditField : public GUIElement
{
public:
	EditField(const std::string& label)
		:GUIElement(label) 
	{
		inputTextBuffer.resize(_MAX_PATH);
	}

	void begin() override 
	{
		ImGui::InputText(label.c_str(), &inputTextBuffer[0], _MAX_PATH);
	}

	std::string text() const { return std::string(&inputTextBuffer[0]); }

	operator std::string() const { return text(); }

private:
	std::vector<char> inputTextBuffer;
};

class TextField : public GUIElement
{
public:
	TextField(const std::string& label, const std::string& text = "")
		:GUIElement(label), text(text) {}
	
	void begin() override
	{
		ImGui::Text("%s: %s", label.c_str(), text.c_str());
	}

	void operator=(const std::string& other) { text = other; }

	std::string text;
};

struct GUIWindow : GUIElement
{
    explicit GUIWindow(const std::string& label)
        : GUIElement(label) { }

    GUIWindow(const std::string& label, const ImVec2& size, const ImVec2& pos, const ImColor& backgroundColor, ImGuiWindowFlags flags)
        : GUIElement(label), size(size), pos(pos), backgroundColor(backgroundColor), flags(flags) { }

    void begin() override
    {
        if (pos.x != lastPos.x || pos.y != lastPos.y || ((flags & ImGuiWindowFlags_AlwaysAutoResize) == 0 && (flags & ImGuiWindowFlags_NoResize) != 0))
            ImGui::SetNextWindowSize(size);

        if (pos.x != lastPos.x || pos.y != lastPos.y || ((flags & ImGuiWindowFlags_NoMove) != 0))
            ImGui::SetNextWindowPos(pos);

        ImGui::SetNextWindowSizeConstraints(minSize, maxSize);

        ImGui::PushStyleColor(ImGuiCol_TitleBgActive, titleActiveBackgroundColor);
        ImGui::PushStyleColor(ImGuiCol_TitleBg, titleBackgroundColor);
        ImGui::PushStyleColor(ImGuiCol_TitleBgCollapsed, titleBackgroundColor);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, backgroundColor);
        ImGui::Begin(label.c_str(), &open, flags);

        pos = ImGui::GetWindowPos();
        size = ImGui::GetWindowSize();

        lastPos = pos;
        lastSize = size;
    }

    void end() override
    {
        ImGui::End();
        ImGui::PopStyleColor(4);
    }

    ImVec2 minSize;
    ImVec2 maxSize{FLT_MAX, FLT_MAX};
    ImVec2 lastPos;
    ImVec2 lastSize;
    ImVec2 size{500.0f, 500.0f};
    ImVec2 pos;
    ImColor backgroundColor{0.f, 0.f, 0.f, 0.8f};
    ImColor titleBackgroundColor{0.0f, 0.0f, 0.0f, 0.8f};
    ImColor titleActiveBackgroundColor{0.1f, 0.1f, 0.1f, 0.8f};
    ImGuiWindowFlags flags{0};
    bool open{true};
};
