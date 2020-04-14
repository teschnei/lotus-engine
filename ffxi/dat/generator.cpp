#include "generator.h"

namespace FFXI
{
    Keyframe::Keyframe(char* _name, uint8_t* _buffer, size_t _len) : DatChunk(_name, _buffer, _len)
    {
        intervals.resize(len / (sizeof(float) * 2));
        memcpy(intervals.data(), buffer, len);
    }

    Generator::Generator(char* _name, uint8_t* _buffer, size_t _len) : DatChunk(_name, _buffer, _len)
    {
        header = (GeneratorHeader*)buffer;

        uint8_t* data2 = buffer + header->offset2 - 16;

        while (data2 < buffer + header->offset3 - 16)
        {
            uint8_t data_type = *data2;
            data2 += 4;
            switch (data_type)
            {
            case 0x00:
                data2 += 0;
                break;
            case 0x01:
                billboard = *(uint16_t*)(data2);
                id = std::string((char*)data2 + 8, 4);
                pos = *(glm::vec3*)(data2 + 16);
                effect_type = *(uint8_t*)(data2 + 29);
                lifetime = *(uint16_t*)(data2 + 30);
                data2 += 44;
                break;
            case 0x02:
                dpos = *(glm::vec3*)(data2);
                data2 += 12;
                break;
            case 0x03:
                dpos_fluctuation = *(glm::vec3*)(data2);
                data2 += 12;
                break;
            case 0x06:
                data2 += 12;
                break;
            case 0x07:
                data2 += 28;
                break;
            case 0x08:
                dpos_origin = *(float*)(data2);
                data2 += 4;
                break;
            case 0x09:
                rot = *(glm::vec3*)(data2);
                data2 += 12;
                break;
            case 0x0a:
                rot_fluctutation = *(glm::vec3*)(data2);
                data2 += 12;
                break;
            case 0x0b:
                drot = *(glm::vec3*)(data2);
                data2 += 12;
                break;
            case 0x0c:
                drot_fluctuation = *(glm::vec3*)(data2);
                data2 += 12;
                break;
            case 0x0f:
                scale = *(glm::vec3*)(data2);
                data2 += 12;
                break;
            case 0x10:
                scale_fluctuation = *(glm::vec3*)(data2);
                data2 += 12;
                break;
            case 0x11:
                scale_all_fluctuation = *(float*)(data2);
                data2 += 4;
                break;
            case 0x12:
                data2 += 12;
                break;
            case 0x13:
                data2 += 12;
                break;
            case 0x16:
                color = *(uint32_t*)(data2);
                data2 += 4;
                break;
            case 0x17:
                data2 += 4;
                break;
            case 0x18:
                data2 += 4;
                break;
            case 0x19:
                data2 += 8;
                break;
            case 0x1a:
                data2 += 8;
                break;
            case 0x1d:
                data2 += 4;
                break;
            case 0x1e:
                data2 += 4;
                break;
            case 0x1f:
                gen_radius_fluctuation = *(float*)(data2);
                gen_radius = *(float*)(data2 + 4);
                gen_rot = *(glm::vec3*)(data2 + 8);
                gen_rot2 = *(glm::vec2*)(data2 + 20);
                gen_height = *(float*)(data2 + 28);
                gen_height_fluctuation = *(float*)(data2 + 32);
                rotations = *(uint32_t*)(data2 + 40);
                data2 += 44;
                break;
            case 0x21:
                kf_x_pos = std::string((char*)data2 + 4, 4);
                data2 += 12;
                break;
            case 0x22:
                kf_y_pos = std::string((char*)data2 + 4, 4);
                data2 += 12;
                break;
            case 0x23:
                kf_z_pos = std::string((char*)data2 + 4, 4);
                data2 += 12;
                break;
            case 0x24:
                kf_z_rot = std::string((char*)data2 + 4, 4);
                data2 += 12;
                break;
            case 0x25:
                kf_z_rot = std::string((char*)data2 + 4, 4);
                data2 += 12;
                break;
            case 0x26:
                kf_z_rot = std::string((char*)data2 + 4, 4);
                data2 += 12;
                break;
            case 0x27:
                kf_x_scale = std::string((char*)data2 + 4, 4);
                data2 += 12;
                break;
            case 0x28:
                kf_y_scale = std::string((char*)data2 + 4, 4);
                data2 += 12;
                break;
            case 0x29:
                kf_z_scale = std::string((char*)data2 + 4, 4);
                data2 += 12;
                break;
            case 0x2a:
                kf_r = std::string((char*)data2 + 4, 4);
                data2 += 12;
                break;
            case 0x2b:
                kf_g = std::string((char*)data2 + 4, 4);
                data2 += 12;
                break;
            case 0x2c:
                kf_b = std::string((char*)data2 + 4, 4);
                data2 += 12;
                break;
            case 0x2d:
                kf_a = std::string((char*)data2 + 4, 4);
                data2 += 12;
                break;
            case 0x2e:
                kf_u = std::string((char*)data2 + 4, 4);
                data2 += 12;
                break;
            case 0x2f:
                kf_v = std::string((char*)data2 + 4, 4);
                data2 += 12;
                break;
            case 0x30:
                data2 += 4;
                break;
            case 0x31:
                data2 += 4;
                break;
            case 0x32:
                data2 += 8;
                break;
            case 0x33:
                data2 += 12;
                break;
            case 0x34:
                data2 += 12;
                break;
            case 0x35:
                data2 += 12;
                break;
            case 0x36:
                data2 += 12;
                break;
            case 0x39:
                data2 += 12;
                break;
            case 0x3a:
                data2 += 36;
                break;
            case 0x3b:
                gen_rot_add = *(glm::vec3*)(data2);
                data2 += 12;
                break;
            case 0x3c:
                data2 += 8;
                break;
            case 0x3d:
                data2 += 0;
                break;
            case 0x3e:
                data2 += 8;
                break;
            case 0x3f:
                data2 += 8;
                break;
            case 0x40:
                data2 += 8;
                break;
            case 0x41:
                data2 += 4;
                break;
            case 0x42:
                data2 += 8;
                break;
            case 0x43:
                data2 += 4;
                break;
            case 0x44:
                data2 += 8;
                break;
            case 0x45:
                data2 += 0;
                break;
            case 0x47:
                data2 += 0;
                break;
            case 0x49:
                data2 += 0;
                break;
            case 0x4C:
                data2 += 12;
                break;
            case 0x4F:
                data2 += 12;
                break;
            case 0x53:
                data2 += 8;
                break;
            case 0x54:
                data2 += 20;
                break;
            case 0x55:
                data2 += 36;
                break;
            case 0x56:
                data2 += 4;
                break;
            case 0x58:
                data2 += 16;
                break;
            case 0x59:
                data2 += 12;
                break;
            case 0x5A:
                data2 += 12;
                break;
            case 0x5B:
                data2 += 12;
                break;
            case 0x60:
                data2 += 12;
                break;
            case 0x61:
                data2 += 12;
                break;
            case 0x62:
                data2 += 12;
                break;
            case 0x63:
                data2 += 12;
                break;
            case 0x67:
                data2 += 4;
                break;
            case 0x6A:
                data2 += 8;
                break;
            case 0x6C:
                data2 += 12;
                break;
            case 0x72:
                data2 += 8;
                break;
            case 0x78:
                data2 += 12;
                break;
            case 0x82:
                data2 += 20;
                break;
            case 0x87:
                data2 += 0;
                break;
            case 0x8F:
                data2 += 0;
                break;
            }
        }
    }
}