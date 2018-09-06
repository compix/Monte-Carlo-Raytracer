#pragma once
#include "engine/gui/GUISettings.h"
#include "../../../engine/util/Logger.h"
#include "../../../engine/rendering/Screen.h"

enum class ERTAccelerationStructureType
{
	BVH,
	FatBVH,
	HLBVH
};

enum class ERTBVHBuilderType
{
	SAH,
	Median
};

enum class EPathTracerPipeline
{
	RegularPathTracer,
	BidirectionalPathTracer,
	Rasterization
};

enum class EFilterType
{
	Box = 0,
	Triangle,
	Gaussian,
	Mitchell,
	LanczosSinc
};

namespace PathTracerSettings
{
    struct Demo : GUISettings
    {
        Demo()
        {
            guiElements.insert(guiElements.end(), { &cameraSpeed, &stopAtFrame, &stopAtTime });
        }

        SliderFloat cameraSpeed{ "Camera Speed", 5.0f, 1.0f, 30.0f };
        uint32_t frameNum{ 0 };
        SliderInt stopAtFrame{ "Stop At Frame", 128, -1, 65536 };
		SliderFloat stopAtTime{ "Stop At Time (Seconds)", -1.0f, -1.0f, 259200.0f};
    };

    struct GISettings : GUISettings
    {
		struct FilterSettings
		{
			// In pixels
			SliderFloat2 radius{ "Radius", glm::vec2(2.0f), 0.0f, 8.0f, "%.4f"};
			Button resetRadiusButton{"Reset Radius", [this]() { radius.value =
				glm::vec2(2.0f / Screen::getWidth(), 2.0f / Screen::getHeight()); }};

			// Note: Mitchell and Netravali recommend using values for B and C such that B + 2C = 1.
			SliderFloat mitchellB{ "B", 1.0f / 3.0f, 0.0f, 2.0f, "%.4f" };
			SliderFloat mitchellC{ "C", 1.0f / 3.0f, 0.0f, 0.5f, "%.4f" };

			SliderFloat lanczosSincTau{ "Tau", 3.0f, 1.0f, 8.0f, "%.4f" };

			SliderFloat gaussianAlpha{ "Alpha", 1.0f, 0.01f, 8.0f, "%.4f" };
			float gaussianExpX, gaussianExpY;

			glm::vec2 curPixelOffset;

			ComboBoxEnum<EFilterType> filterType{"Filter Type", {"Box", "Triangle", "Gaussian", "Mitchell", "LanczosSinc"}, 1};
		};

        GISettings()
        {
            guiElements.insert(guiElements.end(), {&useTAA, &maxDepth,
				&denoiseKernelRadius, &bilateralDenoiseSigmaRange, 
				&bilateralDenoiseSigmaSpatial, &useDenoise, &minLuminance, &useTonemapping });
        }

		CheckBox useTAA{ "Use TAA", true };
        SliderInt maxDepth{ "Max Depth", 2, 1, 10 };
		SliderInt denoiseKernelRadius{"Denoise Radius", 1, 0, 10};
		SliderFloat bilateralDenoiseSigmaRange{"Denoise Sigma Range", 0.1f, 0.0f, 10.0f};
		SliderFloat bilateralDenoiseSigmaSpatial{"Denoise Sigma Spatial", 1.0f, 0.0f, 10.0f};
		CheckBox useDenoise{"Use Denoise", false};
		CheckBox useTonemapping{"Use Reinhard Tone Mapping", false};
		SliderFloat minLuminance{"Tone Mapping Min Luminance", 2.0f, 0.1f, 30.0f};
		SliderInt2 imageResolution{ "Resolution", glm::ivec2(512, 512), 1, 4096 };

		// Filters
		FilterSettings filterSettings;

		virtual void begin() override
		{
			GUISettings::begin();

			if (ImGui::TreeNode("Reconstruction Filter"))
			{
				filterSettings.filterType.begin();
				filterSettings.radius.begin();
				filterSettings.resetRadiusButton.begin();

				switch (filterSettings.filterType.getEnumValue())
				{
				case EFilterType::Box:
					break;
				case EFilterType::Triangle:
					break;
				case EFilterType::Gaussian:
					filterSettings.gaussianAlpha.begin();
					break;
				case EFilterType::Mitchell:
				{
					float oldB = filterSettings.mitchellB;
					float oldC = filterSettings.mitchellC;
					filterSettings.mitchellB.begin();
					filterSettings.mitchellC.begin();

					if (oldB != filterSettings.mitchellB)
						filterSettings.mitchellC.value = (1.0f - filterSettings.mitchellB) / 2.0f;

					if (oldC != filterSettings.mitchellC)
						filterSettings.mitchellB.value = 1.0f - 2.0f * filterSettings.mitchellC;
					break;
				}
				case EFilterType::LanczosSinc:
					filterSettings.lanczosSincTau.begin();
					break;
				default:
					break;
				}

				ImGui::TreePop();
			}
			 
			filterSettings.gaussianExpX = std::exp(-filterSettings.gaussianAlpha * filterSettings.radius.value.x * filterSettings.radius.value.x);
			filterSettings.gaussianExpY = std::exp(-filterSettings.gaussianAlpha * filterSettings.radius.value.y * filterSettings.radius.value.y);
		}

