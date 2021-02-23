#include "scheduler.h"

FFXI::Scheduler::Scheduler(char* _name, uint8_t* _buffer, size_t _len) : DatChunk(_name, _buffer, _len)
{
    header = (SchedulerHeader*)buffer;

    data = buffer + sizeof(SchedulerHeader);
}

std::pair<uint8_t*, uint32_t> FFXI::Scheduler::getStage(uint32_t stage)
{
    uint8_t* ret = data;
    uint32_t current_stage = 0;
    uint32_t frame = 0;
    while (ret < buffer + sizeof(SchedulerHeader) + len && current_stage < stage)
    {
        uint8_t type = *ret;
        uint8_t length = *(ret + 1) * sizeof(uint32_t);
        uint16_t delay = *(uint16_t*)(ret + 4);
        uint16_t duration = *(uint16_t*)(ret + 6);
        char* id = (char*)(ret + 8);

        if (len == 0)
            return { nullptr, 0 };

        ret += length;
        frame += delay;
        current_stage++;
    }
    return { ret, frame };
}
