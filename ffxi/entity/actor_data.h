#pragma once
#include <array>
#include <map>

namespace ActorData
{
    //TODO: add emote location, etc 
    struct PCDatIDs
    {
        size_t skel;
        size_t motion;
        size_t motion_dw_l;
        size_t motion_dw_r;
    };

    static std::array PCSkeletonIDs {
        PCDatIDs{ .skel = 7072, .motion = 9672, .motion_dw_l = 40431, .motion_dw_r = 40815 },
        PCDatIDs{ .skel = 10248, .motion = 12848, .motion_dw_l = 41711, .motion_dw_r = 42095 },
        PCDatIDs{ .skel = 13424, .motion = 16024, .motion_dw_l = 42991, .motion_dw_r = 43375 },
        PCDatIDs{ .skel = 16600, .motion = 19200, .motion_dw_l = 44271, .motion_dw_r = 44655 },
        PCDatIDs{ .skel = 19776, .motion = 22376, .motion_dw_l = 45551, .motion_dw_r = 45935 },
        PCDatIDs{ .skel = 19776, .motion = 22376, .motion_dw_l = 45551, .motion_dw_r = 45935 },
        PCDatIDs{ .skel = 23176, .motion = 25776, .motion_dw_l = 46831, .motion_dw_r = 47215 },
        PCDatIDs{ .skel = 26352, .motion = 28952, .motion_dw_l = 48111, .motion_dw_r = 48495 }
    };

