#include "RegionFile.h"
#include "../../../platform/log.h"
#include <cstring>
#include <cassert>
#include <cstdio>

#ifdef _WIN32
#  include <direct.h>
#  define mkdir_p(p) _mkdir(p)
#else
#  include <sys/stat.h>
#  include <sys/types.h>
#  define mkdir_p(p) mkdir(p, 0755)
#endif

static const int SECTOR_BYTES = 4096;
static const int SECTOR_INTS  = SECTOR_BYTES / 4;  // 1024
static const int REGION_SIDE  = 32;  // chunks per region axis

static void logAssert(int actual, int expected) {
    if (actual != expected)
        LOGI("ERROR: I/O operation failed (%d vs %d)\n", actual, expected);
}

// ---------------------------------------------------------------------------
RegionFile::RegionFile(const std::string& path)
    : basePath(path)
{}

RegionFile::~RegionFile() {
    for (auto& kv : shards) delete kv.second;
    shards.clear();
}

bool RegionFile::open() {
    // Ensure region/ subdir exists
    std::string dir = basePath + "/region";
    mkdir_p(dir.c_str());
    return true;
}

// ---------------------------------------------------------------------------
RegionShard* RegionFile::getShard(int rx, int rz) {
    int64_t key = regionKey(rx, rz);
    auto it = shards.find(key);
    if (it != shards.end()) return it->second;

    // Build filename:  <world>/region/r.RX.RZ.dat
    char name[256];
    snprintf(name, sizeof(name), "%s/region/r.%d.%d.dat",
             basePath.c_str(), rx, rz);

    RegionShard* s = new RegionShard();
    s->offsets   = new int[SECTOR_INTS];
    s->emptyBuf  = new int[SECTOR_INTS];
    memset(s->offsets,  0, SECTOR_INTS * sizeof(int));
    memset(s->emptyBuf, 0, SECTOR_INTS * sizeof(int));

    s->file = fopen(name, "r+b");
    if (s->file) {
        // Read existing offset table
        logAssert(fread(s->offsets, sizeof(int), SECTOR_INTS, s->file), SECTOR_INTS);
        s->sectorFree[0] = false;
        for (int i = 0; i < SECTOR_INTS; i++) {
            int off = s->offsets[i];
            if (off) {
                int base  = off >> 8;
                int count = off & 0xff;
                for (int j = 0; j < count; j++)
                    s->sectorFree[base + j] = false;
            }
        }
    } else {
        // New region file
        s->file = fopen(name, "w+b");
        if (!s->file) {
            LOGI("RegionFile: failed to create %s\n", name);
            delete s;
            return nullptr;
        }
        // Write blank offset table
        logAssert(fwrite(s->offsets, sizeof(int), SECTOR_INTS, s->file), SECTOR_INTS);
        s->sectorFree[0] = false;
    }

    shards[key] = s;
    return s;
}

// ---------------------------------------------------------------------------
bool RegionFile::readChunk(int x, int z, RakNet::BitStream** destChunkData) {
    int rx, rz, lx, lz;
    toRegion(x, z, rx, rz, lx, lz);
    int localIdx = lx + lz * REGION_SIDE;

    RegionShard* s = getShard(rx, rz);
    if (!s) return false;

    int offset = s->offsets[localIdx];
    if (offset == 0) return false;

    int sectorNum = offset >> 8;
    fseek(s->file, sectorNum * SECTOR_BYTES, SEEK_SET);

    int length = 0;
    fread(&length, sizeof(int), 1, s->file);
    length -= sizeof(int);
    if (length <= 0) return false;

    unsigned char* data = new unsigned char[length];
    logAssert(fread(data, 1, length, s->file), length);
    *destChunkData = new RakNet::BitStream(data, length, false);
    return true;
}

// ---------------------------------------------------------------------------
bool RegionFile::writeChunk(int x, int z, RakNet::BitStream& chunkData) {
    int rx, rz, lx, lz;
    toRegion(x, z, rx, rz, lx, lz);
    int localIdx = lx + lz * REGION_SIDE;

    RegionShard* s = getShard(rx, rz);
    if (!s) return false;

    int size         = chunkData.GetNumberOfBytesUsed() + sizeof(int);
    int offset       = s->offsets[localIdx];
    int sectorNum    = offset >> 8;
    int sectorCount  = offset & 0xff;
    int sectorsNeeded = (size / SECTOR_BYTES) + 1;

    if (sectorsNeeded > 256) {
        LOGI("ERROR: Chunk too big to save\n");
        return false;
    }

    if (sectorNum != 0 && sectorCount == sectorsNeeded) {
        write(s, sectorNum, chunkData);
    } else {
        // Free old sectors
        for (int i = 0; i < sectorCount; i++)
            s->sectorFree[sectorNum + i] = true;

        // Find free run
        int slot = 0, runLength = 0;
        bool extendFile = false;
        while (runLength < sectorsNeeded) {
            if (s->sectorFree.find(slot + runLength) == s->sectorFree.end()) {
                extendFile = true; break;
            }
            if (s->sectorFree[slot + runLength]) {
                runLength++;
            } else {
                slot = slot + runLength + 1;
                runLength = 0;
            }
        }

        if (extendFile) {
            fseek(s->file, 0, SEEK_END);
            for (int i = 0; i < (sectorsNeeded - runLength); i++) {
                fwrite(s->emptyBuf, sizeof(int), SECTOR_INTS, s->file);
                s->sectorFree[slot + i] = true;
            }
        }

        s->offsets[localIdx] = (slot << 8) | sectorsNeeded;
        for (int i = 0; i < sectorsNeeded; i++)
            s->sectorFree[slot + i] = false;

        write(s, slot, chunkData);

        // Persist the offset entry
        fseek(s->file, localIdx * sizeof(int), SEEK_SET);
        fwrite(&s->offsets[localIdx], sizeof(int), 1, s->file);
    }

    return true;
}

// ---------------------------------------------------------------------------
bool RegionFile::write(RegionShard* s, int sector, RakNet::BitStream& chunkData) {
    fseek(s->file, sector * SECTOR_BYTES, SEEK_SET);
    int size = chunkData.GetNumberOfBytesUsed() + sizeof(int);
    logAssert(fwrite(&size, sizeof(int), 1, s->file), 1);
    logAssert(fwrite(chunkData.GetData(), 1,
              chunkData.GetNumberOfBytesUsed(), s->file),
              (int)chunkData.GetNumberOfBytesUsed());
    return true;
}
