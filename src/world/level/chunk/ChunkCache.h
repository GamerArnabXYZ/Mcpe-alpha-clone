#ifndef NET_MINECRAFT_WORLD_LEVEL_CHUNK__ChunkCache_H__
#define NET_MINECRAFT_WORLD_LEVEL_CHUNK__ChunkCache_H__

// Infinite terrain support: replaced fixed-size chunk array with unordered_map.
// The world now generates chunks on-demand at any (x, z) coordinate.

#include "ChunkSource.h"
#include "storage/ChunkStorage.h"
#include "EmptyLevelChunk.h"
#include "../Level.h"
#include "../LevelConstants.h"
#include <unordered_map>
#include <cstdint>

// Pack two 32-bit chunk coords into one 64-bit key
static inline int64_t chunkKey(int x, int z) {
    return ((int64_t)(uint32_t)x) | (((int64_t)(uint32_t)z) << 32);
}

class ChunkCache: public ChunkSource {
    static const int MAX_SAVES = 2;
public:
    ChunkCache(Level* level_, ChunkStorage* storage_, ChunkSource* source_)
    :   xLast(-999999999),
        zLast(-999999999),
        last(NULL),
        level(level_),
        storage(storage_),
        source(source_)
    {
        isChunkCache = true;
        emptyChunk = new EmptyLevelChunk(level_, NULL, 0, 0);
    }

    ~ChunkCache() {
        delete source;
        delete emptyChunk;

        for (auto& pair : chunkMap) {
            if (pair.second && pair.second != emptyChunk) {
                pair.second->deleteBlockData();
                delete pair.second;
            }
        }
        chunkMap.clear();
    }

    // Infinite world — all chunk coords are valid
    bool hasChunk(int x, int z) {
        if (x == xLast && z == zLast && last != NULL)
            return true;
        return chunkMap.find(chunkKey(x, z)) != chunkMap.end();
    }

    LevelChunk* create(int x, int z) {
        return getChunk(x, z);
    }

    LevelChunk* getChunk(int x, int z) {
        if (x == xLast && z == zLast && last != NULL)
            return last;

        int64_t key = chunkKey(x, z);
        auto it = chunkMap.find(key);
        if (it != chunkMap.end()) {
            xLast = x; zLast = z; last = it->second;
            return it->second;
        }

        // Try loading from disk first
        LevelChunk* newChunk = load(x, z);
        bool updatedFromDisk = false;

        if (newChunk == NULL) {
            if (source == NULL) {
                newChunk = emptyChunk;
            } else {
                newChunk = source->getChunk(x, z);
            }
        } else {
            updatedFromDisk = true;
        }

        chunkMap[key] = newChunk;
        newChunk->lightLava();

        if (updatedFromDisk) {
            // Recompute sky/block lighting for chunks loaded from disk
            for (int cx = 0; cx < 16; cx++) {
                for (int cz = 0; cz < 16; cz++) {
                    int height = level->getHeightmap(cx + x * 16, cz + z * 16);
                    for (int cy = height; cy >= 0; cy--) {
                        level->updateLight(LightLayer::Sky,
                            cx + x*16, cy, cz + z*16,
                            cx + x*16, cy, cz + z*16);
                        level->updateLight(LightLayer::Block,
                            cx + x*16 - 1, cy, cz + z*16 - 1,
                            cx + x*16 + 1, cy, cz + z*16 + 1);
                    }
                }
            }
        }

        if (newChunk != emptyChunk)
            newChunk->load();

        // Post-process (feature generation) when all 4 adjacent chunks exist
        if (!newChunk->terrainPopulated
            && hasChunk(x+1, z+1) && hasChunk(x, z+1) && hasChunk(x+1, z))
            postProcess(this, x, z);
        if (hasChunk(x-1, z) && !getChunk(x-1, z)->terrainPopulated
            && hasChunk(x-1, z+1) && hasChunk(x, z+1))
            postProcess(this, x-1, z);
        if (hasChunk(x, z-1) && !getChunk(x, z-1)->terrainPopulated
            && hasChunk(x+1, z-1) && hasChunk(x+1, z))
            postProcess(this, x, z-1);
        if (hasChunk(x-1, z-1) && !getChunk(x-1, z-1)->terrainPopulated
            && hasChunk(x, z-1) && hasChunk(x-1, z))
            postProcess(this, x-1, z-1);

        xLast = x; zLast = z; last = newChunk;
        return newChunk;
    }

    Biome::MobList getMobsAt(const MobCategory& mobCategory, int x, int y, int z) {
        return source->getMobsAt(mobCategory, x, y, z);
    }

    void postProcess(ChunkSource* parent, int x, int z) {
        LevelChunk* chunk = getChunk(x, z);
        if (!chunk->terrainPopulated) {
            chunk->terrainPopulated = true;
            if (source != NULL) {
                source->postProcess(parent, x, z);
                chunk->clearUpdateMap();
            }
        }
    }

    bool tick() {
        if (storage != NULL) storage->tick();
        return source->tick();
    }

    bool shouldSave() { return true; }

    std::string gatherStats() {
        char buf[64];
        snprintf(buf, sizeof(buf), "ChunkCache: %d chunks loaded", (int)chunkMap.size());
        return std::string(buf);
    }

    void saveAll(bool onlyUnsaved) {
        if (storage != NULL) {
            std::vector<LevelChunk*> toSave;
            for (auto& pair : chunkMap) {
                LevelChunk* c = pair.second;
                if (c && c != emptyChunk && (!onlyUnsaved || c->shouldSave(false)))
                    toSave.push_back(c);
            }
            storage->saveAll(level, toSave);
        }
    }

    // Unload chunks far from player to control memory usage.
    // Call periodically (e.g., every N ticks from Level::tick).
    void evictFarChunks(int playerChunkX, int playerChunkZ, int keepRadius) {
        std::vector<int64_t> toErase;
        for (auto& pair : chunkMap) {
            LevelChunk* c = pair.second;
            if (!c || c == emptyChunk) continue;
            int dx = c->x - playerChunkX;
            int dz = c->z - playerChunkZ;
            if (dx < -keepRadius || dx > keepRadius || dz < -keepRadius || dz > keepRadius) {
                // Save before unloading
                if (storage != NULL) {
                    storage->save(level, c);
                    storage->saveEntities(level, c);
                }
                c->unload();
                c->deleteBlockData();
                delete c;
                toErase.push_back(pair.first);
            }
        }
        for (int64_t k : toErase) {
            chunkMap.erase(k);
        }
        // Invalidate position cache if it was evicted
        if (chunkMap.find(chunkKey(xLast, zLast)) == chunkMap.end()) {
            last = NULL;
        }
    }

public:
    int xLast;
    int zLast;

private:
    LevelChunk* load(int x, int z) {
        if (storage == NULL) return NULL;
        LevelChunk* levelChunk = storage->load(level, x, z);
        if (levelChunk != NULL)
            levelChunk->lastSaveTime = level->getTime();
        return levelChunk;
    }

    void saveEntities(LevelChunk* levelChunk) {
        if (storage != NULL)
            storage->saveEntities(level, levelChunk);
    }

    void save(LevelChunk* levelChunk) {
        if (storage != NULL) {
            levelChunk->lastSaveTime = level->getTime();
            storage->save(level, levelChunk);
        }
    }

    LevelChunk* emptyChunk;
    ChunkSource* source;
    ChunkStorage* storage;
    Level* level;
    LevelChunk* last;

    std::unordered_map<int64_t, LevelChunk*> chunkMap;
};

#endif /*NET_MINECRAFT_WORLD_LEVEL_CHUNK__ChunkCache_H__*/