		virtual void end() override
		{
			GUISettings::end();
		}

	};

	struct PipelineSettings : GUISettings
	{
		PipelineSettings()
		{
			guiElements.insert(guiElements.end(), { &pipeline });
		}

		ComboBoxEnum<EPathTracerPipeline> pipeline{"Pipeline", { "RegularPathTracer", "BidirectionalPathTracer", "Rasterization" }, 2};
	};

	struct IntersectionAPISettings : GUISettings
	{
		IntersectionAPISettings()
		{
			guiElements.insert(guiElements.end(), { &bvh.accelerationStructure, &bvh.force2Level, &bvh.forceFlat });

			fatBvhGuiElements.insert(fatBvhGuiElements.end(), { &bvh.builder, &bvh.traversalCost, &bvh.numBins });
			twoLevelBvhGuiElements.insert(twoLevelBvhGuiElements.end(), { &bvh.builder, &bvh.traversalCost, &bvh.numBins });
			bvhGuiElements.insert(bvhGuiElements.end(), { &bvh.builder, &bvh.traversalCost, &bvh.numBins,
				&bvh.useSplits, &bvh.maxSplitDepth, &bvh.minOverlap, &bvh.extraNodeBudget });
		}

		void begin() override
		{
			for (auto e : guiElements)
				e->begin();

			if (!bvh.force2Level)
			{
				switch (bvh.accelerationStructure.getEnumValue())
				{
				case ERTAccelerationStructureType::BVH:
					for (auto e : bvhGuiElements)
						e->begin();
					break;
				case ERTAccelerationStructureType::FatBVH:
					for (auto e : fatBvhGuiElements)
						e->begin();
					break;
				case ERTAccelerationStructureType::HLBVH:
					break;
				default:
					break;
				}
			}
			else
			{
				for (auto e : twoLevelBvhGuiElements)
					e->begin();
			}

			refreshApiButton.begin();
		}

		void end() override
		{
				for (auto e : guiElements)
					e->end();

				if (!bvh.force2Level)
				{
					switch (bvh.accelerationStructure.getEnumValue())
					{
					case ERTAccelerationStructureType::BVH:
						for (auto e : bvhGuiElements)
							e->end();
						break;
					case ERTAccelerationStructureType::FatBVH:
						for (auto e : fatBvhGuiElements)
							e->end();
						break;
					case ERTAccelerationStructureType::HLBVH:
						break;
					default:
						break;
					}
				}
				else
				{
					for (auto e : twoLevelBvhGuiElements)
						e->end();
				}

				refreshApiButton.end();
		}

		std::vector<GUIElement*> bvhGuiElements;
		std::vector<GUIElement*> fatBvhGuiElements;
		std::vector<GUIElement*> twoLevelBvhGuiElements;

		struct BVHGuiElements
		{
			CheckBox force2Level{"Force 2 Level", false};
			CheckBox forceFlat{"Force Flat", false};
			ComboBoxEnum<ERTAccelerationStructureType> accelerationStructure{ "Acceleration Structure",{ "BVH", "FatBVH" }, 1 };
			ComboBoxEnum<ERTBVHBuilderType> builder{ "Builder",{ "SAH", "Median" }, 0 };
			SliderFloat traversalCost{"Traversal Cost", 10.0f, 1.0f, 100.0f};
			SliderInt numBins{"Num Bins", 64, 1, 256};

			CheckBox useSplits{"Use Splits", false};
			SliderInt maxSplitDepth{"Max Split Depth", 10, 6, 18};
			SliderFloat minOverlap{"Min Overlap", 0.05f, 0.0f, 0.2f};
			SliderFloat extraNodeBudget{"Extra Node Budget", 0.5f, 0.0f, 1.0f};
		};

		BVHGuiElements bvh;

		Button refreshApiButton{"Refresh API"};
	};

    extern Demo DEMO;
    extern GISettings GI;
	extern PipelineSettings PIPELINE;
	extern IntersectionAPISettings INTERSECTION_API;
}
