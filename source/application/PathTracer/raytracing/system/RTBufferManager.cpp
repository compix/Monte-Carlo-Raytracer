#include "RTBufferManager.h"

void RTBufferManager::clearMemoryRecordContext(const std::string& contextName)
{
	m_memoryRecords[contextName].reset();
}

size_t RTBufferManager::getTotalAllocatedSize()
{
	size_t sum = 0;
	for (auto& p : m_memoryRecords)
		sum += p.second.totalAllocatedSize;

	return sum;
}

size_t RTBufferManager::getMaxAllocatedSize()
{
	size_t maxVal = 0;
	for (auto& p : m_memoryRecords)
		maxVal = std::max(maxVal, p.second.maxAllocatedSize);

	return maxVal;
}

std::unordered_map<std::string, RTMemoryRecord> RTBufferManager::m_memoryRecords;

std::string RTBufferManager::m_curMemoryContext = RT_MEMORY_RECORD_GLOBAL_CONTEXT;