    static std::array PCModelIDs {
        std::array {
            std::map<uint16_t, size_t> { { 0, 7080 }, { 32, 0 } },
            std::map<uint16_t, size_t> { { 0, 7112 }, { 256, 63323 }, { 320, 71247 }, { 576, 98787 }, { 608, 102961 }, { 672, 0 } },
            std::map<uint16_t, size_t> { { 0, 7368 }, { 256, 63387 }, { 320, 71503 }, { 576, 98819 }, { 608, 103025 }, { 672, 0 } },
            std::map<uint16_t, size_t> { { 0, 7624 }, { 256, 63451 }, { 320, 71759 }, { 576, 98851 }, { 608, 103089 }, { 672, 0 } },
            std::map<uint16_t, size_t> { { 0, 7880 }, { 256, 63515 }, { 320, 72015 }, { 576, 98883 }, { 608, 103153 }, { 672, 0 } },
            std::map<uint16_t, size_t> { { 0, 8136 }, { 256, 63579 }, { 320, 72271 }, { 576, 98915 }, { 608, 103217 }, { 672, 0 } },
            std::map<uint16_t, size_t> { { 0, 8392 }, { 512, 63643 }, { 640, 72527 }, { 896, 107301 }, { 928, 0 } },
            std::map<uint16_t, size_t> { { 0, 41199 }, { 512, 66459 }, { 640, 81999 }, { 896, 105201 }, { 928, 0 } },
            std::map<uint16_t, size_t> { { 0, 9416 }, { 256, 0 } }
        },
        std::array {
            std::map<uint16_t, size_t> { { 0, 10256 }, { 32, 0 } },
            std::map<uint16_t, size_t> { { 0, 10288 }, { 256, 63771 }, { 320, 72783 }, { 576, 98947 }, { 608, 103281 }, { 672, 0 } },
            std::map<uint16_t, size_t> { { 0, 10544 }, { 256, 63835 }, { 320, 73039 }, { 576, 98979 }, { 608, 103345 }, { 672, 0 } },
            std::map<uint16_t, size_t> { { 0, 10800 }, { 256, 63899 }, { 320, 73295 }, { 576, 99011 }, { 608, 103409 }, { 672, 0 } },
            std::map<uint16_t, size_t> { { 0, 11056 }, { 256, 63963 }, { 320, 73551 }, { 576, 99043 }, { 608, 103473 }, { 672, 0 } },
            std::map<uint16_t, size_t> { { 0, 11312 }, { 256, 64027 }, { 320, 73807 }, { 576, 99075 }, { 608, 103537 }, { 672, 0 } },
            std::map<uint16_t, size_t> { { 0, 11568 }, { 512, 64091 }, { 640, 74063 }, { 896, 107601 }, { 928, 0 } },
            std::map<uint16_t, size_t> { { 0, 42479 }, { 512, 66587 }, { 640, 82255 }, { 896, 105501 }, { 928, 0 } },
            std::map<uint16_t, size_t> { { 0, 12592 }, { 256, 0 } }
        },
        std::array {
            std::map<uint16_t, size_t> { { 0, 13432 }, { 32, 0 } },
            std::map<uint16_t, size_t> { { 0, 13464 }, { 256, 64219 }, { 320, 74319 }, { 576, 99107 }, { 608, 103601 }, { 672, 0 } },
            std::map<uint16_t, size_t> { { 0, 13720 }, { 256, 64283 }, { 320, 74575 }, { 576, 99139 }, { 608, 103665 }, { 672, 0 } },
            std::map<uint16_t, size_t> { { 0, 13976 }, { 256, 64347 }, { 320, 74831 }, { 576, 99171 }, { 608, 103729 }, { 672, 0 } },
            std::map<uint16_t, size_t> { { 0, 14232 }, { 256, 64411 }, { 320, 75087 }, { 576, 99203 }, { 608, 103793 }, { 672, 0 } },
            std::map<uint16_t, size_t> { { 0, 14488 }, { 256, 64475 }, { 320, 75343 }, { 576, 99235 }, { 608, 103857 }, { 672, 0 } },
            std::map<uint16_t, size_t> { { 0, 14744 }, { 512, 64539 }, { 640, 75599 }, { 896, 107901 }, { 928, 0 } },
            std::map<uint16_t, size_t> { { 0, 43759 }, { 512, 66715 }, { 640, 82511 }, { 896, 105801 }, { 928, 0 } },
            std::map<uint16_t, size_t> { { 0, 15768 }, { 256, 0 } }
        },
        std::array {
            std::map<uint16_t, size_t> { { 0, 16608 }, { 32, 0 } },
            std::map<uint16_t, size_t> { { 0, 16640 }, { 256, 64667 }, { 320, 75855 }, { 576, 99267 }, { 608, 103921 }, { 672, 0 } },
            std::map<uint16_t, size_t> { { 0, 16896 }, { 256, 64731 }, { 320, 76111 }, { 576, 99299 }, { 608, 103985 }, { 672, 0 } },
            std::map<uint16_t, size_t> { { 0, 17152 }, { 256, 64795 }, { 320, 76367 }, { 576, 99331 }, { 608, 104049 }, { 672, 0 } },
            std::map<uint16_t, size_t> { { 0, 17408 }, { 256, 64859 }, { 320, 76623 }, { 576, 99363 }, { 608, 104113 }, { 672, 0 } },
            std::map<uint16_t, size_t> { { 0, 17664 }, { 256, 64923 }, { 320, 76879 }, { 576, 99395 }, { 608, 104177 }, { 672, 0 } },
            std::map<uint16_t, size_t> { { 0, 17920 }, { 512, 64987 }, { 640, 77135 }, { 896, 108201 }, { 928, 0 } },
            std::map<uint16_t, size_t> { { 0, 45039 }, { 512, 66843 }, { 640, 82767 }, { 896, 106101 }, { 928, 0 } },
            std::map<uint16_t, size_t> { { 0, 18944 }, { 256, 0 } }
        },
        std::array {
            std::map<uint16_t, size_t> { { 0, 19784 }, { 32, 0 } },
            std::map<uint16_t, size_t> { { 0, 19816 }, { 256, 65115 }, { 320, 77391 }, { 576, 99427 }, { 608, 104241 }, { 672, 0 } },
            std::map<uint16_t, size_t> { { 0, 20072 }, { 256, 65179 }, { 320, 77647 }, { 576, 99459 }, { 608, 104305 }, { 672, 0 } },
            std::map<uint16_t, size_t> { { 0, 20328 }, { 256, 65243 }, { 320, 77903 }, { 576, 99491 }, { 608, 104369 }, { 672, 0 } },
            std::map<uint16_t, size_t> { { 0, 20584 }, { 256, 65307 }, { 320, 78159 }, { 576, 99523 }, { 608, 104433 }, { 672, 0 } },
            std::map<uint16_t, size_t> { { 0, 20840 }, { 256, 65371 }, { 320, 78415 }, { 576, 99555 }, { 608, 104497 }, { 672, 0 } },
            std::map<uint16_t, size_t> { { 0, 21096 }, { 512, 65435 }, { 640, 78671 }, { 896, 108501 }, { 928, 0 } },
            std::map<uint16_t, size_t> { { 0, 46319 }, { 512, 66971 }, { 640, 83023 }, { 896, 106401 }, { 928, 0 } },
            std::map<uint16_t, size_t> { { 0, 22120 }, { 256, 0 } }
        },
        std::array {
            std::map<uint16_t, size_t> { { 0, 22960 }, { 32, 0 } },
            std::map<uint16_t, size_t> { { 0, 19816 }, { 256, 65115 }, { 320, 77391 }, { 576, 99427 }, { 608, 104241 }, { 672, 0 } },
            std::map<uint16_t, size_t> { { 0, 20072 }, { 256, 65179 }, { 320, 77647 }, { 576, 99459 }, { 608, 104305 }, { 672, 0 } },
            std::map<uint16_t, size_t> { { 0, 20328 }, { 256, 65243 }, { 320, 77903 }, { 576, 99491 }, { 608, 104369 }, { 672, 0 } },
            std::map<uint16_t, size_t> { { 0, 20584 }, { 256, 65307 }, { 320, 78159 }, { 576, 99523 }, { 608, 104433 }, { 672, 0 } },
            std::map<uint16_t, size_t> { { 0, 20840 }, { 256, 65371 }, { 320, 78415 }, { 576, 99555 }, { 608, 104497 }, { 672, 0 } },
            std::map<uint16_t, size_t> { { 0, 21096 }, { 512, 65435 }, { 640, 78671 }, { 896, 108501 }, { 928, 0 } },
            std::map<uint16_t, size_t> { { 0, 46319 }, { 512, 66971 }, { 640, 83023 }, { 896, 106401 }, { 928, 0 } },
            std::map<uint16_t, size_t> { { 0, 22120 }, { 256, 0 } }
        },
        std::array {
            std::map<uint16_t, size_t> { { 0, 23184 }, { 32, 0 } },
            std::map<uint16_t, size_t> { { 0, 23216 }, { 256, 65563 }, { 320, 78927 }, { 576, 99587 }, { 608, 104561 }, { 672, 0 } },
            std::map<uint16_t, size_t> { { 0, 23472 }, { 256, 65627 }, { 320, 79183 }, { 576, 99619 }, { 608, 104625 }, { 672, 0 } },
            std::map<uint16_t, size_t> { { 0, 23728 }, { 256, 65691 }, { 320, 79439 }, { 576, 99651 }, { 608, 104689 }, { 672, 0 } },
            std::map<uint16_t, size_t> { { 0, 23984 }, { 256, 65755 }, { 320, 79695 }, { 576, 99683 }, { 608, 104753 }, { 672, 0 } },
            std::map<uint16_t, size_t> { { 0, 24240 }, { 256, 65819 }, { 320, 79951 }, { 576, 99715 }, { 608, 104817 }, { 672, 0 } },
            std::map<uint16_t, size_t> { { 0, 24496 }, { 512, 65883 }, { 640, 80207 }, { 896, 108801 }, { 928, 0 } },
            std::map<uint16_t, size_t> { { 0, 47599 }, { 512, 67099 }, { 640, 83279 }, { 896, 106701 }, { 928, 0 } },
            std::map<uint16_t, size_t> { { 0, 25520 }, { 256, 0 } }
        },
        std::array {
            std::map<uint16_t, size_t> { { 0, 26360 }, { 32, 0 } },
            std::map<uint16_t, size_t> { { 0, 26392 }, { 256, 66011 }, { 320, 80463 }, { 576, 99747 }, { 608, 104881 }, { 672, 0 } },
            std::map<uint16_t, size_t> { { 0, 26648 }, { 256, 66075 }, { 320, 80719 }, { 576, 99779 }, { 608, 104945 }, { 672, 0 } },
            std::map<uint16_t, size_t> { { 0, 26904 }, { 256, 66139 }, { 320, 80975 }, { 576, 99811 }, { 608, 105009 }, { 672, 0 } },
            std::map<uint16_t, size_t> { { 0, 27160 }, { 256, 66203 }, { 320, 81231 }, { 576, 99843 }, { 608, 105073 }, { 672, 0 } },
            std::map<uint16_t, size_t> { { 0, 27416 }, { 256, 66267 }, { 320, 81487 }, { 576, 99875 }, { 608, 105137 }, { 672, 0 } },
            std::map<uint16_t, size_t> { { 0, 27672 }, { 512, 66331 }, { 640, 81743 }, { 896, 109101 }, { 928, 0 } },
            std::map<uint16_t, size_t> { { 0, 48879 }, { 512, 67227 }, { 640, 83535 }, { 896, 107001 }, { 928, 0 } },
            std::map<uint16_t, size_t> { { 0, 28696 }, { 256, 0 } }
        }
    };
}