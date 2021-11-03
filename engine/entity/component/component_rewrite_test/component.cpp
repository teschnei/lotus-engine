#include "component.h"
#include "engine/core.h"

namespace lotus::Test
{
    void ComponentRunners::runQueries()
    {
        engine->renderer->runRaytracerQueries();
    }
}