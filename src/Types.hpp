#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct Vec3i {
    int x = 0;
    int y = 0;
    int z = 0;
};

inline Vec3 operator+(const Vec3& a, const Vec3& b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline Vec3 operator-(const Vec3& a, const Vec3& b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline Vec3 operator*(const Vec3& a, float s) { return {a.x * s, a.y * s, a.z * s}; }
inline Vec3 operator/(const Vec3& a, float s) { return {a.x / s, a.y / s, a.z / s}; }
inline Vec3& operator+=(Vec3& a, const Vec3& b) { a.x += b.x; a.y += b.y; a.z += b.z; return a; }
inline Vec3& operator*=(Vec3& a, float s) { a.x *= s; a.y *= s; a.z *= s; return a; }

inline bool operator==(const Vec3i& a, const Vec3i& b) { return a.x == b.x && a.y == b.y && a.z == b.z; }
inline bool operator!=(const Vec3i& a, const Vec3i& b) { return !(a == b); }
inline Vec3i operator+(const Vec3i& a, const Vec3i& b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline Vec3i operator-(const Vec3i& a, const Vec3i& b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline Vec3i operator*(const Vec3i& a, int s) { return {a.x * s, a.y * s, a.z * s}; }

inline float dot(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline float length(const Vec3& v) { return std::sqrt(dot(v, v)); }

inline Vec3 normalize(const Vec3& v) {
    float len = length(v);
    if (len <= 0.0001f) {
        return {};
    }
    return v / len;
}

inline float distance(const Vec3& a, const Vec3& b) {
    return length(a - b);
}

inline Vec3 toVec3(const Vec3i& v) {
    return {static_cast<float>(v.x), static_cast<float>(v.y), static_cast<float>(v.z)};
}

enum class BlockType : uint8_t {
    Air,
    Stone,
    Dirt,
    FuelOre,
    MetalOre,
    PlayerWall,
    ArmourBlock,
    MiningBlock,
    MissileSilo
};

enum class TurnPhase {
    MineBuild,
    AttackDefend
};

enum class MissileState {
    Inactive,
    Flying,
    Exploded
};

enum class ToolType : uint8_t {
    Mine,
    Wall,
    Armour,
    Drill,
    Silo,
    Fire
};

struct Block {
    BlockType type = BlockType::Air;
    int ownerId = -1;
};

struct Wind {
    Vec3 direction = {0.0f, 0.0f, 0.0f};
    float strength = 0.0f;
};

struct MiningBlockEntity {
    Vec3i position{};
    int ownerId = -1;
    float drillProgress = 0.0f;
    bool active = true;
};

struct Missile {
    Vec3 position{};
    Vec3 velocity{};
    Vec3 launchDirection{};
    int ownerId = -1;
    MissileState state = MissileState::Inactive;
    float age = 0.0f;
    float blastRadius = 6.0f;
};

struct Explosion {
    Vec3 position{};
    float radius = 0.0f;
    float ttl = 0.0f;
    Vec3 normal{};
};

struct MineAction {
    bool active = false;
    Vec3i target{};
    float progress = 0.0f;
};

struct Player {
    int id = 0;
    std::string name;
    Vec3i position{};
    Vec3i facing{1, 0, 0};
    int fuel = 24;
    int metal = 36;
    int health = 100;
    ToolType tool = ToolType::Mine;
    float moveCooldown = 0.0f;
    float aimAngleDeg = 42.0f;
    float launchPower = 10.5f;
    MineAction mining{};
};

struct TurnSystem {
    int turnNumber = 1;
    float turnDuration = 90.0f;
    float timeRemaining = 90.0f;
};

inline TurnPhase phaseForTurn(int turnNumber) {
    return (turnNumber % 2 == 1) ? TurnPhase::MineBuild : TurnPhase::AttackDefend;
}

inline Vec3i drillDirectionForPlayer(int playerId) {
    return (playerId == 0) ? Vec3i{1, 0, 0} : Vec3i{-1, 0, 0};
}

inline bool isStructure(BlockType type) {
    return type == BlockType::PlayerWall
        || type == BlockType::ArmourBlock
        || type == BlockType::MiningBlock
        || type == BlockType::MissileSilo;
}

inline float clampf(float v, float lo, float hi) {
    return std::max(lo, std::min(v, hi));
}
