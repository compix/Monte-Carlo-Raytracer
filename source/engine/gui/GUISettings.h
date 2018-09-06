#pragma once
#include <vector>
#include "GUIElements.h"

struct GUISettings : public GUIElement
{
	GUISettings()
		:GUIElement("") {}

	GUISettings(const std::string& label)
		:GUIElement(label) {}

    std::vector<GUIElement*> guiElements;

	virtual void begin() override
	{
		for (auto e : guiElements)
			e->begin();
	}

	virtual void end() override
	{
		for (auto e : guiElements)
			e->end();
	}
};
