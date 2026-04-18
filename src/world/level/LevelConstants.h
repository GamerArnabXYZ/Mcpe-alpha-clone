#ifndef _MINECRAFT_WORLD_LEVELCONSTANTS_H_
#define _MINECRAFT_WORLD_LEVELCONSTANTS_H_

#include <climits>

const int LEVEL_HEIGHT = 128;
// CHUNK_CACHE_WIDTH: used only by LevelRenderer for render grid size.
// The actual world/chunk storage is now infinite (unordered_map in ChunkCache).
const int CHUNK_CACHE_WIDTH = 32; // render-grid width in chunks (kept for renderer)
const int CHUNK_WIDTH = 16; // in blocks
const int CHUNK_DEPTH = 16;
// LEVEL_WIDTH/DEPTH: used only for renderer alloc estimates; NOT used as world bounds.
const int LEVEL_WIDTH = CHUNK_CACHE_WIDTH * CHUNK_WIDTH;
const int LEVEL_DEPTH = CHUNK_CACHE_WIDTH * CHUNK_DEPTH;
const int CHUNK_COLUMNS = CHUNK_WIDTH * CHUNK_DEPTH;
const int CHUNK_BLOCK_COUNT = CHUNK_COLUMNS * LEVEL_HEIGHT;

#endif
