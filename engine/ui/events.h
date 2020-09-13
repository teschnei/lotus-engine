#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>

namespace lotus::ui
{
    class Element;
    class EventListener
    {
    public:
        EventListener(Element* _e, std::function<void(Element*)> _cb) : element(_e), callback(_cb) {}
        Element* element {nullptr};
        std::function<void(Element*)> callback;

        //TODO: make this lua only? vararg arguments
        void call()
        {
            callback(element);
        }
    };

    class Events
    {
    public:
        Events() {}

        void trigger(std::string event);

    private:
        std::unordered_map<std::string, std::vector<EventListener>> event_listeners;
    };
}

