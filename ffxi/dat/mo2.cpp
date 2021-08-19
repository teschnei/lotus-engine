#include "mo2.h"

#pragma pack(push,2)
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
    glm::vec<4, int32_t> quat;
    glm::quat quat_base;
    glm::vec<3, int32_t> trans;
    glm::vec3 trans_base;
    glm::vec<3, int32_t> scale;
    glm::vec3 scale_base;
};

#pragma pack(pop)

FFXI::MO2::MO2(char* _name, uint8_t* _buffer, size_t _len) : DatChunk(_name, _buffer, _len)
{
    name = std::string(_name, 3);
    Animation* header = reinterpret_cast<Animation*>(buffer);
    Element* elements = reinterpret_cast<Element*>(header + 1);
    float* data = reinterpret_cast<float*>(elements);

    frames = header->frames;
    speed = header->speed;

    glm::quat rot;
    glm::vec3 trans;
    glm::vec3 scale;

    for(uint16_t e = 0; e < header->elements; ++e)
    {
        Element* ele = elements + e;

        if (!(ele->quat.x < 0 || ele->quat.y < 0 || ele->quat.z < 0 || ele->quat.w < 0))
        {
            for (uint16_t f = 0; f < header->frames; ++f)
            {
                rot.x = ele->quat.x > 0 ? data[ele->quat.x + f] : ele->quat_base.x;
                rot.y = ele->quat.y > 0 ? data[ele->quat.y + f] : ele->quat_base.y;
                rot.z = ele->quat.z > 0 ? data[ele->quat.z + f] : ele->quat_base.z;
                rot.w = ele->quat.w > 0 ? data[ele->quat.w + f] : ele->quat_base.w;

                trans.x = ele->trans.x > 0 ? data[ele->trans.x + f] : ele->trans_base.x;
                trans.y = ele->trans.y > 0 ? data[ele->trans.y + f] : ele->trans_base.y;
                trans.z = ele->trans.z > 0 ? data[ele->trans.z + f] : ele->trans_base.z;

                scale.x = ele->scale.x > 0 ? data[ele->scale.x + f] : ele->scale_base.x;
                scale.y = ele->scale.y > 0 ? data[ele->scale.y + f] : ele->scale_base.y;
                scale.z = ele->scale.z > 0 ? data[ele->scale.z + f] : ele->scale_base.z;

                animation_data[ele->bone].push_back({ rot, trans, scale });
            }
        }
        else
        {
            for (uint16_t f = 0; f < header->frames; ++f)
            {
                animation_data[ele->bone].push_back({ glm::quat{1, 0, 0, 0}, glm::vec3{0}, glm::vec3{1} });
            }
        }
    }
    //animations don't use frame 0 (FFXI thing?)
    frames = header->frames - 1;
    for (auto& [frame, frame_data] : animation_data)
    {
        frame_data.erase(frame_data.begin());
    }
}
