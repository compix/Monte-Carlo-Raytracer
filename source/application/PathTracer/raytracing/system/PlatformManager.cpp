#include "PlatformManager.h"
#include "engine/util/Logger.h"
#include "mutex"

#if defined(_WIN32)
#include <gl/wglew.h>
#elif defined(__unix__)
// TODO
#elif __APPLE__
// TODO
#endif

std::vector<CLWPlatform> PlatformManager::m_platforms;

std::vector<CLWDevice> PlatformManager::m_supportedDevices;

std::unordered_map<int, int> PlatformManager::m_deviceToPlatformMap;

bool PlatformManager::m_initialized = false;

int PlatformManager::m_activeDeviceIdx = -1;

void PlatformManager::init()
{
	// Platforms are OpenCL implementations which can vary in version and vendor.
	CLWPlatform::CreateAllPlatforms(m_platforms);

	if (m_platforms.size() == 0)
	{
		LOG_ERROR("No OpenCL platforms installed.");
		return;
	}

	// Devices are processors (CPU, GPU...) that support the implementation.
	// In this case we are only interested in GPU devices with GL interoperability which
	// are stored in m_supportedDevices. 
	for (int i = 0; i < m_platforms.size(); ++i)
	{
		for (int d = 0; d < (int)m_platforms[i].GetDeviceCount(); ++d)
		{
			CLWDevice curDevice = m_platforms[i].GetDevice(d);

			if (curDevice.GetType() != CL_DEVICE_TYPE_GPU || !curDevice.HasGlInterop())
			{
				LOG("Skipped incompatible device: " << curDevice.GetName());
				continue;
			}

			m_supportedDevices.push_back(curDevice);
			m_deviceToPlatformMap[static_cast<int>(m_supportedDevices.size()) - 1] = i;
		}
	}

	m_initialized = true;

	m_activeDeviceIdx = getBestDeviceIdx();
}

int PlatformManager::getBestDeviceIdx()
{
	if (!m_initialized)
	{
		return INVALID_DEVICE_INDEX;
	}

	size_t deviceMemSize = 0;
	int deviceIdx = INVALID_DEVICE_INDEX;
	for (size_t i = 0; i < m_supportedDevices.size(); ++i)
	{

		if (m_supportedDevices[i].GetGlobalMemSize() > deviceMemSize)
		{
			deviceIdx = static_cast<int>(i);
			deviceMemSize = m_supportedDevices[i].GetGlobalMemSize();
		}
	}

	return deviceIdx;
}

const std::vector<CLWDevice>& PlatformManager::getSupportedDevices()
{
	return m_supportedDevices;
}

int PlatformManager::getDeviceIndex(const CLWDevice& device)
{
	for (size_t i = 0; i < m_supportedDevices.size(); ++i)
	{
		if (m_supportedDevices[i].GetID() == device.GetID())
		{
			return static_cast<int>(i);
		}
	}

	return -1;
}

CLWDevice& PlatformManager::getDevice(int deviceIdx)
{
	assert(deviceIdx >= 0 && deviceIdx < m_supportedDevices.size());
	return m_supportedDevices[deviceIdx];
}

CLWDevice* PlatformManager::getActiveDevice()
{
	if (m_activeDeviceIdx >= 0 && m_activeDeviceIdx < m_supportedDevices.size())
	{
		return &getDevice(m_activeDeviceIdx);
	}

	return nullptr;
}

void PlatformManager::setActiveDeviceIdx(int deviceIdx)
{
	m_activeDeviceIdx = deviceIdx;
}

int PlatformManager::getActiveDeviceIdx()
{
	return m_activeDeviceIdx;
}

CLWPlatform* PlatformManager::getActiveDevicePlatform()
{
	int deviceIdx = getActiveDeviceIdx();
	if (deviceIdx != INVALID_DEVICE_INDEX)
	{
		return &m_platforms[m_deviceToPlatformMap[deviceIdx]];
	}

	return nullptr;
}

CLWContext PlatformManager::createCLContextWithGLInterop()
{
	// A context is a set of user defined devices that are available on the platform of the context.
	// Each device in the context has a command queue which is used to execute commands, e.g. user defined kernels. 

	auto platformPtr = getActiveDevicePlatform();
	cl_platform_id platform = 0;
	if (platformPtr)
		platform = *platformPtr;

#if defined(_WIN32)
	cl_context_properties props[] =
	{
		CL_CONTEXT_PLATFORM, (cl_context_properties)platform,
		CL_GL_CONTEXT_KHR, (cl_context_properties)wglGetCurrentContext(),
		CL_WGL_HDC_KHR, (cl_context_properties)wglGetCurrentDC(),
		0
	};
#elif defined(__unix__)
	cl_context_properties props[] =
	{
		CL_CONTEXT_PLATFORM, (cl_context_properties)platform,
		CL_GL_CONTEXT_KHR, (cl_context_properties)glXGetCurrentContext(),
		CL_WGL_HDC_KHR, (cl_context_properties)glXGetCurrentDrawable(),
		0
	};
#elif __APPLE__
	// TODO
#endif

	return CLWContext::Create(*getActiveDevice(), props);
}
