#pragma once

#include "Types.hpp"

#include <array>

enum class LevelType {
    SinglePlanet,
    TwinPlanets
};

enum class PlanetTheme : uint8_t {
    Ember,
    Azure,
    Auric,
    Iron
};

enum class CavePattern : uint8_t {
    Chambers,
    Faults,
    Worms,
    Hollow
};

struct PlanetStyle {
    PlanetTheme theme = PlanetTheme::Azure;
    CavePattern cavePattern = CavePattern::Chambers;
    float fuelBias = 1.0f;
    float metalBias = 1.0f;
    float roughness = 1.0f;
    float glowPulse = 1.0f;
};

struct PlanetRegion {
    int minX = 0;
    int maxX = 0;
    int minY = 0;
    int maxY = 0;
    int minZ = 0;
    int maxZ = 0;

    int sizeX() const { return maxX - minX + 1; }
    int sizeY() const { return maxY - minY + 1; }
    int sizeZ() const { return maxZ - minZ + 1; }
    int centerY() const { return (minY + maxY) / 2; }
    Vec3 center() const {
        return {
            minX + sizeX() * 0.5f,
            minY + sizeY() * 0.5f,
            minZ + sizeZ() * 0.5f
        };
    }
};

class World {
public:
    static constexpr int kMinPlanetSize = 19;
    static constexpr int kMaxPlanetSize = 29;
    static constexpr int kTwinMinPlanetSize = 10;
    static constexpr int kTwinMaxPlanetSize = 14;
    static constexpr int kSizeX = 78;
    static constexpr int kSizeY = 44;
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
    LevelType levelType() const { return levelType_; }
    int planetCount() const { return planetCount_; }
    const PlanetRegion& planetForPlayer(int playerId) const { return planets_[playerId == 0 ? 0 : planetCount_ - 1]; }
    const PlanetStyle& styleForPlayer(int playerId) const { return styles_[playerId == 0 ? 0 : planetCount_ - 1]; }
    const PlanetStyle& styleForPlanetIndex(int index) const { return styles_[index]; }
    int activeMinX() const { return activeMinX_; }
    int activeMaxX() const { return activeMaxX_; }
    int activeMinY() const { return activeMinY_; }
    int activeMaxY() const { return activeMaxY_; }
    int sliceZ() const { return kSliceZ; }

    int sizeX() const { return kSizeX; }
    int sizeY() const { return kSizeY; }
    int sizeZ() const { return kSizeZ; }

private:
    int index(const Vec3i& p) const;

    std::vector<Block> blocks_;
    LevelType levelType_ = LevelType::SinglePlanet;
    int planetCount_ = 1;
    std::array<PlanetRegion, 2> planets_{};
    std::array<PlanetStyle, 2> styles_{};
    Vec3 overallCenter_{};
    int activeMinX_ = 0;
    int activeMaxX_ = kSizeX - 1;
    int activeMinY_ = 0;
    int activeMaxY_ = kSizeY - 1;
    int planetSize_ = kMinPlanetSize;
    int planetMin_ = (kSizeX - kMinPlanetSize) / 2;
    int planetMax_ = planetMin_ + kMinPlanetSize - 1;
};
