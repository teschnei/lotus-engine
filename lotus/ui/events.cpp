#include "events.h"

namespace lotus::ui
{
void Events::trigger(std::string event)
{
    if (auto ev = event_listeners.find(event); ev != event_listeners.end())
    {
        for (auto& e : ev->second)
        {
            e.call();
        }
    }
}
} // namespace lotus::ui
