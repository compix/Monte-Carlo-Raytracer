#pragma once
#include <engine/ecs/ECS.h>
#include <imgui/imgui.h>
#include "../assets/kernels/kernel_data.h"

class RTUberMaterialComponent : public Component
{
public:
	struct MaterialData
	{
		glm::vec3 diffuseColor{ 1.0f };
		glm::vec3 glossColor{ 1.0f };
		glm::vec3 specularReflectionColor{ 0.0f };
		glm::vec3 specularTransmissionColor{ 0.0f };
		bool isGlossyTransmission{ false };
		glm::vec3 opacity{ 1.0f };
		glm::vec2 roughness{ 0.1f };
		float eta{ 1.5f };
		int rtMaterialId = RT_INVALID_ID;

		bool updated = false;
	};

	RTUberMaterialComponent() {}

	virtual std::string getName() const override
	{
		return "RT Uber Material";
	}

	virtual void onShowInEditor() override
	{
		int i = 0;
		for (auto& mat : materialData)
		{
			std::string nodeStr = "Material" + std::to_string(i);
			if (ImGui::TreeNode(nodeStr.c_str()))
			{
				ImGui::Text("Material Id: %d", mat.rtMaterialId);

				mat.updated |= ImGui::DragFloat3("Diffuse Color", &mat.diffuseColor[0], 0.01f);
				mat.updated |= ImGui::DragFloat3("Gloss Color", &mat.glossColor[0], 0.01f);
				mat.updated |= ImGui::DragFloat3("Specular Reflection Color", &mat.specularReflectionColor[0], 0.01f);
				mat.updated |= ImGui::DragFloat3("Specular Transmission Color", &mat.specularTransmissionColor[0], 0.01f);
				mat.updated |= ImGui::Checkbox("Is Glossy Transmission", &mat.isGlossyTransmission);
				mat.updated |= ImGui::DragFloat3("Opacity", &mat.opacity[0], 0.01f);
				mat.updated |= ImGui::DragFloat2("Roughness", &mat.roughness[0], 0.001f, 0.0f, 1.0f);
				mat.updated |= ImGui::DragFloat("Index of Refraction", &mat.eta, 0.001f, 1.0f, 6.0f);
				ImGui::TreePop();
			}

			++i;
		}
	}

	std::vector<MaterialData> materialData;
};
