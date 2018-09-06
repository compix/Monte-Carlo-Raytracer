#include "PlatformGUI.h"
#include "imgui/imgui.h"
#include <sstream>
#include "system/PlatformManager.h"
#include "engine/gui/GUIElements.h"

void PlatformGUI::update()
{
	CLWDevice* device = PlatformManager::getActiveDevice();
	if (!device)
		return;

	std::stringstream ss;
	ss << "~~~~~ Active Device Info: ~~~~~\n";
	ss << "Name: " << device->GetName() << std::endl;
	ss << "Vendor: " << device->GetVendor() << std::endl;
	ss << "Profile: " << device->GetProfile() << std::endl;
	ss << "Version: " << device->GetVersion() << std::endl;
	ss << "Global Memory: " << device->GetGlobalMemSize() / 1024 / 1024 << "mb\n";
	ss << "Max Alloc Size: " << device->GetMaxAllocSize() / 1024 / 1024 << "mb\n";

	ImGui::Text(ss.str().c_str());

	if (m_firstTime)
	{
		static std::vector<std::string> itemEntries;
		for (auto& d : PlatformManager::getSupportedDevices())
		{
			itemEntries.push_back(d.GetName() + " " + d.GetVendor());
			m_deviceSelection.items.push_back(itemEntries[itemEntries.size()-1].c_str());
		}

		m_deviceSelection.curItem = PlatformManager::getActiveDeviceIdx();
		m_firstTime = false;
	}

	ImGui::Text("Selected Device:\n");
	m_deviceSelection.begin();
	PlatformManager::setActiveDeviceIdx(m_deviceSelection);
}
