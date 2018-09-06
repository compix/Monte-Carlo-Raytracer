#pragma once
#include "CLWPlatform.h"
#include "CLWDevice.h"
#include "unordered_map"
#include "CLWContext.h"

#define INVALID_DEVICE_INDEX -1

class PlatformManager
{
public:
	static void init();

	/**
	* The best device is trivially defined as the device with the highest memory.
	*/
	static int getBestDeviceIdx();

	/**
	* Supported devices are GPU devices with OpenGL interoperability.
	*/
	static const std::vector<CLWDevice>& getSupportedDevices();
	static int getDeviceIndex(const CLWDevice& device);
	static CLWDevice& getDevice(int deviceIdx);

	/**
	* By default the active device is the device with index getBestDeviceIdx().
	* The user can select a different device however.
	*/
	static CLWDevice* getActiveDevice();
	static void setActiveDeviceIdx(int deviceIdx);
	static int getActiveDeviceIdx();
	static CLWPlatform* getActiveDevicePlatform();
	static CLWContext createCLContextWithGLInterop();
private:
	static std::vector<CLWPlatform> m_platforms;
	static std::vector<CLWDevice> m_supportedDevices;
	static std::unordered_map<int, int> m_deviceToPlatformMap; // Map platforms (value) to devices (key)
	static int m_activeDeviceIdx;
	static bool m_initialized;
};