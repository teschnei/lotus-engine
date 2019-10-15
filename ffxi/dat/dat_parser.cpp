#include "dat_parser.h"
#include <fstream>

#pragma pack(push,1)
typedef struct 
{
  char id[4];
  unsigned long type:7;
  unsigned long next:19;
  unsigned long is_shadow:1;
  unsigned long is_extracted:1;
  unsigned long ver_num:3;
  unsigned long is_virtual:1;
  unsigned int parent;
  unsigned int child;
} DATHEAD;
#pragma pack(pop)

DatParser::DatParser(const std::string& filepath)
{
    std::ifstream dat{filepath, std::ios::ate | std::ios::binary };

    size_t fileSize = (size_t) dat.tellg();
    std::vector<uint8_t> buffer(fileSize);

    dat.seekg(0);
    dat.read((char*)buffer.data(), fileSize);
    dat.close();

    int terminate = 0;
    int rmp = 0;
    int rmw = 0;
    int directory = 0;
    int bin = 0;
    int generator = 0;
    int camera = 0;
    int scheduler = 0;
    int mtx = 0;
    int tim = 0;
    int texInfo = 0;
    int vum = 0;
    int om1 = 0;
    int fileInfo = 0;
    int anm = 0;
    int rsd = 0;
    int unknown = 0;
    int osm = 0;
    int skd = 0;
    int mtd = 0;
    int mld = 0;
    int mlt = 0;
    int mws = 0;
    int mod = 0;
    int tim2 = 0;
    int keyframe = 0;
    int bmp = 0;
    int bmp2 = 0;
    int mzb = 0;
    int mmd = 0;
    int mep = 0;
    int d3m = 0;
    int d3s = 0;
    int d3a = 0;
    int distProg = 0;
    int vuLineProg = 0;
    int ringProg = 0;
    int d3b = 0;
    int asn = 0;
    int mot = 0;
    int skl = 0;
    int sk2 = 0;
    int os2 = 0;
    int mo2 = 0;
    int psw = 0;
    int wsd = 0;
    int mmb = 0;
    int weather = 0;
    int meb = 0;
    int msb = 0;
    int med = 0;
    int msh = 0;
    int ysh = 0;
    int mbp = 0;
    int rid = 0;
    int wd = 0;
    int bgm = 0;
    int lfd = 0;
    int lfe = 0;
    int esh = 0;
    int sch = 0;
    int sep = 0;
    int vtx = 0;
    int lwo = 0;
    int rme = 0;
    int elt = 0;
    int rab = 0;
    int mtt = 0;
    int mtb = 0;
    int cib = 0;
    int tlt = 0;
    int pointLightProg = 0;
    int mgd = 0;
    int mgb = 0;
    int sph = 0;
    int bmd = 0;
    int qif = 0;
    int qdt = 0;
    int mif = 0;
    int mdt = 0;
    int sif = 0;
    int sdt = 0;
    int acd = 0;
    int acb = 0;
    int afb = 0;
    int aft = 0;
    int wwd = 0;
    int nullProg = 0;
    int spw = 0;
    int fud = 0;
    int disgregaterProg = 0;
    int smt = 0;
    int damValueProg = 0;
    int bp = 0;

    int offset = 0;
    while(offset < buffer.size())
    {
        DATHEAD* dathead = (DATHEAD*)&buffer[offset];
        int len = (dathead->next & 0x7ffff) * 16;

        switch (dathead->type)
        {
        case 0x00:
            terminate++;
            break;
        case 0x01:
            rmp++;
            break;
        case 0x02:
            rmw++;
            break;
        case 0x03:
            directory++;
            break;
        case 0x04:
            bin++;
            break;
        case 0x05:
            generator++;
            break;
        case 0x06:
            camera++;
            break;
        case 0x07:
            scheduler++;
            break;
        case 0x08:
            mtx++;
            break;
        case 0x09:
            tim++;
            break;
        case 0x0A:
            texInfo++;
            break;
        case 0x0B:
            vum++;
            break;
        case 0x0C:
            om1++;
            break;
        case 0x0D:
            fileInfo++;
            break;
        case 0x0E:
            anm++;
            break;
        case 0x0F:
            rsd++;
            break;
        case 0x10:
            unknown++;
            break;
        case 0x11:
            osm++;
            break;
        case 0x12:
            skd++;
            break;
        case 0x13:
            mtd++;
            break;
        case 0x14:
            mld++;
            break;
        case 0x15:
            mlt++;
            break;
        case 0x16:
            mws++;
            break;
        case 0x17:
            mod++;
            break;
        case 0x18:
            tim2++;
            break;
        case 0x19:
            keyframe++;
            break;
        case 0x1A:
            bmp++;
            break;
        case 0x1B:
            bmp2++;
            break;
        case 0x1C:
            mzb++;
            if (FFXI::MZB::DecodeMZB(&buffer[offset + sizeof(DATHEAD)], len - sizeof(DATHEAD)))
            {
                mzbs.push_back(std::make_unique<FFXI::MZB>(&buffer[offset + sizeof(DATHEAD)], len - (sizeof(DATHEAD))));
            }
            break;
        case 0x1D:
            mmd++;
            break;
        case 0x1E:
            mep++;
            break;
        case 0x1F:
            d3m++;
            break;
        case 0x20:
            d3s++;
            dxt3s.push_back(std::make_unique<FFXI::DXT3>(&buffer[offset + sizeof(DATHEAD)]));
            break;
        case 0x21:
            d3a++;
            break;
        case 0x22:
            distProg++;
            break;
        case 0x23:
            vuLineProg++;
            break;
        case 0x24:
            ringProg++;
            break;
        case 0x25:
            d3b++;
            break;
        case 0x26:
            asn++;
            break;
        case 0x27:
            mot++;
            break;
        case 0x28:
            skl++;
            break;
        case 0x29:
            sk2++;
            sk2s.push_back(std::make_unique<FFXI::SK2>(&buffer[offset + sizeof(DATHEAD)], len - sizeof(DATHEAD)));
            break;
        case 0x2A:
            os2++;
            os2s.push_back(std::make_unique<FFXI::OS2>(&buffer[offset + sizeof(DATHEAD)], len - sizeof(DATHEAD)));
            break;
        case 0x2B:
            mo2++;
            mo2s.push_back(std::make_unique<FFXI::MO2>(&buffer[offset + sizeof(DATHEAD)], len - sizeof(DATHEAD), dathead->id));
            break;
        case 0x2C:
            psw++;
            break;
        case 0x2D:
            wsd++;
            break;
        case 0x2E:
            mmb++;
            if (FFXI::MMB::DecodeMMB(&buffer[offset + sizeof(DATHEAD)], len - sizeof(DATHEAD)))
            {
                mmbs.push_back(std::make_unique<FFXI::MMB>(&buffer[offset + sizeof(DATHEAD)], len - (sizeof(DATHEAD))));
            }
            break;
        case 0x2F:
            weather++;
            break;
        case 0x30:
            meb++;
            break;
        case 0x31:
            msb++;
            break;
        case 0x32:
            med++;
            break;
        case 0x33:
            msh++;
            break;
        case 0x34:
            ysh++;
            break;
        case 0x35:
            mbp++;
            break;
        case 0x36:
            rid++;
            break;
        case 0x37:
            wd++;
            break;
        case 0x38:
            bgm++;
            break;
        case 0x39:
            lfd++;
            break;
        case 0x3A:
            lfe++;
            break;
        case 0x3B:
            esh++;
            break;
        case 0x3C:
            sch++;
            break;
        case 0x3D:
            sep++;
            break;
        case 0x3E:
            vtx++;
            break;
        case 0x3F:
            lwo++;
            break;
        case 0x40:
            rme++;
            break;
        case 0x41:
            elt++;
            break;
        case 0x42:
            rab++;
            break;
        case 0x43:
            mtt++;
            break;
        case 0x44:
            mtb++;
            break;
        case 0x45:
            cib++;
            break;
        case 0x46:
            tlt++;
            break;
        case 0x47:
            pointLightProg++;
            break;
        case 0x48:
            mgd++;
            break;
        case 0x49:
            mgb++;
            break;
        case 0x4A:
            sph++;
            break;
        case 0x4B:
            bmd++;
            break;
        case 0x4C:
            qif++;
            break;
        case 0x4D:
            qdt++;
            break;
        case 0x4E:
            mif++;
            break;
        case 0x4F:
            mdt++;
            break;
        case 0x50:
            sif++;
            break;
        case 0x51:
            sdt++;
            break;
        case 0x52:
            acd++;
            break;
        case 0x53:
            acb++;
            break;
        case 0x54:
            afb++;
            break;
        case 0x55:
            aft++;
            break;
        case 0x56:
            wwd++;
            break;
        case 0x57:
            nullProg++;
            break;
        case 0x58:
            spw++;
            break;
        case 0x59:
            fud++;
            break;
        case 0x5A:
            disgregaterProg++;
            break;
        case 0x5B:
            smt++;
            break;
        case 0x5C:
            damValueProg++;
            break;
        case 0x5D:
            bp++;
            break;
        default:
            break;
        }
        offset += len;
    }
}
