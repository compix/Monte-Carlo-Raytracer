#pragma once
#include "engine/gui/GUIElements.h"

class PlatformGUI
{
public:
	void update();

private:
	bool m_firstTime = true;
	ComboBox m_deviceSelection{ "" };
};