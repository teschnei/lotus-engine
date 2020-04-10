#include "dat_parser.h"
#include "mzb.h"
#include "mmb.h"
#include "os2.h"
#include "mo2.h"
#include "sk2.h"
#include "dxt3.h"
#include "scheduler.h"
#include "generator.h"
#include "d3m.h"
#include <fstream>

namespace FFXI
{

#pragma pack(push,1)
    typedef struct
    {
        char id[4];
        unsigned long type : 7;
        unsigned long next : 19;
        unsigned long is_shadow : 1;
        unsigned long is_extracted : 1;
        unsigned long ver_num : 3;
        unsigned long is_virtual : 1;
        unsigned int parent;
        unsigned int child;
    } DATHEAD;
#pragma pack(pop)

    DatParser::DatParser(const std::string& filepath, bool _rtx) : rtx(_rtx)
    {
        std::ifstream dat{ filepath, std::ios::ate | std::ios::binary };

        if (!dat.good())
            throw std::exception("dat not found");

        size_t file_size = (size_t)dat.tellg();
        buffer.resize(file_size);

        dat.seekg(0);
        dat.read((char*)buffer.data(), file_size);
        dat.close();

        int offset = 0;
        DatChunk* current_chunk = nullptr;
        while (offset < buffer.size())
        {
            DATHEAD* dathead = (DATHEAD*)&buffer[offset];
            int len = (dathead->next & 0x7ffff) * 16;

            switch (dathead->type)
            {
                //terminate
            case 0x00:
                current_chunk = current_chunk->parent;
                break;
                //rmp
            case 0x01:
            {
                std::unique_ptr<DatChunk> new_chunk = std::make_unique<DatChunk>(dathead->id, &buffer[offset + sizeof(DATHEAD)], len - sizeof(DATHEAD));
                DatChunk* new_chunk_ptr = new_chunk.get();
                if (!root)
                {
                    root = std::move(new_chunk);
                }
                else
                {
                    new_chunk->parent = current_chunk;
                    current_chunk->children.push_back(std::move(new_chunk));
                }
                current_chunk = new_chunk_ptr;
            }
            break;
            case 0x02:
                current_chunk->rmw++;
                {
                    std::unique_ptr<DatChunk> new_chunk = std::make_unique<DatChunk>(dathead->id, &buffer[offset + sizeof(DATHEAD)], len - sizeof(DATHEAD));
                    new_chunk->parent = current_chunk;
                    current_chunk->children.push_back(std::move(new_chunk));
                }
                break;
            case 0x03:
                current_chunk->directory++;
                {
                    std::unique_ptr<DatChunk> new_chunk = std::make_unique<DatChunk>(dathead->id, &buffer[offset + sizeof(DATHEAD)], len - sizeof(DATHEAD));
                    new_chunk->parent = current_chunk;
                    current_chunk->children.push_back(std::move(new_chunk));
                }
                break;
            case 0x04:
                current_chunk->bin++;
                {
                    std::unique_ptr<DatChunk> new_chunk = std::make_unique<DatChunk>(dathead->id, &buffer[offset + sizeof(DATHEAD)], len - sizeof(DATHEAD));
                    new_chunk->parent = current_chunk;
                    current_chunk->children.push_back(std::move(new_chunk));
                }
                break;
            case 0x05:
                current_chunk->generator++;
                {
                    std::unique_ptr<DatChunk> new_chunk = std::make_unique<Generator>(dathead->id, &buffer[offset + sizeof(DATHEAD)], len - sizeof(DATHEAD));
                    new_chunk->parent = current_chunk;
                    current_chunk->children.push_back(std::move(new_chunk));
                }
                break;
            case 0x06:
                current_chunk->camera++;
                {
                    std::unique_ptr<DatChunk> new_chunk = std::make_unique<DatChunk>(dathead->id, &buffer[offset + sizeof(DATHEAD)], len - sizeof(DATHEAD));
                    new_chunk->parent = current_chunk;
                    current_chunk->children.push_back(std::move(new_chunk));
                }
                break;
            case 0x07:
                current_chunk->scheduler++;
                {
                    std::unique_ptr<DatChunk> new_chunk = std::make_unique<Scheduler>(dathead->id, &buffer[offset + sizeof(DATHEAD)], len - sizeof(DATHEAD));
                    new_chunk->parent = current_chunk;
                    current_chunk->children.push_back(std::move(new_chunk));
                }
                break;
            case 0x08:
                current_chunk->mtx++;
                {
                    std::unique_ptr<DatChunk> new_chunk = std::make_unique<DatChunk>(dathead->id, &buffer[offset + sizeof(DATHEAD)], len - sizeof(DATHEAD));
                    new_chunk->parent = current_chunk;
                    current_chunk->children.push_back(std::move(new_chunk));
                }
                break;
            case 0x09:
                current_chunk->tim++;
                {
                    std::unique_ptr<DatChunk> new_chunk = std::make_unique<DatChunk>(dathead->id, &buffer[offset + sizeof(DATHEAD)], len - sizeof(DATHEAD));
                    new_chunk->parent = current_chunk;
                    current_chunk->children.push_back(std::move(new_chunk));
                }
                break;
            case 0x0A:
                current_chunk->texInfo++;
                {
                    std::unique_ptr<DatChunk> new_chunk = std::make_unique<DatChunk>(dathead->id, &buffer[offset + sizeof(DATHEAD)], len - sizeof(DATHEAD));
                    new_chunk->parent = current_chunk;
                    current_chunk->children.push_back(std::move(new_chunk));
                }
                break;
            case 0x0B:
                current_chunk->vum++;
                {
                    std::unique_ptr<DatChunk> new_chunk = std::make_unique<DatChunk>(dathead->id, &buffer[offset + sizeof(DATHEAD)], len - sizeof(DATHEAD));
                    new_chunk->parent = current_chunk;
                    current_chunk->children.push_back(std::move(new_chunk));
                }
                break;
            case 0x0C:
                current_chunk->om1++;
                {
                    std::unique_ptr<DatChunk> new_chunk = std::make_unique<DatChunk>(dathead->id, &buffer[offset + sizeof(DATHEAD)], len - sizeof(DATHEAD));
                    new_chunk->parent = current_chunk;
                    current_chunk->children.push_back(std::move(new_chunk));
                }
                break;
            case 0x0D:
                current_chunk->fileInfo++;
                {
                    std::unique_ptr<DatChunk> new_chunk = std::make_unique<DatChunk>(dathead->id, &buffer[offset + sizeof(DATHEAD)], len - sizeof(DATHEAD));
                    new_chunk->parent = current_chunk;
                    current_chunk->children.push_back(std::move(new_chunk));
                }
                break;
            case 0x0E:
                current_chunk->anm++;
                {
                    std::unique_ptr<DatChunk> new_chunk = std::make_unique<DatChunk>(dathead->id, &buffer[offset + sizeof(DATHEAD)], len - sizeof(DATHEAD));
                    new_chunk->parent = current_chunk;
                    current_chunk->children.push_back(std::move(new_chunk));
                }
                break;
            case 0x0F:
                current_chunk->rsd++;
                {
                    std::unique_ptr<DatChunk> new_chunk = std::make_unique<DatChunk>(dathead->id, &buffer[offset + sizeof(DATHEAD)], len - sizeof(DATHEAD));
                    new_chunk->parent = current_chunk;
                    current_chunk->children.push_back(std::move(new_chunk));
                }
                break;
            case 0x10:
                current_chunk->unknown++;
                {
                    std::unique_ptr<DatChunk> new_chunk = std::make_unique<DatChunk>(dathead->id, &buffer[offset + sizeof(DATHEAD)], len - sizeof(DATHEAD));
                    new_chunk->parent = current_chunk;
                    current_chunk->children.push_back(std::move(new_chunk));
                }
                break;
            case 0x11:
                current_chunk->osm++;
                {
                    std::unique_ptr<DatChunk> new_chunk = std::make_unique<DatChunk>(dathead->id, &buffer[offset + sizeof(DATHEAD)], len - sizeof(DATHEAD));
                    new_chunk->parent = current_chunk;
                    current_chunk->children.push_back(std::move(new_chunk));
                }
                break;
            case 0x12:
                current_chunk->skd++;
                {
                    std::unique_ptr<DatChunk> new_chunk = std::make_unique<DatChunk>(dathead->id, &buffer[offset + sizeof(DATHEAD)], len - sizeof(DATHEAD));
                    new_chunk->parent = current_chunk;
                    current_chunk->children.push_back(std::move(new_chunk));
                }
                break;
            case 0x13:
                current_chunk->mtd++;
                {
                    std::unique_ptr<DatChunk> new_chunk = std::make_unique<DatChunk>(dathead->id, &buffer[offset + sizeof(DATHEAD)], len - sizeof(DATHEAD));
                    new_chunk->parent = current_chunk;
                    current_chunk->children.push_back(std::move(new_chunk));
                }
                break;
            case 0x14:
                current_chunk->mld++;
                {
                    std::unique_ptr<DatChunk> new_chunk = std::make_unique<DatChunk>(dathead->id, &buffer[offset + sizeof(DATHEAD)], len - sizeof(DATHEAD));
                    new_chunk->parent = current_chunk;
                    current_chunk->children.push_back(std::move(new_chunk));
                }
                break;
            case 0x15:
                current_chunk->mlt++;
                {
                    std::unique_ptr<DatChunk> new_chunk = std::make_unique<DatChunk>(dathead->id, &buffer[offset + sizeof(DATHEAD)], len - sizeof(DATHEAD));
                    new_chunk->parent = current_chunk;
                    current_chunk->children.push_back(std::move(new_chunk));
                }
                break;
            case 0x16:
                current_chunk->mws++;
                {
                    std::unique_ptr<DatChunk> new_chunk = std::make_unique<DatChunk>(dathead->id, &buffer[offset + sizeof(DATHEAD)], len - sizeof(DATHEAD));
                    new_chunk->parent = current_chunk;
                    current_chunk->children.push_back(std::move(new_chunk));
                }
                break;
            case 0x17:
                current_chunk->mod++;
                {
                    std::unique_ptr<DatChunk> new_chunk = std::make_unique<DatChunk>(dathead->id, &buffer[offset + sizeof(DATHEAD)], len - sizeof(DATHEAD));
                    new_chunk->parent = current_chunk;
                    current_chunk->children.push_back(std::move(new_chunk));
                }
                break;
            case 0x18:
                current_chunk->tim2++;
                {
                    std::unique_ptr<DatChunk> new_chunk = std::make_unique<DatChunk>(dathead->id, &buffer[offset + sizeof(DATHEAD)], len - sizeof(DATHEAD));
                    new_chunk->parent = current_chunk;
                    current_chunk->children.push_back(std::move(new_chunk));
                }
                break;
            case 0x19:
                current_chunk->keyframe++;
                {
                    std::unique_ptr<DatChunk> new_chunk = std::make_unique<Keyframe>(dathead->id, &buffer[offset + sizeof(DATHEAD)], len - sizeof(DATHEAD));
                    new_chunk->parent = current_chunk;
                    current_chunk->children.push_back(std::move(new_chunk));
                }
                break;
            case 0x1A:
                current_chunk->bmp++;
                {
                    std::unique_ptr<DatChunk> new_chunk = std::make_unique<DatChunk>(dathead->id, &buffer[offset + sizeof(DATHEAD)], len - sizeof(DATHEAD));
                    new_chunk->parent = current_chunk;
                    current_chunk->children.push_back(std::move(new_chunk));
                }
                break;
            case 0x1B:
                current_chunk->bmp2++;
                {
                    std::unique_ptr<DatChunk> new_chunk = std::make_unique<DatChunk>(dathead->id, &buffer[offset + sizeof(DATHEAD)], len - sizeof(DATHEAD));
                    new_chunk->parent = current_chunk;
                    current_chunk->children.push_back(std::move(new_chunk));
                }
                break;
            case 0x1C:
                current_chunk->mzb++;
                if (MZB::DecodeMZB(&buffer[offset + sizeof(DATHEAD)], len - sizeof(DATHEAD)))
                {
                    std::unique_ptr<DatChunk> new_chunk = std::make_unique<MZB>(dathead->id, &buffer[offset + sizeof(DATHEAD)], len - (sizeof(DATHEAD)));
                    new_chunk->parent = current_chunk;
                    current_chunk->children.push_back(std::move(new_chunk));
                }
                break;
            case 0x1D:
                current_chunk->mmd++;
                {
                    std::unique_ptr<DatChunk> new_chunk = std::make_unique<DatChunk>(dathead->id, &buffer[offset + sizeof(DATHEAD)], len - sizeof(DATHEAD));
                    new_chunk->parent = current_chunk;
                    current_chunk->children.push_back(std::move(new_chunk));
                }
                break;
            case 0x1E:
                current_chunk->mep++;
                {
                    std::unique_ptr<DatChunk> new_chunk = std::make_unique<DatChunk>(dathead->id, &buffer[offset + sizeof(DATHEAD)], len - sizeof(DATHEAD));
                    new_chunk->parent = current_chunk;
                    current_chunk->children.push_back(std::move(new_chunk));
                }
                break;
            case 0x1F:
                current_chunk->d3m++;
                {
                    std::unique_ptr<DatChunk> new_chunk = std::make_unique<D3M>(dathead->id, &buffer[offset + sizeof(DATHEAD)], len - sizeof(DATHEAD));
                    new_chunk->parent = current_chunk;
                    current_chunk->children.push_back(std::move(new_chunk));
                }
                break;
            case 0x20:
                current_chunk->d3s++;
                {
                    std::unique_ptr<DatChunk> new_chunk = std::make_unique<DXT3>(dathead->id, &buffer[offset + sizeof(DATHEAD)], len - sizeof(DATHEAD));
                    new_chunk->parent = current_chunk;
                    current_chunk->children.push_back(std::move(new_chunk));
                }
                break;
            case 0x21:
                current_chunk->d3a++;
                {
                    std::unique_ptr<DatChunk> new_chunk = std::make_unique<DatChunk>(dathead->id, &buffer[offset + sizeof(DATHEAD)], len - sizeof(DATHEAD));
                    new_chunk->parent = current_chunk;
                    current_chunk->children.push_back(std::move(new_chunk));
                }
                break;
            case 0x22:
                current_chunk->distProg++;
                {
                    std::unique_ptr<DatChunk> new_chunk = std::make_unique<DatChunk>(dathead->id, &buffer[offset + sizeof(DATHEAD)], len - sizeof(DATHEAD));
                    new_chunk->parent = current_chunk;
                    current_chunk->children.push_back(std::move(new_chunk));
                }
                break;
            case 0x23:
                current_chunk->vuLineProg++;
                {
                    std::unique_ptr<DatChunk> new_chunk = std::make_unique<DatChunk>(dathead->id, &buffer[offset + sizeof(DATHEAD)], len - sizeof(DATHEAD));
                    new_chunk->parent = current_chunk;
                    current_chunk->children.push_back(std::move(new_chunk));
                }
                break;
            case 0x24:
                current_chunk->ringProg++;
                {
                    std::unique_ptr<DatChunk> new_chunk = std::make_unique<DatChunk>(dathead->id, &buffer[offset + sizeof(DATHEAD)], len - sizeof(DATHEAD));
                    new_chunk->parent = current_chunk;
                    current_chunk->children.push_back(std::move(new_chunk));
                }
                break;
            case 0x25:
                current_chunk->d3b++;
                {
                    std::unique_ptr<DatChunk> new_chunk = std::make_unique<DatChunk>(dathead->id, &buffer[offset + sizeof(DATHEAD)], len - sizeof(DATHEAD));
                    new_chunk->parent = current_chunk;
                    current_chunk->children.push_back(std::move(new_chunk));
                }
                break;
            case 0x26:
                current_chunk->asn++;
                {
                    std::unique_ptr<DatChunk> new_chunk = std::make_unique<DatChunk>(dathead->id, &buffer[offset + sizeof(DATHEAD)], len - sizeof(DATHEAD));
                    new_chunk->parent = current_chunk;
                    current_chunk->children.push_back(std::move(new_chunk));
                }
                break;
            case 0x27:
                current_chunk->mot++;
                {
                    std::unique_ptr<DatChunk> new_chunk = std::make_unique<DatChunk>(dathead->id, &buffer[offset + sizeof(DATHEAD)], len - sizeof(DATHEAD));
                    new_chunk->parent = current_chunk;
                    current_chunk->children.push_back(std::move(new_chunk));
                }
                break;
            case 0x28:
                current_chunk->skl++;
                {
                    std::unique_ptr<DatChunk> new_chunk = std::make_unique<DatChunk>(dathead->id, &buffer[offset + sizeof(DATHEAD)], len - sizeof(DATHEAD));
                    new_chunk->parent = current_chunk;
                    current_chunk->children.push_back(std::move(new_chunk));
                }
                break;
            case 0x29:
                current_chunk->sk2++;
                {
                    std::unique_ptr<DatChunk> new_chunk = std::make_unique<SK2>(dathead->id, &buffer[offset + sizeof(DATHEAD)], len - sizeof(DATHEAD));
                    new_chunk->parent = current_chunk;
                    current_chunk->children.push_back(std::move(new_chunk));
                }
                break;
            case 0x2A:
                current_chunk->os2++;
                {
                    std::unique_ptr<DatChunk> new_chunk = std::make_unique<OS2>(dathead->id, &buffer[offset + sizeof(DATHEAD)], len - sizeof(DATHEAD));
                    new_chunk->parent = current_chunk;
                    current_chunk->children.push_back(std::move(new_chunk));
                }
                break;
            case 0x2B:
                current_chunk->mo2++;
                {
                    std::unique_ptr<DatChunk> new_chunk = std::make_unique<MO2>(dathead->id, &buffer[offset + sizeof(DATHEAD)], len - sizeof(DATHEAD));
                    new_chunk->parent = current_chunk;
                    current_chunk->children.push_back(std::move(new_chunk));
                }
                break;
            case 0x2C:
                current_chunk->psw++;
                {
                    std::unique_ptr<DatChunk> new_chunk = std::make_unique<DatChunk>(dathead->id, &buffer[offset + sizeof(DATHEAD)], len - sizeof(DATHEAD));
                    new_chunk->parent = current_chunk;
                    current_chunk->children.push_back(std::move(new_chunk));
                }
                break;
            case 0x2D:
                current_chunk->wsd++;
                {
                    std::unique_ptr<DatChunk> new_chunk = std::make_unique<DatChunk>(dathead->id, &buffer[offset + sizeof(DATHEAD)], len - sizeof(DATHEAD));
                    new_chunk->parent = current_chunk;
                    current_chunk->children.push_back(std::move(new_chunk));
                }
                break;
            case 0x2E:
                current_chunk->mmb++;
                if (MMB::DecodeMMB(&buffer[offset + sizeof(DATHEAD)], len - sizeof(DATHEAD)))
                {
                    std::unique_ptr<DatChunk> new_chunk = std::make_unique<MMB>(dathead->id, &buffer[offset + sizeof(DATHEAD)], len - sizeof(DATHEAD), rtx);
                    new_chunk->parent = current_chunk;
                    current_chunk->children.push_back(std::move(new_chunk));
                }
                break;
            case 0x2F:
                current_chunk->weather++;
                {
                    std::unique_ptr<DatChunk> new_chunk = std::make_unique<Weather>(dathead->id, &buffer[offset + sizeof(DATHEAD)], len - sizeof(DATHEAD));
                    new_chunk->parent = current_chunk;
                    current_chunk->children.push_back(std::move(new_chunk));
                }
                break;
            case 0x30:
                current_chunk->meb++;
                {
                    std::unique_ptr<DatChunk> new_chunk = std::make_unique<DatChunk>(dathead->id, &buffer[offset + sizeof(DATHEAD)], len - sizeof(DATHEAD));
                    new_chunk->parent = current_chunk;
                    current_chunk->children.push_back(std::move(new_chunk));
                }
                break;
            case 0x31:
                current_chunk->msb++;
                {
                    std::unique_ptr<DatChunk> new_chunk = std::make_unique<DatChunk>(dathead->id, &buffer[offset + sizeof(DATHEAD)], len - sizeof(DATHEAD));
                    new_chunk->parent = current_chunk;
                    current_chunk->children.push_back(std::move(new_chunk));
                }
                break;
            case 0x32:
                current_chunk->med++;
                {
                    std::unique_ptr<DatChunk> new_chunk = std::make_unique<DatChunk>(dathead->id, &buffer[offset + sizeof(DATHEAD)], len - sizeof(DATHEAD));
                    new_chunk->parent = current_chunk;
                    current_chunk->children.push_back(std::move(new_chunk));
                }
                break;
            case 0x33:
                current_chunk->msh++;
                {
                    std::unique_ptr<DatChunk> new_chunk = std::make_unique<DatChunk>(dathead->id, &buffer[offset + sizeof(DATHEAD)], len - sizeof(DATHEAD));
                    new_chunk->parent = current_chunk;
                    current_chunk->children.push_back(std::move(new_chunk));
                }
                break;
            case 0x34:
                current_chunk->ysh++;
                {
                    std::unique_ptr<DatChunk> new_chunk = std::make_unique<DatChunk>(dathead->id, &buffer[offset + sizeof(DATHEAD)], len - sizeof(DATHEAD));
                    new_chunk->parent = current_chunk;
                    current_chunk->children.push_back(std::move(new_chunk));
                }
                break;
            case 0x35:
                current_chunk->mbp++;
                {
                    std::unique_ptr<DatChunk> new_chunk = std::make_unique<DatChunk>(dathead->id, &buffer[offset + sizeof(DATHEAD)], len - sizeof(DATHEAD));
                    new_chunk->parent = current_chunk;
                    current_chunk->children.push_back(std::move(new_chunk));
                }
                break;
            case 0x36:
                current_chunk->rid++;
                {
                    std::unique_ptr<DatChunk> new_chunk = std::make_unique<DatChunk>(dathead->id, &buffer[offset + sizeof(DATHEAD)], len - sizeof(DATHEAD));
                    new_chunk->parent = current_chunk;
                    current_chunk->children.push_back(std::move(new_chunk));
                }
                break;
            case 0x37:
                current_chunk->wd++;
                {
                    std::unique_ptr<DatChunk> new_chunk = std::make_unique<DatChunk>(dathead->id, &buffer[offset + sizeof(DATHEAD)], len - sizeof(DATHEAD));
                    new_chunk->parent = current_chunk;
                    current_chunk->children.push_back(std::move(new_chunk));
                }
                break;
            case 0x38:
                current_chunk->bgm++;
                {
                    std::unique_ptr<DatChunk> new_chunk = std::make_unique<DatChunk>(dathead->id, &buffer[offset + sizeof(DATHEAD)], len - sizeof(DATHEAD));
                    new_chunk->parent = current_chunk;
                    current_chunk->children.push_back(std::move(new_chunk));
                }
                break;
            case 0x39:
                current_chunk->lfd++;
                {
                    std::unique_ptr<DatChunk> new_chunk = std::make_unique<DatChunk>(dathead->id, &buffer[offset + sizeof(DATHEAD)], len - sizeof(DATHEAD));
                    new_chunk->parent = current_chunk;
                    current_chunk->children.push_back(std::move(new_chunk));
                }
                break;
            case 0x3A:
                current_chunk->lfe++;
                {
                    std::unique_ptr<DatChunk> new_chunk = std::make_unique<DatChunk>(dathead->id, &buffer[offset + sizeof(DATHEAD)], len - sizeof(DATHEAD));
                    new_chunk->parent = current_chunk;
                    current_chunk->children.push_back(std::move(new_chunk));
                }
                break;
            case 0x3B:
                current_chunk->esh++;
                {
                    std::unique_ptr<DatChunk> new_chunk = std::make_unique<DatChunk>(dathead->id, &buffer[offset + sizeof(DATHEAD)], len - sizeof(DATHEAD));
                    new_chunk->parent = current_chunk;
                    current_chunk->children.push_back(std::move(new_chunk));
                }
                break;
            case 0x3C:
                current_chunk->sch++;
                {
                    std::unique_ptr<DatChunk> new_chunk = std::make_unique<DatChunk>(dathead->id, &buffer[offset + sizeof(DATHEAD)], len - sizeof(DATHEAD));
                    new_chunk->parent = current_chunk;
                    current_chunk->children.push_back(std::move(new_chunk));
                }
                break;
            case 0x3D:
                current_chunk->sep++;
                {
                    std::unique_ptr<DatChunk> new_chunk = std::make_unique<DatChunk>(dathead->id, &buffer[offset + sizeof(DATHEAD)], len - sizeof(DATHEAD));
                    new_chunk->parent = current_chunk;
                    current_chunk->children.push_back(std::move(new_chunk));
                }
                break;
            case 0x3E:
                current_chunk->vtx++;
                {
                    std::unique_ptr<DatChunk> new_chunk = std::make_unique<DatChunk>(dathead->id, &buffer[offset + sizeof(DATHEAD)], len - sizeof(DATHEAD));
                    new_chunk->parent = current_chunk;
                    current_chunk->children.push_back(std::move(new_chunk));
                }
                break;
            case 0x3F:
                current_chunk->lwo++;
                {
                    std::unique_ptr<DatChunk> new_chunk = std::make_unique<DatChunk>(dathead->id, &buffer[offset + sizeof(DATHEAD)], len - sizeof(DATHEAD));
                    new_chunk->parent = current_chunk;
                    current_chunk->children.push_back(std::move(new_chunk));
                }
                break;
            case 0x40:
                current_chunk->rme++;
                {
                    std::unique_ptr<DatChunk> new_chunk = std::make_unique<DatChunk>(dathead->id, &buffer[offset + sizeof(DATHEAD)], len - sizeof(DATHEAD));
                    new_chunk->parent = current_chunk;
                    current_chunk->children.push_back(std::move(new_chunk));
                }
                break;
            case 0x41:
                current_chunk->elt++;
                {
                    std::unique_ptr<DatChunk> new_chunk = std::make_unique<DatChunk>(dathead->id, &buffer[offset + sizeof(DATHEAD)], len - sizeof(DATHEAD));
                    new_chunk->parent = current_chunk;
                    current_chunk->children.push_back(std::move(new_chunk));
                }
                break;
            case 0x42:
                current_chunk->rab++;
                {
                    std::unique_ptr<DatChunk> new_chunk = std::make_unique<DatChunk>(dathead->id, &buffer[offset + sizeof(DATHEAD)], len - sizeof(DATHEAD));
                    new_chunk->parent = current_chunk;
                    current_chunk->children.push_back(std::move(new_chunk));
                }
                break;
            case 0x43:
                current_chunk->mtt++;
                {
                    std::unique_ptr<DatChunk> new_chunk = std::make_unique<DatChunk>(dathead->id, &buffer[offset + sizeof(DATHEAD)], len - sizeof(DATHEAD));
                    new_chunk->parent = current_chunk;
                    current_chunk->children.push_back(std::move(new_chunk));
                }
                break;
            case 0x44:
                current_chunk->mtb++;
                {
                    std::unique_ptr<DatChunk> new_chunk = std::make_unique<DatChunk>(dathead->id, &buffer[offset + sizeof(DATHEAD)], len - sizeof(DATHEAD));
                    new_chunk->parent = current_chunk;
                    current_chunk->children.push_back(std::move(new_chunk));
                }
                break;
            case 0x45:
                current_chunk->cib++;
                {
                    std::unique_ptr<DatChunk> new_chunk = std::make_unique<DatChunk>(dathead->id, &buffer[offset + sizeof(DATHEAD)], len - sizeof(DATHEAD));
                    new_chunk->parent = current_chunk;
                    current_chunk->children.push_back(std::move(new_chunk));
                }
                break;
            case 0x46:
                current_chunk->tlt++;
                {
                    std::unique_ptr<DatChunk> new_chunk = std::make_unique<DatChunk>(dathead->id, &buffer[offset + sizeof(DATHEAD)], len - sizeof(DATHEAD));
                    new_chunk->parent = current_chunk;
                    current_chunk->children.push_back(std::move(new_chunk));
                }
                break;
            case 0x47:
                current_chunk->pointLightProg++;
                {
                    std::unique_ptr<DatChunk> new_chunk = std::make_unique<DatChunk>(dathead->id, &buffer[offset + sizeof(DATHEAD)], len - sizeof(DATHEAD));
                    new_chunk->parent = current_chunk;
                    current_chunk->children.push_back(std::move(new_chunk));
                }
                break;
            case 0x48:
                current_chunk->mgd++;
                {
                    std::unique_ptr<DatChunk> new_chunk = std::make_unique<DatChunk>(dathead->id, &buffer[offset + sizeof(DATHEAD)], len - sizeof(DATHEAD));
                    new_chunk->parent = current_chunk;
                    current_chunk->children.push_back(std::move(new_chunk));
                }
                break;
            case 0x49:
                current_chunk->mgb++;
                {
                    std::unique_ptr<DatChunk> new_chunk = std::make_unique<DatChunk>(dathead->id, &buffer[offset + sizeof(DATHEAD)], len - sizeof(DATHEAD));
                    new_chunk->parent = current_chunk;
                    current_chunk->children.push_back(std::move(new_chunk));
                }
                break;
            case 0x4A:
                current_chunk->sph++;
                {
                    std::unique_ptr<DatChunk> new_chunk = std::make_unique<DatChunk>(dathead->id, &buffer[offset + sizeof(DATHEAD)], len - sizeof(DATHEAD));
                    new_chunk->parent = current_chunk;
                    current_chunk->children.push_back(std::move(new_chunk));
                }
                break;
            case 0x4B:
                current_chunk->bmd++;
                {
                    std::unique_ptr<DatChunk> new_chunk = std::make_unique<DatChunk>(dathead->id, &buffer[offset + sizeof(DATHEAD)], len - sizeof(DATHEAD));
                    new_chunk->parent = current_chunk;
                    current_chunk->children.push_back(std::move(new_chunk));
                }
                break;
            case 0x4C:
                current_chunk->qif++;
                {
                    std::unique_ptr<DatChunk> new_chunk = std::make_unique<DatChunk>(dathead->id, &buffer[offset + sizeof(DATHEAD)], len - sizeof(DATHEAD));
                    new_chunk->parent = current_chunk;
                    current_chunk->children.push_back(std::move(new_chunk));
                }
                break;
            case 0x4D:
                current_chunk->qdt++;
                {
                    std::unique_ptr<DatChunk> new_chunk = std::make_unique<DatChunk>(dathead->id, &buffer[offset + sizeof(DATHEAD)], len - sizeof(DATHEAD));
                    new_chunk->parent = current_chunk;
                    current_chunk->children.push_back(std::move(new_chunk));
                }
                break;
            case 0x4E:
                current_chunk->mif++;
                {
                    std::unique_ptr<DatChunk> new_chunk = std::make_unique<DatChunk>(dathead->id, &buffer[offset + sizeof(DATHEAD)], len - sizeof(DATHEAD));
                    new_chunk->parent = current_chunk;
                    current_chunk->children.push_back(std::move(new_chunk));
                }
                break;
            case 0x4F:
                current_chunk->mdt++;
                {
                    std::unique_ptr<DatChunk> new_chunk = std::make_unique<DatChunk>(dathead->id, &buffer[offset + sizeof(DATHEAD)], len - sizeof(DATHEAD));
                    new_chunk->parent = current_chunk;
                    current_chunk->children.push_back(std::move(new_chunk));
                }
                break;
            case 0x50:
                current_chunk->sif++;
                {
                    std::unique_ptr<DatChunk> new_chunk = std::make_unique<DatChunk>(dathead->id, &buffer[offset + sizeof(DATHEAD)], len - sizeof(DATHEAD));
                    new_chunk->parent = current_chunk;
                    current_chunk->children.push_back(std::move(new_chunk));
                }
                break;
            case 0x51:
                current_chunk->sdt++;
                {
                    std::unique_ptr<DatChunk> new_chunk = std::make_unique<DatChunk>(dathead->id, &buffer[offset + sizeof(DATHEAD)], len - sizeof(DATHEAD));
                    new_chunk->parent = current_chunk;
                    current_chunk->children.push_back(std::move(new_chunk));
                }
                break;
            case 0x52:
                current_chunk->acd++;
                {
                    std::unique_ptr<DatChunk> new_chunk = std::make_unique<DatChunk>(dathead->id, &buffer[offset + sizeof(DATHEAD)], len - sizeof(DATHEAD));
                    new_chunk->parent = current_chunk;
                    current_chunk->children.push_back(std::move(new_chunk));
                }
                break;
            case 0x53:
                current_chunk->acb++;
                {
                    std::unique_ptr<DatChunk> new_chunk = std::make_unique<DatChunk>(dathead->id, &buffer[offset + sizeof(DATHEAD)], len - sizeof(DATHEAD));
                    new_chunk->parent = current_chunk;
                    current_chunk->children.push_back(std::move(new_chunk));
                }
                break;
            case 0x54:
                current_chunk->afb++;
                {
                    std::unique_ptr<DatChunk> new_chunk = std::make_unique<DatChunk>(dathead->id, &buffer[offset + sizeof(DATHEAD)], len - sizeof(DATHEAD));
                    new_chunk->parent = current_chunk;
                    current_chunk->children.push_back(std::move(new_chunk));
                }
                break;
            case 0x55:
                current_chunk->aft++;
                {
                    std::unique_ptr<DatChunk> new_chunk = std::make_unique<DatChunk>(dathead->id, &buffer[offset + sizeof(DATHEAD)], len - sizeof(DATHEAD));
                    new_chunk->parent = current_chunk;
                    current_chunk->children.push_back(std::move(new_chunk));
                }
                break;
            case 0x56:
                current_chunk->wwd++;
                {
                    std::unique_ptr<DatChunk> new_chunk = std::make_unique<DatChunk>(dathead->id, &buffer[offset + sizeof(DATHEAD)], len - sizeof(DATHEAD));
                    new_chunk->parent = current_chunk;
                    current_chunk->children.push_back(std::move(new_chunk));
                }
                break;
            case 0x57:
                current_chunk->nullProg++;
                {
                    std::unique_ptr<DatChunk> new_chunk = std::make_unique<DatChunk>(dathead->id, &buffer[offset + sizeof(DATHEAD)], len - sizeof(DATHEAD));
                    new_chunk->parent = current_chunk;
                    current_chunk->children.push_back(std::move(new_chunk));
                }
                break;
            case 0x58:
                current_chunk->spw++;
                {
                    std::unique_ptr<DatChunk> new_chunk = std::make_unique<DatChunk>(dathead->id, &buffer[offset + sizeof(DATHEAD)], len - sizeof(DATHEAD));
                    new_chunk->parent = current_chunk;
                    current_chunk->children.push_back(std::move(new_chunk));
                }
                break;
            case 0x59:
                current_chunk->fud++;
                {
                    std::unique_ptr<DatChunk> new_chunk = std::make_unique<DatChunk>(dathead->id, &buffer[offset + sizeof(DATHEAD)], len - sizeof(DATHEAD));
                    new_chunk->parent = current_chunk;
                    current_chunk->children.push_back(std::move(new_chunk));
                }
                break;
            case 0x5A:
                current_chunk->disgregaterProg++;
                {
                    std::unique_ptr<DatChunk> new_chunk = std::make_unique<DatChunk>(dathead->id, &buffer[offset + sizeof(DATHEAD)], len - sizeof(DATHEAD));
                    new_chunk->parent = current_chunk;
                    current_chunk->children.push_back(std::move(new_chunk));
                }
                break;
            case 0x5B:
                current_chunk->smt++;
                {
                    std::unique_ptr<DatChunk> new_chunk = std::make_unique<DatChunk>(dathead->id, &buffer[offset + sizeof(DATHEAD)], len - sizeof(DATHEAD));
                    new_chunk->parent = current_chunk;
                    current_chunk->children.push_back(std::move(new_chunk));
                }
                break;
            case 0x5C:
                current_chunk->damValueProg++;
                {
                    std::unique_ptr<DatChunk> new_chunk = std::make_unique<DatChunk>(dathead->id, &buffer[offset + sizeof(DATHEAD)], len - sizeof(DATHEAD));
                    new_chunk->parent = current_chunk;
                    current_chunk->children.push_back(std::move(new_chunk));
                }
                break;
            case 0x5D:
                current_chunk->bp++;
                {
                    std::unique_ptr<DatChunk> new_chunk = std::make_unique<DatChunk>(dathead->id, &buffer[offset + sizeof(DATHEAD)], len - sizeof(DATHEAD));
                    new_chunk->parent = current_chunk;
                    current_chunk->children.push_back(std::move(new_chunk));
                }
                break;
            default:
                break;
            }
            offset += len;
        }
    }
}
