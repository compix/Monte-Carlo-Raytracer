#include "RTShapeComponent.h"
#include <imgui/imgui.h>

void RTShapeComponent::onShowInEditor()
{
	for (auto shape : shapes)
	{
		ImGui::Text("Shape Id: %d", shape->GetId());
	}
}
