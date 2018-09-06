#pragma once
#include "engine/rendering/architecture/RenderPass.h"
#include <CLW.h>
#include <radeon_rays.h>
#include "engine/ecs/ECS.h"
#include "../kernels/RTKernel.h"
#include "engine/rendering/renderer/SimpleMeshRenderer.h"
#include "engine/rendering/shader/Shader.h"
#include "engine/rendering/Texture2D.h"
#include "kernel_data.h"
#include "../textures/RTInteropTexture2D.h"

class RTBDPTPass : public RenderPass
{
public:

	RTBDPTPass();

	virtual void update() override;

private:
	/**
	* A float* buffer is used in the BDPT because atomic operations are necessary. They are not supported by float4*.
	* For reconstruction a float4* buffer is expected. This function copies the content of the float* buffer to a float4* buffer.
	*/
	void copyRadianceBuffer();

	void createBuffers();
	void setupKernels();
	int setSceneArgs(RTKernel& kernel, int sceneArgsStart = 0);
	int setImageArgs(RTKernel& kernel, int argsStart);

	void generateStartVertices();
	void generateSecondaryVertices();
	void prepareVertexConnections();
	void makeConnections();

	int getMaxPossibleConnectionsCount();

	CLWBuffer<RadeonRays::float4> m_radianceBuffer;
	CLWBuffer<float> m_finalRadianceBuffer;
	CLWBuffer<RadeonRays::float4> m_tempRadianceBuffer;
	RTKernel m_copyBufferKernel;

	RTKernel m_startVerticesGenerationKernel;
	RTKernel m_secondaryVerticesGenerationKernel;
	RTKernel m_prepareConnectionsKernel;
	RTKernel m_connectionKernel;

	// The camera path buffer has maxDepth + 2 vertices per pixel
	CLWBuffer<RTBDPTVertex> m_cameraVertices;
	// The light path buffer has maxDepth + 1 vertices per pixel
	CLWBuffer<RTBDPTVertex> m_lightVertices;

	CLWBuffer<RTBDPTVertex> m_sampledCameraVertices;
	CLWBuffer<RTBDPTVertex> m_sampledLightVertices;

	CLWBuffer<RadeonRays::ray> m_cameraRays;
	CLWBuffer<RTRayDifferentials> m_cameraRayDifferentials;
	CLWBuffer<RadeonRays::Intersection> m_cameraIntersections;
	CLWBuffer<RadeonRays::float3> m_cameraThroughputs;
	CLWBuffer<float> m_cameraFwdPdfs;
	CLWBuffer<int> m_cameraVertexCounts;

	CLWBuffer<RadeonRays::ray> m_lightRays;
	CLWBuffer<RadeonRays::Intersection> m_lightIntersections;
	CLWBuffer<RadeonRays::float3> m_lightThroughputs;
	CLWBuffer<float> m_lightFwdPdfs;
	CLWBuffer<int> m_lightVertexCounts;

	CLWBuffer<RadeonRays::ray> m_connectionRays;
	CLWBuffer<int> m_connectionVisibilities;

	int m_frameIndex = 0;
	int m_maxDepth;
	bool m_hasErrors = false;
	float m_totalRenderTime = 0.0f;
};
