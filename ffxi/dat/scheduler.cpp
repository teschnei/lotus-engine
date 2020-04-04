#include "scheduler.h"

FFXI::Scheduler::Scheduler(char* _name, uint8_t* _buffer, size_t _len) : DatChunk(_name, _buffer, _len)
{
    header = (SchedulerHeader*)buffer;

    uint8_t* data = buffer + sizeof(SchedulerHeader);

    while (data < buffer + sizeof(SchedulerHeader) + len)
    {
        uint8_t type = *data;
        uint8_t length = *(data + 1) * sizeof(uint32_t);

        switch (type)
        {
        case 0x00:
        {
            //end
            data = data + len;
            break;
        }
        case 0x02:
        {
            //generator
            char* id = (char*)(data + 8);
            break;
        }
        case 0x03:
        {
            //scheduler
            char* id = (char*)(data + 8);
            break;
        }
        case 0x05:
        {
            //motion
            char* id = (char*)(data + 8);
            break;
        }
        }

        data += length;
    }
}
