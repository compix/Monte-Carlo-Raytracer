#include <engine/Engine.h>
#include <memory>
#include <engine/util/Logger.h>
#include "application/PathTracer/PathTracingApp.h"
#include "engine/rendering/Screen.h"
#include <stdlib.h>

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32)
int setenv(const char *name, const char *value, int overwrite)
{
	int errcode = 0;
	if (!overwrite) {
		size_t envsize = 0;
		errcode = getenv_s(&envsize, NULL, 0, name);
		if (errcode || envsize) return errcode;
	}
	return _putenv_s(name, value);
}
#endif 

int main(int, char**)
{
	setenv("CUDA_CACHE_DISABLE", "1", 1);

    std::unique_ptr<Engine> engine = std::make_unique<Engine>();
    std::unique_ptr<PathTracingApp> game = std::make_unique<PathTracingApp>();

    engine->init(game.get());

    while (engine->running())
    {
        engine->update();
    }
	
    engine->shutdown();

    return 0;
}