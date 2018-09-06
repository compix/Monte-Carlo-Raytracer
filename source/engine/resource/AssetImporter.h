#pragma once
#include <memory>
#include <string>
#include "Model.h"
#include "../gui/GUISettings.h"
#include "queue"
#include "../../../third_party/assimp/include/assimp/ProgressHandler.hpp"
#include "mutex"
#include "../util/async_util.h"

struct aiScene;
struct aiNode;
class Mesh;

struct ModelImportSettings : public GUISettings
{
	ModelImportSettings()
	{
		guiElements.insert(guiElements.end(), {&translationAsModelCenter, &mapToUnitCube, &isRelativeAssetPath, &scale, &texturesDirectory});
	}

	/** 
	* Imported models can have an inconvenient origin.
	* This option sets the translation origin to the center of the model.
	*/
	CheckBox translationAsModelCenter{"Origin as Model Center", true};
	CheckBox mapToUnitCube{"Map to Unit Cube", false};
	CheckBox isRelativeAssetPath{ "Is Relative Asset Path", false };
	SliderFloat3 scale{ "Scale", glm::vec3(1.0f), 0.0f, 1000.0f };
	DirectoryPickerGUIElement texturesDirectory{ "Textures Directory: " };
};

struct ModelImportProgressHandler : public Assimp::ProgressHandler
{
	virtual bool Update(float percentage = -1.f) override;
};

class AssetImporter
{
public:
    static std::shared_ptr<Model> import(const std::string& filename, const ModelImportSettings& importSettings);
	static std::shared_ptr<Model> import(const std::string& filename, const ModelImportSettings& importSettings, Assimp::ProgressHandler* progressHandler);
	static std::shared_ptr<AsyncFuture<std::shared_ptr<Model>>> importAsync(const std::string& filename, const ModelImportSettings& importSettings);
private:
    static std::shared_ptr<Model> process(const aiScene* scene, const aiNode* aiNode);
};
