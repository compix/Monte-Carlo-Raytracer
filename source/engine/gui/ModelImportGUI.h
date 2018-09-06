#pragma once
#include "engine/ecs/ECS.h"
#include "../geometry/Transform.h"
#include "../resource/AssetImporter.h"
#include "../interfaces/IUpdateable.h"

class ModelImportGUI : public IUpdateable
{
public:
	ModelImportGUI(const std::string& path);

	void update() override;

	const GUIWindow& getWindow() const { return m_window; }

	void addOnImportFunction(std::function<void(ComponentPtr<Transform>)> func)
	{
		m_onImportFuncs.push_back(func);
	}

	void addOnCancelFunction(std::function<void()> func)
	{
		m_onCancelFuncs.push_back(func);
	}
private:
	ModelImportSettings m_importSettings;
	GUIWindow m_window{"Import Settings"};

	std::string m_modelPath;

	std::vector<std::function<void(ComponentPtr<Transform>)>> m_onImportFuncs;
	std::vector<std::function<void()>> m_onCancelFuncs;

	std::shared_ptr<AsyncFuture<std::shared_ptr<Model>>> m_modelFuture;
};