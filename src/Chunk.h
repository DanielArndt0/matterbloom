#pragma once
#include "Particle.h"
#include <vector>

const int CHUNK_SIZE = 32;

class Chunk
{
public:
    Chunk();
    ~Chunk() = default;

    bool isDirty() const { return dirty; }
    void setDirty(bool flag) { dirty = flag; }

    void updateBounds(int localX, int localY);
    void resetBounds();

    int getMinX() const { return minX; }
    int getMaxX() const { return maxX; }
    int getMinY() const { return minY; }
    int getMaxY() const { return maxY; }

private:
    bool dirty = false;

    int minX, maxX, minY, maxY;
};
