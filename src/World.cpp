#include "World.hpp"

#include <random>

namespace {
constexpr int kCrustThickness = 2;

bool insidePlanetBounds(const Vec3i& p, const PlanetRegion& region) {
    return p.x >= region.minX && p.x <= region.maxX
        && p.y >= region.minY && p.y <= region.maxY
        && p.z >= region.minZ && p.z <= region.maxZ;
}

bool nearOuterShell(const Vec3i& p, const PlanetRegion& region) {
    return p.x < region.minX + kCrustThickness
        || p.y < region.minY + kCrustThickness
        || p.z < region.minZ + kCrustThickness
        || p.x > region.maxX - kCrustThickness
        || p.y > region.maxY - kCrustThickness
        || p.z > region.maxZ - kCrustThickness;
}

int shellDepth(const Vec3i& p, const PlanetRegion& region) {
    int dx = std::min(p.x - region.minX, region.maxX - p.x);
    int dy = std::min(p.y - region.minY, region.maxY - p.y);
    int dz = std::min(p.z - region.minZ, region.maxZ - p.z);
    return std::min({dx, dy, dz});
}

PlanetStyle makePlanetStyle(std::mt19937& rng) {
    std::uniform_int_distribution<int> themeDist(0, 3);
    std::uniform_int_distribution<int> caveDist(0, 3);
    std::uniform_real_distribution<float> roughnessDist(0.85f, 1.35f);
    std::uniform_real_distribution<float> pulseDist(0.7f, 1.35f);

    PlanetStyle style;
    style.theme = static_cast<PlanetTheme>(themeDist(rng));
    style.cavePattern = static_cast<CavePattern>(caveDist(rng));
    style.roughness = roughnessDist(rng);
    style.glowPulse = pulseDist(rng);

    switch (style.theme) {
        case PlanetTheme::Ember:
            style.fuelBias = 1.35f;
            style.metalBias = 0.82f;
            break;
        case PlanetTheme::Azure:
            style.fuelBias = 0.88f;
            style.metalBias = 1.28f;
            break;
        case PlanetTheme::Auric:
            style.fuelBias = 1.18f;
            style.metalBias = 1.05f;
            break;
        case PlanetTheme::Iron:
            style.fuelBias = 0.82f;
            style.metalBias = 1.38f;
            break;
    }

    return style;
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
    return overallCenter_;
}

void World::generate() {
    static std::mt19937 rng(std::random_device{}());
    constexpr int kSizeSteps = (kMaxPlanetSize - kMinPlanetSize) / 2;
    constexpr int kTwinSizeSteps = (kTwinMaxPlanetSize - kTwinMinPlanetSize) / 2;
    std::uniform_real_distribution<float> sizeBiasRoll(0.0f, 1.0f);
    std::uniform_real_distribution<float> levelTypeRoll(0.0f, 1.0f);

    blocks_.assign(blocks_.size(), {BlockType::Air, -1});

    auto biasedPlanetSize = [&]() {
        float biasedStep = sizeBiasRoll(rng);
        int sizeStep = static_cast<int>(biasedStep * biasedStep * static_cast<float>(kSizeSteps + 1));
        if (sizeStep > kSizeSteps) {
            sizeStep = kSizeSteps;
        }
        return kMinPlanetSize + sizeStep * 2;
    };

    auto biasedTwinPlanetSize = [&]() {
        float biasedStep = sizeBiasRoll(rng);
        int sizeStep = static_cast<int>(biasedStep * biasedStep * static_cast<float>(kTwinSizeSteps + 1));
        if (sizeStep > kTwinSizeSteps) {
            sizeStep = kTwinSizeSteps;
        }
        return kTwinMinPlanetSize + sizeStep * 2;
    };

    auto sculptPlanetShell = [&](const PlanetRegion& region, const PlanetStyle& style) {
        std::uniform_real_distribution<float> edgeRoll(0.0f, 1.0f);
        std::uniform_int_distribution<int> bayDepthDist(2, 4);
        std::uniform_int_distribution<int> baySpanDist(2, 5);
        std::uniform_int_distribution<int> chipZDist(region.minZ + 1, region.maxZ - 1);
        std::uniform_int_distribution<int> chipYDist(region.minY + 2, region.maxY - 2);

        int chipCount = 2 + static_cast<int>(std::round(style.roughness * 2.5f));
        for (int chip = 0; chip < chipCount; ++chip) {
            int depth = bayDepthDist(rng);
            int span = baySpanDist(rng);
            bool fromLeft = edgeRoll(rng) < 0.5f;
            bool fromTop = edgeRoll(rng) < 0.5f;
            int cy = chipYDist(rng);
            int cz = chipZDist(rng);
            int cx = (region.minX + region.maxX) / 2;
            for (int z = std::max(region.minZ + 1, cz - span / 2); z <= std::min(region.maxZ - 1, cz + span / 2); ++z) {
                for (int y = std::max(region.minY + 1, cy - span); y <= std::min(region.maxY - 1, cy + span); ++y) {
                    for (int d = 0; d < depth; ++d) {
                        int x = fromLeft ? region.minX + d : region.maxX - d;
                        int topBottomDepth = fromTop ? region.minY + d : region.maxY - d;
                        set({x, y, z}, BlockType::Air);
                        for (int x2 = std::max(region.minX + 1, cx - span); x2 <= std::min(region.maxX - 1, cx + span); ++x2) {
                            set({x2, topBottomDepth, z}, BlockType::Air);
                        }
                    }
                }
            }
        }

        std::uniform_int_distribution<int> ventYDist(region.minY + 2, region.maxY - 2);
        for (int vent = 0; vent < 2; ++vent) {
            int cy = ventYDist(rng);
            int depth = 2 + vent;
            for (int y = std::max(region.minY + 1, cy - 1); y <= std::min(region.maxY - 1, cy + 1); ++y) {
                for (int z = region.minZ + 1; z <= region.maxZ - 1; ++z) {
                    for (int d = 0; d < depth; ++d) {
                        set({region.minX + d, y, z}, BlockType::Air);
                        set({region.maxX - d, y, z}, BlockType::Air);
                    }
                }
            }
        }
    };

    auto carvePlanet = [&](const PlanetRegion& region, const PlanetStyle& style) {
        std::uniform_int_distribution<int> xDist(region.minX + 3, region.maxX - 3);
        std::uniform_int_distribution<int> yDist(region.minY + 3, region.maxY - 3);
        std::uniform_int_distribution<int> zDist(region.minZ + 1, region.maxZ - 1);
        std::uniform_real_distribution<float> radiusDist(1.2f, 3.9f);
        std::uniform_real_distribution<float> oreRoll(0.0f, 1.0f);

        int caveCount = 18;
        switch (style.cavePattern) {
            case CavePattern::Chambers: caveCount = 16; break;
            case CavePattern::Faults: caveCount = 12; break;
            case CavePattern::Worms: caveCount = 24; break;
            case CavePattern::Hollow: caveCount = 10; break;
        }

        for (int i = 0; i < caveCount; ++i) {
            Vec3 caveCenter{
                static_cast<float>(xDist(rng)),
                static_cast<float>(yDist(rng)),
                static_cast<float>(zDist(rng))
            };
            float radius = radiusDist(rng);
            for (int z = region.minZ; z <= region.maxZ; ++z) {
                for (int y = region.minY; y <= region.maxY; ++y) {
                    for (int x = region.minX; x <= region.maxX; ++x) {
                        Vec3i p{x, y, z};
                        if (!insidePlanetBounds(p, region) || nearOuterShell(p, region)) {
                            continue;
                        }
                        Vec3 point = toVec3(p);
                        float metric = distance(point, caveCenter);
                        if (style.cavePattern == CavePattern::Faults) {
                            metric = std::fabs((point.x - caveCenter.x) * 0.45f + (point.y - caveCenter.y))
                                + std::fabs(point.z - caveCenter.z) * 0.35f;
                        } else if (style.cavePattern == CavePattern::Worms) {
                            metric = distance(point, caveCenter + Vec3{
                                std::sin(point.y * 0.45f + i) * 1.4f,
                                std::cos(point.x * 0.33f + i) * 1.2f,
                                0.0f
                            });
                        } else if (style.cavePattern == CavePattern::Hollow) {
                            Vec3 c = region.center();
                            float innerRadius = std::min({region.sizeX(), region.sizeY(), region.sizeZ()}) * 0.23f;
                            if (distance(point, c) < innerRadius + std::sin(point.x + point.y) * 0.8f) {
                                set(p, BlockType::Air);
                                continue;
                            }
                        }

                        if (metric <= radius) {
                            set(p, BlockType::Air);
                        }
                    }
                }
            }
        }

        Vec3 c = region.center();
        float maxCoreDistance = distance(toVec3({region.minX, region.minY, region.minZ}), c);
        for (int z = region.minZ + 1; z < region.maxZ; ++z) {
            for (int y = region.minY + 1; y < region.maxY; ++y) {
                for (int x = region.minX + 1; x < region.maxX; ++x) {
                    Vec3i p{x, y, z};
                    if (get(p).type == BlockType::Air || isStructure(get(p).type)) {
                        continue;
                    }
                    float distToCore = distance(toVec3(p), c);
                    float richness = 1.0f - (distToCore / maxCoreDistance);
                    float fuelChance = (0.02f + richness * 0.10f) * style.fuelBias;
                    float metalChance = (0.03f + richness * 0.12f) * style.metalBias;
                    float roll = oreRoll(rng);
                    if (roll < fuelChance) {
                        set(p, BlockType::FuelOre);
                    } else if (roll < fuelChance + metalChance) {
                        set(p, BlockType::MetalOre);
                    }
                }
            }
        }
    };

    auto ensureMinimumResourceCoverage = [&](const PlanetRegion& region, const PlanetStyle& style, float minCoverage) {
        std::vector<Vec3i> candidateCells;
        int solidCells = 0;
        int resourceCells = 0;

        for (int z = region.minZ; z <= region.maxZ; ++z) {
            for (int y = region.minY; y <= region.maxY; ++y) {
                for (int x = region.minX; x <= region.maxX; ++x) {
                    Vec3i p{x, y, z};
                    Block block = get(p);
                    if (block.type == BlockType::Air || isStructure(block.type)) {
                        continue;
                    }
                    ++solidCells;
                    if (block.type == BlockType::FuelOre || block.type == BlockType::MetalOre) {
                        ++resourceCells;
                    } else {
                        candidateCells.push_back(p);
                    }
                }
            }
        }

        if (solidCells <= 0) {
            return;
        }

        int requiredResources = static_cast<int>(std::ceil(static_cast<float>(solidCells) * minCoverage));
        int needed = std::max(0, requiredResources - resourceCells);
        if (needed <= 0 || candidateCells.empty()) {
            return;
        }

        std::shuffle(candidateCells.begin(), candidateCells.end(), rng);
        std::uniform_real_distribution<float> oreRoll(0.0f, 1.0f);
        for (int i = 0; i < needed && i < static_cast<int>(candidateCells.size()); ++i) {
            float roll = oreRoll(rng);
            set(candidateCells[i], roll < (style.fuelBias / (style.fuelBias + style.metalBias))
                ? BlockType::FuelOre
                : BlockType::MetalOre);
        }
    };

    bool useTwinPlanets = levelTypeRoll(rng) < 0.5f;
    if (useTwinPlanets) {
        styles_[0] = makePlanetStyle(rng);
        styles_[1] = makePlanetStyle(rng);
        if (styles_[1].theme == styles_[0].theme) {
            styles_[1].theme = static_cast<PlanetTheme>((static_cast<int>(styles_[1].theme) + 1) % 4);
        }
        if (styles_[1].cavePattern == styles_[0].cavePattern) {
            styles_[1].cavePattern = static_cast<CavePattern>((static_cast<int>(styles_[1].cavePattern) + 1) % 4);
        }

        auto makeTwinDimensions = [&]() {
            const int baseSize = biasedTwinPlanetSize();
            std::uniform_int_distribution<int> axisDeltaDist(-2, 2);
            int sizeX = static_cast<int>(clampf(
                static_cast<float>(baseSize + axisDeltaDist(rng) * 2),
                static_cast<float>(kTwinMinPlanetSize),
                static_cast<float>(kTwinMaxPlanetSize)
            ));
            int sizeY = static_cast<int>(clampf(
                static_cast<float>(baseSize + axisDeltaDist(rng) * 2),
                static_cast<float>(kTwinMinPlanetSize),
                static_cast<float>(kTwinMaxPlanetSize)
            ));
            int sizeZ = static_cast<int>(clampf(
                static_cast<float>(baseSize + axisDeltaDist(rng) * 2),
                static_cast<float>(kTwinMinPlanetSize),
                static_cast<float>(kTwinMaxPlanetSize)
            ));
            return std::array<int, 3>{sizeX, sizeY, sizeZ};
        };

        std::array<int, 3> dimsA = makeTwinDimensions();
        std::array<int, 3> dimsB = makeTwinDimensions();
        if (dimsB == dimsA) {
            dimsB[0] = std::max(kTwinMinPlanetSize, dimsB[0] - 2);
            dimsB[1] = std::min(kTwinMaxPlanetSize, dimsB[1] + 2);
        }

        int sizeAX = dimsA[0];
        int sizeAY = dimsA[1];
        int sizeAZ = dimsA[2];
        int sizeBX = dimsB[0];
        int sizeBY = dimsB[1];
        int sizeBZ = dimsB[2];
        std::uniform_int_distribution<int> gapDist(10, 15);
        int gap = gapDist(rng);

        levelType_ = LevelType::TwinPlanets;
        planetCount_ = 2;

        int totalWidth = sizeAX + sizeBX + gap;
        int leftMinX = (kSizeX - totalWidth) / 2;
        int rightMinX = leftMinX + sizeAX + gap;
        std::uniform_int_distribution<int> staggerDist(4, 8);
        std::uniform_int_distribution<int> jitterDist(-2, 2);
        int stagger = staggerDist(rng);
        int leftMinY = static_cast<int>(clampf((kSizeY - sizeAY) / 2 - stagger + jitterDist(rng), 2.0f, static_cast<float>(kSizeY - sizeAY - 2)));
        int rightMinY = static_cast<int>(clampf((kSizeY - sizeBY) / 2 + stagger + jitterDist(rng), 2.0f, static_cast<float>(kSizeY - sizeBY - 2)));

        planets_[0] = {
            leftMinX, leftMinX + sizeAX - 1,
            leftMinY, leftMinY + sizeAY - 1,
            (kSizeZ - sizeAZ) / 2, (kSizeZ - sizeAZ) / 2 + sizeAZ - 1
        };
        planets_[1] = {
            rightMinX, rightMinX + sizeBX - 1,
            rightMinY, rightMinY + sizeBY - 1,
            (kSizeZ - sizeBZ) / 2, (kSizeZ - sizeBZ) / 2 + sizeBZ - 1
        };

        activeMinX_ = planets_[0].minX;
        activeMaxX_ = planets_[1].maxX;
        activeMinY_ = std::min(planets_[0].minY, planets_[1].minY);
        activeMaxY_ = std::max(planets_[0].maxY, planets_[1].maxY);
        overallCenter_ = (planets_[0].center() + planets_[1].center()) * 0.5f;
        planetSize_ = std::max({sizeAX, sizeAY, sizeAZ, sizeBX, sizeBY, sizeBZ});
        planetMin_ = planets_[0].minX;
        planetMax_ = planets_[1].maxX;
    } else {
        styles_[0] = makePlanetStyle(rng);
        styles_[1] = styles_[0];
        planetSize_ = biasedPlanetSize();
        levelType_ = LevelType::SinglePlanet;
        planetCount_ = 1;

        int minX = (kSizeX - planetSize_) / 2;
        int minY = (kSizeY - planetSize_) / 2;
        int minZ = (kSizeZ - planetSize_) / 2;
        planets_[0] = {minX, minX + planetSize_ - 1, minY, minY + planetSize_ - 1, minZ, minZ + planetSize_ - 1};
        planets_[1] = planets_[0];
        activeMinX_ = planets_[0].minX;
        activeMaxX_ = planets_[0].maxX;
        activeMinY_ = planets_[0].minY;
        activeMaxY_ = planets_[0].maxY;
        overallCenter_ = planets_[0].center();
        planetMin_ = planets_[0].minX;
        planetMax_ = planets_[0].maxX;
    }

    for (int i = 0; i < planetCount_; ++i) {
        const PlanetRegion& region = planets_[i];
        for (int z = region.minZ; z <= region.maxZ; ++z) {
            for (int y = region.minY; y <= region.maxY; ++y) {
                for (int x = region.minX; x <= region.maxX; ++x) {
                    Vec3i p{x, y, z};
                    BlockType type = nearOuterShell(p, region) ? BlockType::Dirt : BlockType::Stone;
                    set(p, type);
                }
            }
        }
    }

    for (int i = 0; i < planetCount_; ++i) {
        sculptPlanetShell(planets_[i], styles_[i]);
        carvePlanet(planets_[i], styles_[i]);
    }

    if (levelType_ == LevelType::TwinPlanets) {
        for (int i = 0; i < planetCount_; ++i) {
            ensureMinimumResourceCoverage(planets_[i], styles_[i], 0.35f);
        }
    }

    for (int i = 0; i < planetCount_; ++i) {
        const PlanetRegion& region = planets_[i];
        for (int z = region.minZ; z <= region.maxZ; ++z) {
            for (int y = region.minY; y <= region.maxY; ++y) {
                for (int x = region.minX; x <= region.maxX; ++x) {
                    Vec3i p{x, y, z};
                    Block block = get(p);
                    if (block.type == BlockType::Air || isStructure(block.type)) {
                        continue;
                    }
                    if (shellDepth(p, region) <= 0) {
                        set(p, BlockType::Dirt);
                    }
                }
            }
        }
    }
}
