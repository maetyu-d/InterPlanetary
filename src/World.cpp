#include "World.hpp"

#include <random>

namespace {
constexpr int kCrustThickness = 2;

bool insidePlanetBounds(const Vec3i& p, int planetMin, int planetMax) {
    return p.x >= planetMin && p.x <= planetMax
        && p.y >= planetMin && p.y <= planetMax
        && p.z >= planetMin && p.z <= planetMax;
}

bool nearOuterShell(const Vec3i& p, int planetMin, int planetMax) {
    return p.x < planetMin + kCrustThickness
        || p.y < planetMin + kCrustThickness
        || p.z < planetMin + kCrustThickness
        || p.x > planetMax - kCrustThickness
        || p.y > planetMax - kCrustThickness
        || p.z > planetMax - kCrustThickness;
}
}

World::World() : blocks_(kSizeX * kSizeY * kSizeZ) {
    generate();
}

int World::index(const Vec3i& p) const {
    return p.x + p.y * kSizeX + p.z * kSizeX * kSizeY;
}

bool World::inBounds(const Vec3i& p) const {
    return p.x >= 0 && p.x < kSizeX
        && p.y >= 0 && p.y < kSizeY
        && p.z >= 0 && p.z < kSizeZ;
}

Block World::get(const Vec3i& p) const {
    if (!inBounds(p)) {
        return {BlockType::Air, -1};
    }
    return blocks_[index(p)];
}

Block& World::at(const Vec3i& p) {
    return blocks_[index(p)];
}

void World::set(const Vec3i& p, BlockType type, int ownerId) {
    if (!inBounds(p)) {
        return;
    }
    blocks_[index(p)] = {type, ownerId};
}

bool World::isSolid(const Vec3i& p) const {
    return get(p).type != BlockType::Air;
}

bool World::isAir(const Vec3i& p) const {
    return get(p).type == BlockType::Air;
}

Vec3 World::center() const {
    float c = planetMin_ + planetSize_ * 0.5f;
    return {c, c, c};
}

void World::generate() {
    static std::mt19937 rng(1337);
    std::uniform_int_distribution<int> sizeStepDist(0, (kMaxPlanetSize - kMinPlanetSize) / 2);
    planetSize_ = kMinPlanetSize + sizeStepDist(rng) * 2;
    planetMin_ = (kMaxPlanetSize - planetSize_) / 2;
    planetMax_ = planetMin_ + planetSize_ - 1;

    for (int z = 0; z < kSizeZ; ++z) {
        for (int y = 0; y < kSizeY; ++y) {
            for (int x = 0; x < kSizeX; ++x) {
                Vec3i p{x, y, z};
                if (!insidePlanetBounds(p, planetMin_, planetMax_)) {
                    set(p, BlockType::Air);
                    continue;
                }
                BlockType type = nearOuterShell(p, planetMin_, planetMax_) ? BlockType::Dirt : BlockType::Stone;
                set(p, type);
            }
        }
    }

    std::uniform_int_distribution<int> xDist(planetMin_ + 3, planetMax_ - 3);
    std::uniform_int_distribution<int> yDist(planetMin_ + 3, planetMax_ - 3);
    std::uniform_int_distribution<int> zDist(planetMin_ + 1, planetMax_ - 1);
    std::uniform_real_distribution<float> radiusDist(1.4f, 3.5f);
    std::uniform_real_distribution<float> oreRoll(0.0f, 1.0f);

    // Carve cave bubbles so the cube feels like a sealed planet with buried voids.
    for (int i = 0; i < 22; ++i) {
        Vec3 center{
            static_cast<float>(xDist(rng)),
            static_cast<float>(yDist(rng)),
            static_cast<float>(zDist(rng))
        };
        float radius = radiusDist(rng);
        for (int z = 0; z < kSizeZ; ++z) {
            for (int y = 0; y < kSizeY; ++y) {
                for (int x = 0; x < kSizeX; ++x) {
                    Vec3i p{x, y, z};
                    if (!insidePlanetBounds(p, planetMin_, planetMax_) || nearOuterShell(p, planetMin_, planetMax_)) {
                        continue;
                    }
                    Vec3 worldPos = toVec3(p);
                    if (distance(worldPos, center) <= radius) {
                        set(p, BlockType::Air);
                    }
                }
            }
        }
    }

    // The deeper toward the core, the richer the planet becomes.
    Vec3 c = center();
    float maxCoreDistance = distance({0.0f, 0.0f, 0.0f}, c);
    for (int z = planetMin_ + 1; z < planetMax_; ++z) {
        for (int y = planetMin_ + 1; y < planetMax_; ++y) {
            for (int x = planetMin_ + 1; x < planetMax_; ++x) {
                Vec3i p{x, y, z};
                if (get(p).type == BlockType::Air || isStructure(get(p).type)) {
                    continue;
                }
                float distToCore = distance(toVec3(p), c);
                float richness = 1.0f - (distToCore / maxCoreDistance);
                float fuelChance = 0.02f + richness * 0.10f;
                float metalChance = 0.03f + richness * 0.12f;
                float roll = oreRoll(rng);
                if (roll < fuelChance) {
                    set(p, BlockType::FuelOre);
                } else if (roll < fuelChance + metalChance) {
                    set(p, BlockType::MetalOre);
                }
            }
        }
    }
}
