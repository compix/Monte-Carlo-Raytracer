#include "KernelManager.h"
#include <sstream>
#include "../../../../engine/util/file.h"
#include "../source/engine/util/Logger.h"

std::vector<std::string> KernelManager::m_includeDirectories;

std::unordered_map<std::string, CLWProgram> KernelManager::m_programs;

std::vector<std::function<void()>> KernelManager::m_recompilationListeners;

void KernelManager::init()
{
}

void KernelManager::addIncludeDirectory(const std::string& absolutePath)
{
	m_includeDirectories.push_back(absolutePath);
}

CLWProgram KernelManager::getProgram(const std::string& programName, CLWContext context)
{
	auto findResult = m_programs.find(programName);

	if (findResult != m_programs.end())
		return findResult->second;

	// Create program and add to program map
	createAndAddProgram(programName, context);
	return m_programs[programName];
}

void KernelManager::createAndAddProgram(const std::string& programName, CLWContext context)
{
	std::stringstream buildOptions;
	std::string includeDir = std::string(ASSET_ROOT_FOLDER) + "kernels/";
	// -cl-mad-enable -cl-fast-relaxed-math 
	buildOptions << "-cl-mad-enable -cl-fast-relaxed-math -cl-std=CL1.2 -I " << includeDir;
	for (auto& incDir : m_includeDirectories)
	{
		buildOptions << "-I " << incDir;
	}
	std::string kernelPath = std::string(ASSET_ROOT_FOLDER) + std::string("kernels/") + programName + ".cl";
	auto program = CLWProgram::CreateFromFile(kernelPath.c_str(), buildOptions.str().c_str(), context);
	m_programs[programName] = program;

	// Show build log
	std::vector<char> buildLog;
	size_t logSize;
	clGetProgramBuildInfo(program, context.GetDevice(0), CL_PROGRAM_BUILD_LOG, 0, nullptr, &logSize);

	buildLog.resize(logSize);
	clGetProgramBuildInfo(program, context.GetDevice(0), CL_PROGRAM_BUILD_LOG, logSize, &buildLog[0], nullptr);

	if (logSize > 0)
	{
		LOG("~~~~~ BUILD LOG " << programName << " ~~~~~");
		LOG(std::string(&buildLog[0]));
	}
}

void KernelManager::recompilePrograms(CLWContext context)
{
	try 
	{
		for (auto& p : m_programs)
		{
			createAndAddProgram(p.first, context);
		}
	}
	catch (std::exception e)
	{
		LOG_ERROR(e.what());
		return;
	}

	for (auto& l : m_recompilationListeners)
		l();
}
