#include "ModelImportGUI.h"
#include "../util/ECSUtil/ECSUtil.h"
#include "../rendering/Screen.h"
#include "../util/file.h"
#include "../resource/ResourceManager.h"

ModelImportGUI::ModelImportGUI(const std::string& path)
	:m_modelPath(path)
{
	m_window.open = true;
	m_window.flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse;

	m_importSettings.texturesDirectory.path = file::Path(m_modelPath).getDirectory();
}

void ModelImportGUI::update()
{
	if (m_window.open)
	{
		bool isOpen = m_window.open;

		glm::ivec2 screenCenter = Screen::getCenter();
		m_window.pos.x = screenCenter.x - m_window.lastSize.x / 2;
		m_window.pos.y = Screen::getHeight() - m_window.lastSize.y - 10;

		m_window.begin();
		file::Path path(m_modelPath);
		ImGui::Text("Path: %s \n", path.asString().c_str());

		m_importSettings.begin();
		m_importSettings.end();

		if (ImGui::Button("Import"))
		{
			m_modelFuture = ResourceManager::getModelAsync(m_modelPath, m_importSettings);
		}

		if (m_modelFuture && m_modelFuture->isCompleted())
		{
			auto loadedEntityTransform = ECSUtil::startStreamingEntitiesFromModel(m_modelFuture->getValue().get(), Shader::DefaultShader, m_importSettings);

			if (loadedEntityTransform)
			{
				for (auto onImportFunc : m_onImportFuncs)
					onImportFunc(loadedEntityTransform);
			}

			m_modelFuture = nullptr;
		}

		ImGui::SameLine();
		if (ImGui::Button("Cancel"))
		{
			m_window.open = false;
		}

		m_window.end();

		if (isOpen && !m_window.open)
		{
			for (auto onCancelFunc : m_onCancelFuncs)
				onCancelFunc();
		}
	}
}
