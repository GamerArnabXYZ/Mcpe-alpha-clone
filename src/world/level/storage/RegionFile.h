#ifndef NET_MINECRAFT_WORLD_LEVEL_STORAGE__RegionFile_H__
#define NET_MINECRAFT_WORLD_LEVEL_STORAGE__RegionFile_H__

// Infinite-terrain region storage.
// The world is split into 32×32-chunk region files named r.RX.RZ.dat
// stored under <worldPath>/region/. Each region file uses a 1024-entry
// offset table (same binary layout as before) so old saves are compatible
// when the player hasn't moved outside region (0,0).

#include <map>
#include <string>
#include <unordered_map>
#include <cstdint>
#include "../../../raknet/BitStream.h"

// One open region file handle
struct RegionShard {
    FILE*   file      = nullptr;
    int*    offsets   = nullptr;   // 1024 ints
    int*    emptyBuf  = nullptr;   // zeroed sector buffer
    std::map<int,bool> sectorFree;

    RegionShard()  = default;
    ~RegionShard() { close(); delete[] offsets; delete[] emptyBuf; }
    RegionShard(const RegionShard&)            = delete;
    RegionShard& operator=(const RegionShard&) = delete;

    void close() { if (file) { fclose(file); file = nullptr; } }
};

// Key: packed (regionX << 32 | (uint32_t)regionZ)
static inline int64_t regionKey(int rx, int rz) {
    return ((int64_t)(uint32_t)rx) | (((int64_t)(uint32_t)rz) << 32);
}

class RegionFile
{
public:
    explicit RegionFile(const std::string& basePath);
    virtual ~RegionFile();

    bool open();   // kept for API compat — always returns true
    bool readChunk(int x, int z, RakNet::BitStream** destChunkData);
    bool writeChunk(int x, int z, RakNet::BitStream& chunkData);

private:
    // Convert world chunk coords to region coords + local coords
    static void toRegion(int cx, int cz, int& rx, int& rz, int& lx, int& lz) {
        // Arithmetic right-shift (floor div by 32 for negative coords too)
        rx = cx < 0 ? ((cx - 31) / 32) : (cx / 32);
        rz = cz < 0 ? ((cz - 31) / 32) : (cz / 32);
        lx = cx - rx * 32;  // 0..31
        lz = cz - rz * 32;  // 0..31
    }

    RegionShard* getShard(int rx, int rz);
    bool         write(RegionShard* s, int sector, RakNet::BitStream& data);

    std::string basePath;
    std::unordered_map<int64_t, RegionShard*> shards;
};

#endif /*NET_MINECRAFT_WORLD_LEVEL_STORAGE__RegionFile_H__*/
