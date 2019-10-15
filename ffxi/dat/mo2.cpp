#include "mo2.h"
#include <glm/gtc/quaternion.hpp>

struct Animation
{
    uint16_t _pad;
    uint16_t elements;
    uint16_t frames;
    float speed;
};

struct Element
{
    uint32_t bone;
    uint32_t quat[4];
    glm::quat quat_base;
    uint32_t trans[3];
    glm::vec3 trans_base;
    uint32_t scale[3];
    glm::vec3 scale_base;
};

FFXI::MO2::MO2(uint8_t* buffer, size_t max_len, char _name[4]) : name(_name, 4)
{
}
