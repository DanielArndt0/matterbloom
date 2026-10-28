#include "Chunk.h"
#include <algorithm>

Chunk::Chunk()
{
    resetBounds();
}

void Chunk::updateBounds(int localX, int localY)
{
    if (localX < minX)
        minX = localX;
    if (localX > maxX)
        maxX = localX;
    if (localY < minY)
        minY = localY;
    if (localY > maxY)
        maxY = localY;
    dirty = true;
}

void Chunk::resetBounds()
{
    minX = CHUNK_SIZE - 1;
    maxX = 0;
    minY = CHUNK_SIZE - 1;
    maxY = 0;
    dirty = false;
}
