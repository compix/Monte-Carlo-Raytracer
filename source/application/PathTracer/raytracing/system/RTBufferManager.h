#pragma once
#include "unordered_map"
#include <radeon_rays.h>
#include "../../../../../third_party/RadeonRays/CLW/CLWBuffer.h"
#include "../util/CLHelper.h"

struct RTMemoryRecord
{
	size_t maxAllocatedSize = 0;
	size_t totalAllocatedSize = 0;

	void reset()
	{
		maxAllocatedSize = 0;
		totalAllocatedSize = 0;
	}
};

#define RT_MEMORY_RECORD_GLOBAL_CONTEXT std::string("GlobalContext")

class RTBufferManager
{
public:
	template<class T>
	static CLWBuffer<T> createBuffer(cl_mem_flags flags, size_t elementCount, void* data = nullptr);

	static void setMamoryRecordContext(const std::string& contextName) { m_curMemoryContext = contextName; }

	static void clearMemoryRecordContext(const std::string& contextName);

	static size_t getTotalAllocatedSize();
	static size_t getMaxAllocatedSize();

	static std::string getCurrentContextName() { return m_curMemoryContext; }
private:
	static std::unordered_map<std::string, RTMemoryRecord> m_memoryRecords;
	static std::string m_curMemoryContext;
};

struct RTScopedMemoryRecord
{
	RTScopedMemoryRecord(const std::string& contextName, bool clearRecord = true)
	{
		if (clearRecord)
			RTBufferManager::clearMemoryRecordContext(contextName);

		prevMemoryContext = RTBufferManager::getCurrentContextName();
		RTBufferManager::setMamoryRecordContext(contextName);
	}

	~RTScopedMemoryRecord()
	{
		RTBufferManager::setMamoryRecordContext(prevMemoryContext);
	}

	std::string prevMemoryContext;
};

template<class T>
CLWBuffer<T> RTBufferManager::createBuffer(cl_mem_flags flags, size_t elementCount, void* data)
{
	auto& record = m_memoryRecords[m_curMemoryContext];

	size_t usedMemory;
	auto buffer = CLHelper::createBuffer<T>(g_clContext, flags, elementCount, usedMemory, data);

	record.maxAllocatedSize = std::max(record.maxAllocatedSize, usedMemory);
	record.totalAllocatedSize += usedMemory;

	return buffer;
}