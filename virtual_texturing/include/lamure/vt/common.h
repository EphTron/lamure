//
// Created by sebastian on 13.11.17.
//

#ifndef VT_COMMON_H
#define VT_COMMON_H

#include <cstdint>

namespace vt {

    typedef uint64_t id_type;
    typedef uint32_t priority_type;

    class Config {
    public:
        // Sections
        static constexpr const char *TEXTURE_MANAGEMENT = "TEXTURE_MANAGEMENT";
        static constexpr const char *DEBUG = "DEBUG";

        // Texture management fields
        static constexpr const char *TILE_SIZE = "TILE_SIZE";
        static constexpr const char *FILE_PPM = "FILE_PPM";
        static constexpr const char *FILE_MIPMAP = "FILE_MIPMAP";
        static constexpr const char *RUN_IN_PARALLEL = "RUN_IN_PARALLEL";

        // Debug fields
        static constexpr const char *KEEP_INTERMEDIATE_DATA = "KEEP_INTERMEDIATE_DATA";

        static constexpr const char *DEFAULT = "DEFAULT";
    };

}

#endif //VT_COMMON_H