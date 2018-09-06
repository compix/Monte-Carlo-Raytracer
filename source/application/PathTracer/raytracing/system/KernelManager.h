#pragma once
#include "string"
#include "vector"
#include "CLW.h"
#include "unordered_map"
#include "functional"

class KernelManager
{
public:
	static void init();

	static void addIncludeDirectory(const std::string& absolutePath);

	static CLWProgram getProgram(const std::string& programName, CLWContext context);
	static void recompilePrograms(CLWContext context);

	static void addOnRecompilationListener(std::function<void()> listener)
	{
		m_recompilationListeners.push_back(listener);
	}
private:
	static void createAndAddProgram(const std::string& programName, CLWContext context);

	static std::vector<std::string> m_includeDirectories;
	static std::unordered_map<std::string, CLWProgram> m_programs;

	static std::vector<std::function<void()>> m_recompilationListeners;
};