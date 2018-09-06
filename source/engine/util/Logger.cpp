#include "Logger.h"

Logger::StreamKey Logger::m_curAvailableStrStreamKey = 0;
std::unordered_map<Logger::StreamKey, std::stringstream> Logger::m_stringStreams;

Logger::Logger() {}
void Logger::init()
{
    m_stringStreams.insert({ PATH_TRACER_STRING_STREAM, std::stringstream() });
}
void Logger::log(const std::string& msg) noexcept { stream() << msg << std::endl; }
