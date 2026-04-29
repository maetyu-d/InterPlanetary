#pragma once

#include "Types.hpp"

class World {
public:
    static constexpr int kMinPlanetSize = 19;
    static constexpr int kMaxPlanetSize = 29;
    static constexpr int kSizeX = kMaxPlanetSize;
    static constexpr int kSizeY = kMaxPlanetSize;
    static constexpr int kSizeZ = kMaxPlanetSize;
    static constexpr int kSliceZ = kSizeZ / 2;

    World();

    void generate();

    bool inBounds(const Vec3i& p) const;
    Block get(const Vec3i& p) const;
    Block& at(const Vec3i& p);
    void set(const Vec3i& p, BlockType type, int ownerId = -1);
    bool isSolid(const Vec3i& p) const;
    bool isAir(const Vec3i& p) const;
    Vec3 center() const;
    int planetSize() const { return planetSize_; }
    int planetMin() const { return planetMin_; }
    int planetMax() const { return planetMax_; }
    int sliceZ() const { return kSliceZ; }

    int sizeX() const { return kSizeX; }
    int sizeY() const { return kSizeY; }
    int sizeZ() const { return kSizeZ; }

private:
    int index(const Vec3i& p) const;

    std::vector<Block> blocks_;
    int planetSize_ = kMinPlanetSize;
    int planetMin_ = (kMaxPlanetSize - kMinPlanetSize) / 2;
    int planetMax_ = planetMin_ + kMinPlanetSize - 1;
};
