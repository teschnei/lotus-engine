#pragma once

#include <vector>
#include <memory>
#include "engine/types.h"

namespace FFXI
{
    class DatChunk
    {
    public:
        DatChunk(char* _name, uint8_t* _buffer, size_t _len) : name(_name), buffer(_buffer), len(_len) {}
        virtual ~DatChunk() = default;

        char* name;
        uint8_t* buffer;
        size_t len;

        std::vector<std::unique_ptr<DatChunk>> children;
        DatChunk* parent{ nullptr };

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
    };
}
