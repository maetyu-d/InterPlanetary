#include "Game.hpp"

#include "raylib.h"

#include <array>
#include <cmath>
#include <cstdio>
#include <random>

namespace {
constexpr float kMoveDelay = 0.12f;
constexpr int kFuelOreYield = 5;
constexpr int kMetalOreYield = 5;
constexpr int kWallCost = 1;
constexpr int kArmourCost = 4;
constexpr int kSiloCost = 30;
constexpr int kMissileFuelCost = 15;
constexpr int kMissileMetalCost = 20;
constexpr float kExplosionRadius = 3.0f;
constexpr float kGravityMu = 1550.0f;
constexpr float kDrag = 0.0025f;
constexpr float kLaunchClearance = 1.9f;
constexpr float kLaunchBoostDuration = 0.22f;
constexpr float kLaunchBoostStrength = 3.8f;
constexpr float kMissileWindFactor = 0.08f;
constexpr int kMissileSubsteps = 8;
int gPlanetMin = 0;
int gPlanetSize = World::kMinPlanetSize;

Color scaleColor(Color c, float scale, float alphaScale = 1.0f) {
    return {
        static_cast<unsigned char>(clampf(c.r * scale, 0.0f, 255.0f)),
        static_cast<unsigned char>(clampf(c.g * scale, 0.0f, 255.0f)),
        static_cast<unsigned char>(clampf(c.b * scale, 0.0f, 255.0f)),
        static_cast<unsigned char>(clampf(c.a * alphaScale, 0.0f, 255.0f))
    };
}

Color lerpColor(Color a, Color b, float t) {
    t = clampf(t, 0.0f, 1.0f);
    return {
        static_cast<unsigned char>(a.r + (b.r - a.r) * t),
        static_cast<unsigned char>(a.g + (b.g - a.g) * t),
        static_cast<unsigned char>(a.b + (b.b - a.b) * t),
        static_cast<unsigned char>(a.a + (b.a - a.a) * t)
    };
}

Color colorForBlock(const Block& block) {
    switch (block.type) {
        case BlockType::Air: return {10, 16, 22, 255};
        case BlockType::Stone: return {108, 120, 138, 255};
        case BlockType::Dirt: return {164, 118, 72, 255};
        case BlockType::FuelOre: return {255, 203, 52, 255};
        case BlockType::MetalOre: return {74, 225, 245, 255};
        case BlockType::PlayerWall: return block.ownerId == 0 ? Color{88, 193, 255, 255} : Color{255, 134, 154, 255};
        case BlockType::ArmourBlock: return block.ownerId == 0 ? Color{48, 133, 232, 255} : Color{222, 88, 108, 255};
        case BlockType::MiningBlock: return block.ownerId == 0 ? Color{163, 214, 255, 255} : Color{255, 180, 180, 255};
        case BlockType::MissileSilo: return block.ownerId == 0 ? Color{104, 255, 160, 255} : Color{255, 103, 190, 255};
    }
    return MAGENTA;
}

const char* toolName(ToolType tool) {
    switch (tool) {
        case ToolType::Mine: return "Mine";
        case ToolType::Wall: return "Wall";
        case ToolType::Armour: return "Armour";
        case ToolType::Drill: return "Unused";
        case ToolType::Silo: return "Silo";
        case ToolType::Fire: return "Fire";
    }
    return "?";
}

const char* phaseName(TurnPhase phase) {
    return phase == TurnPhase::MineBuild ? "Mine / Build" : "Attack / Defend";
}

Vec3 outwardNormalForPlayer(int playerId) {
    return playerId == 0 ? Vec3{-1.0f, 0.0f, 0.0f} : Vec3{1.0f, 0.0f, 0.0f};
}

Vec3 vecFromAngle(int playerId, float degrees) {
    float radians = (degrees - 45.0f) * PI / 180.0f;
    Vec3 outward = outwardNormalForPlayer(playerId);
    Vec3 inward = outward * -1.0f;
    Vec3 tangent{0.0f, -1.0f, 0.0f};

    // 45 degrees means pure tangent. Lower angles lean inward for tighter wraps; higher angles lean outward for wider arcs.
    Vec3 radialBias = radians >= 0.0f ? outward : inward;
    Vec3 dir = tangent * std::cos(std::fabs(radians)) + radialBias * std::sin(std::fabs(radians));
    return normalize(dir);
}

Rectangle worldViewRect() {
    const float hudWidth = 344.0f;
    const float margin = 132.0f;
    float playWidth = static_cast<float>(GetScreenWidth()) - hudWidth - margin * 2.0f;
    float playHeight = static_cast<float>(GetScreenHeight()) - margin * 2.0f;
    float side = std::min(playWidth, playHeight) * 0.58f;
    float x = margin + (playWidth - side) * 0.5f;
    float y = margin + (playHeight - side) * 0.5f;
    return {x, y, side, side};
}

float worldCellSize() {
    float sizeT = static_cast<float>(gPlanetSize - World::kMinPlanetSize) /
        static_cast<float>(World::kMaxPlanetSize - World::kMinPlanetSize);
    sizeT = clampf(sizeT, 0.0f, 1.0f);
    float cameraPaddingCells = 1.05f + sizeT * 2.0f;
    return worldViewRect().width / (static_cast<float>(gPlanetSize) + cameraPaddingCells);
}

float activePlanetScreenSpan() {
    return static_cast<float>(gPlanetSize) * worldCellSize();
}

Rectangle activePlanetRect() {
    Rectangle view = worldViewRect();
    float activeSpan = activePlanetScreenSpan();
    return {
        view.x + (view.width - activeSpan) * 0.5f,
        view.y + (view.height - activeSpan) * 0.5f,
        activeSpan,
        activeSpan
    };
}

Vector2 activePlanetCenter() {
    Rectangle rect = activePlanetRect();
    return {rect.x + rect.width * 0.5f, rect.y + rect.height * 0.5f};
}

float activePlanetCornerRadius() {
    float halfSpan = activePlanetScreenSpan() * 0.5f;
    return std::sqrt(halfSpan * halfSpan * 2.0f);
}

float atmosphereThicknessForPlanet(float planetSpan) {
    return clampf(planetSpan * 0.16f, 24.0f, 82.0f);
}

Vector2 cellTopLeft(const Vec3i& p) {
    Rectangle view = activePlanetRect();
    float cell = worldCellSize();
    return {
        view.x + (p.x - gPlanetMin) * cell,
        view.y + (p.y - gPlanetMin) * cell
    };
}

Vector2 worldPointToScreen(const Vec3& p) {
    Rectangle view = activePlanetRect();
    float cell = worldCellSize();
    return {
        view.x + (p.x - gPlanetMin) * cell,
        view.y + (p.y - gPlanetMin) * cell
    };
}

void drawNebulaBackdrop() {
    DrawRectangleGradientV(0, 0, GetScreenWidth(), GetScreenHeight(), Color{14, 24, 48, 255}, Color{7, 10, 24, 255});

    DrawCircleGradient(GetScreenWidth() / 2 - 140, 140, 260.0f, Fade(Color{70, 168, 255, 90}, 0.30f), Fade(BLANK, 0.0f));
    DrawCircleGradient(GetScreenWidth() / 2 + 60, GetScreenHeight() - 120, 340.0f, Fade(Color{40, 124, 220, 80}, 0.26f), Fade(BLANK, 0.0f));
    DrawCircleGradient(210, GetScreenHeight() / 2 + 90, 220.0f, Fade(Color{255, 136, 84, 58}, 0.22f), Fade(BLANK, 0.0f));
    DrawCircleGradient(GetScreenWidth() / 2 + 10, GetScreenHeight() / 2 - 140, 300.0f, Fade(Color{110, 210, 255, 88}, 0.18f), Fade(BLANK, 0.0f));
    DrawCircleGradient(GetScreenWidth() / 2 - 260, GetScreenHeight() - 240, 260.0f, Fade(Color{255, 122, 163, 52}, 0.12f), Fade(BLANK, 0.0f));

    for (int i = 0; i < 120; ++i) {
        float x = static_cast<float>((i * 97) % GetScreenWidth());
        float y = static_cast<float>((i * 53 + 37) % GetScreenHeight());
        float size = 1.0f + static_cast<float>(i % 4);
        float pulse = 0.72f + 0.28f * std::sin(GetTime() * (0.5 + (i % 7) * 0.11) + i);
        DrawCircleV({x, y}, size, Fade(RAYWHITE, 0.14f + (i % 6) * 0.035f * pulse));
    }
}

void drawOrbitalBeltField() {
    Vector2 center = activePlanetCenter();
    float t = static_cast<float>(GetTime());
    Rectangle planet = activePlanetRect();
    float cornerRadius = activePlanetCornerRadius();
    float atmosphereThickness = atmosphereThicknessForPlanet(planet.width);
    float beltClearance = clampf(planet.width * 0.12f, 20.0f, 48.0f);
    float baseRadius = cornerRadius + atmosphereThickness + beltClearance;
    float baseRx = baseRadius;
    float baseRy = baseRadius * 0.88f;

    for (int band = 0; band < 3; ++band) {
        float bandSpacing = clampf(planet.width * 0.055f, 12.0f, 24.0f);
        float wobble = std::sin(t * (0.22f + band * 0.07f)) * (6.0f + band * 2.5f);
        float rx = baseRx + band * bandSpacing + wobble;
        float ry = baseRy + band * (bandSpacing * 0.72f) + wobble * 0.5f;
        Color bandColor = band == 0 ? Fade(Color{89, 210, 255, 255}, 0.11f) : (band == 1 ? Fade(Color{130, 125, 255, 255}, 0.07f) : Fade(Color{255, 212, 82, 255}, 0.06f));

        for (int seg = 0; seg < 90; ++seg) {
            float a0 = (static_cast<float>(seg) / 90.0f) * 2.0f * PI;
            float a1 = (static_cast<float>(seg + 1) / 90.0f) * 2.0f * PI;
            Vector2 p0{center.x + std::cos(a0) * rx, center.y + std::sin(a0) * ry};
            Vector2 p1{center.x + std::cos(a1) * rx, center.y + std::sin(a1) * ry};
            DrawLineEx(p0, p1, 1.2f, bandColor);
        }
    }

    for (int rock = 0; rock < 42; ++rock) {
        float lane = 0.98f + (rock % 7) * 0.08f;
        float angle = t * (0.08f + (rock % 5) * 0.01f) + rock * 0.41f;
        float rx = baseRx * lane;
        float ry = baseRy * (0.9f + (rock % 5) * 0.05f);
        Vector2 p{center.x + std::cos(angle) * rx, center.y + std::sin(angle) * ry};
        float radius = 1.5f + (rock % 3);
        Color dust = rock % 4 == 0 ? Fade(Color{255, 213, 84, 255}, 0.48f) : Fade(RAYWHITE, 0.28f);
        DrawCircleV(p, radius * 2.4f, Fade(dust, 0.08f));
        DrawCircleV(p, radius, dust);
    }
}

void drawPlanetAtmosphere() {
    Rectangle planet = activePlanetRect();
    Vector2 center = activePlanetCenter();
    float cornerRadius = activePlanetCornerRadius();
    float atmosphereThickness = atmosphereThicknessForPlanet(planet.width);
    float roundedOuterRadius = cornerRadius + atmosphereThickness;
    float shellSpacing = atmosphereThickness / 9.0f;
    float sizeT = clampf((planet.width - 280.0f) / 220.0f, 0.0f, 1.0f);
    float shellAlphaScale = 0.45f + sizeT * 0.4f;

    for (int shell = 0; shell < 10; ++shell) {
        float expand = shellSpacing * (shell + 1);
        float alpha = 0.12f * shellAlphaScale * (1.0f - static_cast<float>(shell) / 10.0f);
        float roundness = 0.035f + 0.02f * shell;
        Color shellColor = lerpColor(Color{114, 186, 255, 255}, Color{255, 188, 244, 255}, static_cast<float>(shell) / 9.0f);
        DrawRectangleRounded(
            Rectangle{
                planet.x - expand,
                planet.y - expand,
                planet.width + expand * 2.0f,
                planet.height + expand * 2.0f
            },
            roundness,
            12,
            Fade(shellColor, alpha)
        );
    }

    for (int layer = 0; layer < 24; ++layer) {
        float t = static_cast<float>(layer) / 23.0f;
        float radius = cornerRadius + atmosphereThickness * (0.18f + t * 0.92f);
        float alpha = 0.11f * shellAlphaScale * (1.0f - t * 0.72f);
        Color glow = lerpColor(Color{116, 192, 255, 255}, Color{255, 212, 247, 255}, t);
        DrawCircleGradient(
            static_cast<int>(center.x),
            static_cast<int>(center.y),
            radius,
            Fade(glow, alpha),
            Fade(BLANK, 0.0f)
        );
    }

    DrawCircleGradient(
        static_cast<int>(center.x),
        static_cast<int>(center.y),
        roundedOuterRadius,
        Fade(Color{114, 228, 255, 255}, 0.10f * shellAlphaScale),
        Fade(BLANK, 0.0f)
    );

    DrawCircleGradient(
        static_cast<int>(center.x),
        static_cast<int>(center.y),
        cornerRadius + atmosphereThickness * 0.45f,
        Fade(Color{255, 240, 186, 255}, 0.09f * shellAlphaScale),
        Fade(BLANK, 0.0f)
    );

    DrawCircleGradient(
        static_cast<int>(center.x),
        static_cast<int>(center.y),
        cornerRadius + atmosphereThickness * 0.72f,
        Fade(Color{255, 164, 240, 255}, 0.05f * shellAlphaScale),
        Fade(BLANK, 0.0f)
    );
}
}

Game::Game() {
    reset();
}

void Game::reset() {
    world_.generate();
    gPlanetMin = world_.planetMin();
    gPlanetSize = world_.planetSize();
    miningBlocks_.clear();
    missiles_.clear();
    explosions_.clear();

    players_[0] = Player{};
    players_[0].id = 0;
    players_[0].name = "Player 1";
    players_[0].position = {world_.planetMin() - 1, World::kSizeY / 2 + 2, world_.sliceZ()};
    players_[0].facing = {1, 0, 0};

    players_[1] = Player{};
    players_[1].id = 1;
    players_[1].name = "Player 2";
    players_[1].position = {world_.planetMax() + 1, World::kSizeY / 2 - 2, world_.sliceZ()};
    players_[1].facing = {-1, 0, 0};

    turnSystem_ = TurnSystem{};
    turnSystem_.turnNumber = 1;
    turnSystem_.timeRemaining = turnSystem_.turnDuration;
    wind_ = {};
    showHelp_ = true;
    gameOver_ = false;
    winnerId_ = -1;
}

TurnPhase Game::currentPhase() const {
    return phaseForTurn(turnSystem_.turnNumber);
}

void Game::update(float dt) {
    if (IsKeyPressed(KEY_TAB)) {
        showHelp_ = !showHelp_;
    }

    if (gameOver_) {
        if (IsKeyPressed(KEY_ENTER)) {
            reset();
        }
        updateExplosions(dt);
        return;
    }

    updateTurnSystem(dt);

    updatePlayer(dt, players_[0], KEY_A, KEY_D, KEY_W, KEY_S, KEY_Q, KEY_E, KEY_F, KEY_R, KEY_T, KEY_G, KEY_Y, KEY_H);
    updatePlayer(dt, players_[1], KEY_LEFT, KEY_RIGHT, KEY_UP, KEY_DOWN, KEY_COMMA, KEY_PERIOD, KEY_O, KEY_P, KEY_I, KEY_K, KEY_U, KEY_J);

    updateMissiles(dt);
    updateExplosions(dt);

    for (Player& player : players_) {
        player.damageFlash = std::max(0.0f, player.damageFlash - dt * 2.4f);
    }

    for (const Player& player : players_) {
        if (player.health <= 0) {
            winnerId_ = 1 - player.id;
            gameOver_ = true;
        }
    }
}

void Game::updateTurnSystem(float dt) {
    turnSystem_.timeRemaining -= dt;
    if (turnSystem_.timeRemaining > 0.0f) {
        return;
    }

    turnSystem_.turnNumber++;
    turnSystem_.timeRemaining = turnSystem_.turnDuration;

    // Wind shifts at the start of each attack phase so bazooka-like shots feel fresh each round.
    if (currentPhase() == TurnPhase::AttackDefend) {
        randomizeWind();
    } else {
        wind_ = {};
    }

    for (Player& player : players_) {
        player.mining.active = false;
    }
}

void Game::randomizeWind() {
    static std::mt19937 rng(424242);
    std::uniform_real_distribution<float> dirDist(-1.0f, 1.0f);
    std::uniform_real_distribution<float> strengthDist(1.5f, 8.5f);

    wind_.direction = normalize(Vec3{dirDist(rng), dirDist(rng) * 0.2f, 0.0f});
    wind_.strength = strengthDist(rng);
}

std::vector<ToolType> Game::availableToolsForPhase(TurnPhase phase) const {
    if (phase == TurnPhase::MineBuild) {
        return {ToolType::Mine, ToolType::Wall, ToolType::Armour, ToolType::Silo, ToolType::Fire};
    }
    return {ToolType::Wall, ToolType::Armour, ToolType::Fire};
}

void Game::cycleTool(Player& player, int direction) {
    std::vector<ToolType> tools = availableToolsForPhase(currentPhase());
    int currentIndex = 0;
    for (int i = 0; i < static_cast<int>(tools.size()); ++i) {
        if (tools[i] == player.tool) {
            currentIndex = i;
            break;
        }
    }
    currentIndex = (currentIndex + direction + static_cast<int>(tools.size())) % static_cast<int>(tools.size());
    player.tool = tools[currentIndex];
}

void Game::updatePlayer(float dt, Player& player, int leftKey, int rightKey, int upKey, int downKey,
                        int prevToolKey, int nextToolKey, int useKey, int placeKey,
                        int angleUpKey, int angleDownKey, int powerUpKey, int powerDownKey) {
    player.moveCooldown -= dt;

    if (IsKeyPressed(prevToolKey)) {
        cycleTool(player, -1);
    }
    if (IsKeyPressed(nextToolKey)) {
        cycleTool(player, 1);
    }

    player.aimAngleDeg = clampf(
        player.aimAngleDeg + (IsKeyDown(angleUpKey) ? 50.0f * dt : 0.0f) - (IsKeyDown(angleDownKey) ? 50.0f * dt : 0.0f),
        5.0f,
        85.0f
    );
    player.launchPower = clampf(
        player.launchPower + (IsKeyDown(powerUpKey) ? 18.0f * dt : 0.0f) - (IsKeyDown(powerDownKey) ? 18.0f * dt : 0.0f),
        6.5f,
        14.0f
    );

    int dx = 0;
    int dy = 0;
    if (IsKeyDown(leftKey)) dx -= 1;
    if (IsKeyDown(rightKey)) dx += 1;
    if (IsKeyDown(upKey)) dy -= 1;
    if (IsKeyDown(downKey)) dy += 1;
    if (player.moveCooldown <= 0.0f && (dx != 0 || dy != 0)) {
        handleMovement(player, dx, dy);
        player.moveCooldown = kMoveDelay;
    }

    handleUse(player, IsKeyPressed(useKey), IsKeyDown(useKey), dt);

    if (IsKeyPressed(placeKey)) {
        handlePlace(player);
    }
}

void Game::handleMovement(Player& player, int dx, int dy) {
    Vec3i next = player.position + Vec3i{dx, dy, 0};
    if (canPlayerStand(player, next)) {
        player.position = next;
    }
    if (dx != 0 || dy != 0) {
        player.facing = {dx, dy, 0};
    }
}

bool Game::canPlayerStand(const Player& player, const Vec3i& cell) const {
    if (cell.y < world_.planetMin() || cell.y > world_.planetMax() || cell.z != world_.sliceZ()) {
        return false;
    }

    const int exteriorLaneX = player.id == 0 ? world_.planetMin() - 1 : world_.planetMax() + 1;
    const bool onExteriorLane = player.position.x == exteriorLaneX;
    Block cellBlock = world_.inBounds(cell) ? world_.get(cell) : Block{BlockType::Air, -1};

    if (onExteriorLane) {
        if (cell.x == exteriorLaneX) {
            return cellBlock.type == BlockType::Air;
        }

        if (!world_.inBounds(cell) || cellBlock.type != BlockType::Air) {
            return false;
        }

        return cell.x >= world_.planetMin() && cell.x <= world_.planetMax();
    }

    if (world_.inBounds(cell) && cellBlock.type == BlockType::Air) {
        return true;
    }

    return false;
}

Vec3i Game::frontCell(const Player& player) const {
    Vec3i facing = player.facing;
    if (facing == Vec3i{0, 0, 0}) {
        facing = drillDirectionForPlayer(player.id);
    }

    // When a player is walking on the exterior lane, their up/down actions still target their own face,
    // not empty space beyond the cube.
    if (player.id == 0 && player.position.x == world_.planetMin() - 1) {
        if (facing.x > 0) return {world_.planetMin(), player.position.y, world_.sliceZ()};
        if (facing.y != 0) return {world_.planetMin(), player.position.y + facing.y, world_.sliceZ()};
    }
    if (player.id == 1 && player.position.x == world_.planetMax() + 1) {
        if (facing.x < 0) return {world_.planetMax(), player.position.y, world_.sliceZ()};
        if (facing.y != 0) return {world_.planetMax(), player.position.y + facing.y, world_.sliceZ()};
    }

    return player.position + facing;
}

Vec3i Game::buildCellForPlayer(const Player& player) const {
    if (player.id == 0 && player.position.x == world_.planetMin() - 1) {
        return {world_.planetMin(), player.position.y, world_.sliceZ()};
    }
    if (player.id == 1 && player.position.x == world_.planetMax() + 1) {
        return {world_.planetMax(), player.position.y, world_.sliceZ()};
    }
    return player.position;
}

Vec3i Game::wallPlacementCell(const Player& player) const {
    Vec3i facing = player.facing;
    if (facing == Vec3i{0, 0, 0}) {
        facing = drillDirectionForPlayer(player.id);
    }

    if (player.id == 0 && player.position.x == world_.planetMin() - 1) {
        if (facing.x > 0) {
            return {world_.planetMin(), player.position.y, world_.sliceZ()};
        }
        return player.position + facing;
    }
    if (player.id == 1 && player.position.x == world_.planetMax() + 1) {
        if (facing.x < 0) {
            return {world_.planetMax(), player.position.y, world_.sliceZ()};
        }
        return player.position + facing;
    }

    return player.position + facing;
}

float Game::mineTimeForBlock(BlockType type) const {
    switch (type) {
        case BlockType::Stone:
            return 24.0f;
        case BlockType::Dirt:
            return 4.8f;
        case BlockType::FuelOre:
        case BlockType::MetalOre:
            return 6.7f;
        case BlockType::PlayerWall:
            return 2.8f;
        case BlockType::ArmourBlock:
        case BlockType::MiningBlock:
        case BlockType::MissileSilo:
            return 4.2f;
        case BlockType::Air:
            return 0.0f;
    }
    return 5.0f;
}

bool Game::collectBlockResource(Player& player, BlockType type) {
    if (type == BlockType::FuelOre) {
        player.fuel += kFuelOreYield;
        return true;
    }
    if (type == BlockType::MetalOre) {
        player.metal += kMetalOreYield;
        return true;
    }
    return false;
}

void Game::applyDamage(Player& player, int damage, int sourceOwnerId) {
    if (damage <= 0 || player.health <= 0) {
        return;
    }

    player.health = std::max(0, player.health - damage);
    player.damageFlash = 1.0f;

    // Friendly fire still hurts, but enemy hits get a slightly punchier response.
    if (sourceOwnerId != player.id) {
        player.damageFlash = 1.2f;
    }
}

bool Game::spend(Player& player, int fuelCost, int metalCost) {
    if (player.fuel < fuelCost || player.metal < metalCost) {
        return false;
    }
    player.fuel -= fuelCost;
    player.metal -= metalCost;
    return true;
}

void Game::handleUse(Player& player, bool pressed, bool held, float dt) {
    if (player.tool == ToolType::Fire && currentPhase() == TurnPhase::AttackDefend) {
        if (pressed) {
            fireMissile(player);
        }
        return;
    }

    if (player.tool != ToolType::Mine || currentPhase() != TurnPhase::MineBuild) {
        player.mining.active = false;
        return;
    }

    Vec3i target = frontCell(player);
    Block block = world_.get(target);
    if (!held || !world_.inBounds(target) || block.type == BlockType::Air) {
        player.mining.active = false;
        return;
    }

    // Manual mining is the only mining mode in this prototype, so the HUD and target outline focus on it.
    if (!player.mining.active || player.mining.target != target) {
        player.mining.active = true;
        player.mining.target = target;
        player.mining.progress = 0.0f;
    }

    player.mining.progress += dt;
    if (player.mining.progress >= mineTimeForBlock(block.type)) {
        collectBlockResource(player, block.type);
        world_.set(target, BlockType::Air);
        player.mining.active = false;
    }
}

void Game::handlePlace(Player& player) {
    TurnPhase phase = currentPhase();
    switch (player.tool) {
        case ToolType::Wall: {
            Vec3i target = wallPlacementCell(player);
            const bool onExteriorLane =
                (player.id == 0 && player.position.x == world_.planetMin() - 1) ||
                (player.id == 1 && player.position.x == world_.planetMax() + 1);
            const bool isOwnFaceBlock =
                (player.id == 0 && target.x == world_.planetMin()) ||
                (player.id == 1 && target.x == world_.planetMax());
            const bool canAttachToFace = onExteriorLane && isOwnFaceBlock;

            if (!world_.inBounds(target)) {
                return;
            }

            if (!world_.isAir(target) && !canAttachToFace) {
                return;
            }

            if (spend(player, 0, kWallCost)) {
                world_.set(target, BlockType::PlayerWall, player.id);
            }
            break;
        }
        case ToolType::Armour: {
            Vec3i target = wallPlacementCell(player);
            Block targetBlock = world_.get(target);
            if (!world_.inBounds(target) || targetBlock.type != BlockType::PlayerWall || targetBlock.ownerId != player.id) {
                return;
            }
            if (spend(player, 0, kArmourCost)) {
                world_.set(target, BlockType::ArmourBlock, player.id);
            }
            break;
        }
        case ToolType::Silo: {
            Vec3i target = buildCellForPlayer(player);
            if (!world_.inBounds(target) || !world_.isAir(target)) {
                return;
            }
            if (phase == TurnPhase::MineBuild && spend(player, 0, kSiloCost)) {
                world_.set(target, BlockType::MissileSilo, player.id);
            }
            break;
        }
        default:
            break;
    }
}

void Game::updateMiningBlocks(float dt) {
    (void)dt;
}

std::optional<Vec3i> Game::findNearbyOwnedSilo(const Player& player) const {
    constexpr std::array<Vec3i, 5> offsets = {
        Vec3i{0, 0, 0}, Vec3i{1, 0, 0}, Vec3i{-1, 0, 0}, Vec3i{0, 1, 0}, Vec3i{0, -1, 0}
    };

    Vec3i starterSilo = initialSiloPosition(player.id);
    for (const Vec3i& offset : offsets) {
        if (player.position + offset == starterSilo) {
            return starterSilo;
        }
    }

    for (const Vec3i& offset : offsets) {
        Vec3i p = player.position + offset;
        Block b = world_.get(p);
        if (b.type == BlockType::MissileSilo && b.ownerId == player.id) {
            return p;
        }
    }
    return std::nullopt;
}

void Game::fireMissile(Player& player) {
    if (!spend(player, kMissileFuelCost, kMissileMetalCost)) {
        return;
    }

    std::optional<Vec3i> silo = findNearbyOwnedSilo(player);
    if (!silo.has_value()) {
        player.fuel += kMissileFuelCost;
        player.metal += kMissileMetalCost;
        return;
    }

    Missile missile;
    missile.ownerId = player.id;
    missile.state = MissileState::Flying;
    // Start the missile outside the cube face so launches actually clear the planet before gravity bends them back.
    missile.position = toVec3(*silo) + Vec3{0.5f, 0.5f, 0.0f} + outwardNormalForPlayer(player.id) * kLaunchClearance;
    missile.launchDirection = vecFromAngle(player.id, player.aimAngleDeg);
    missile.velocity = missile.launchDirection * player.launchPower;
    missile.blastRadius = kExplosionRadius;
    missiles_.push_back(missile);
}

void Game::updateMissilePhysics(Missile& missile, const Wind& wind, float dt) const {
    Vec3 toCenter = world_.center() - missile.position;
    float distanceToCenter = std::max(1.0f, length(toCenter));
    Vec3 gravity = normalize(toCenter) * (kGravityMu / (distanceToCenter * distanceToCenter));
    Vec3 windForce = wind.direction * (wind.strength * kMissileWindFactor);
    Vec3 dragForce = missile.velocity * -kDrag;
    Vec3 launchBoost = {};

    // A brief rocket burn helps the missile establish its outbound arc before it becomes a gravity-driven shot.
    if (missile.age < kLaunchBoostDuration) {
        float boostWeight = 1.0f - (missile.age / kLaunchBoostDuration);
        launchBoost = missile.launchDirection * (kLaunchBoostStrength * boostWeight);
    }

    // Inverse-square gravity does the real wrapping work here; low drag leaves enough tangential speed to reach the far face.
    missile.velocity += (gravity + windForce + dragForce + launchBoost) * dt;
    missile.position += missile.velocity * dt;
    missile.age += dt;
}

bool Game::missileHasCollided(const Missile& missile) const {
    if (missile.age < 0.35f) {
        return false;
    }
    Vec3i cell{
        static_cast<int>(std::floor(missile.position.x)),
        static_cast<int>(std::floor(missile.position.y)),
        world_.sliceZ()
    };
    return world_.inBounds(cell) && world_.isSolid(cell);
}

void Game::updateMissiles(float dt) {
    for (Missile& missile : missiles_) {
        if (missile.state != MissileState::Flying) {
            continue;
        }

        float stepDt = dt / static_cast<float>(kMissileSubsteps);
        for (int step = 0; step < kMissileSubsteps; ++step) {
            updateMissilePhysics(missile, wind_, stepDt);

            if (missileHasCollided(missile)) {
                missile.state = MissileState::Exploded;
                explodeAt(missile.position, missile.blastRadius, missile.ownerId);
                break;
            }
        }
    }
}

bool Game::wallResistsExplosion(const Vec3i& cell) const {
    Block block = world_.get(cell);
    if (block.type != BlockType::PlayerWall && block.type != BlockType::ArmourBlock) {
        return false;
    }

    static std::mt19937 rng(8675309);
    std::uniform_real_distribution<float> roll(0.0f, 1.0f);
    float resistChance = block.type == BlockType::ArmourBlock ? 0.85f : 0.5f;
    return roll(rng) < resistChance;
}

float Game::wallDamageReductionBetween(const Vec3& from, const Vec3& to) const {
    Vec3 delta = to - from;
    float dist = length(delta);
    if (dist <= 0.001f) {
        return 1.0f;
    }

    Vec3 dir = delta / dist;
    const int samples = std::max(4, static_cast<int>(std::ceil(dist * 8.0f)));
    for (int i = 1; i < samples; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(samples);
        Vec3 probe = from + dir * (dist * t);
        Vec3i cell{
            static_cast<int>(std::floor(probe.x)),
            static_cast<int>(std::floor(probe.y)),
            world_.sliceZ()
        };
        Block block = world_.get(cell);
        if (block.type == BlockType::ArmourBlock) {
            return 0.25f;
        }
        if (block.type == BlockType::PlayerWall) {
            return 0.5f;
        }
    }

    return 1.0f;
}

void Game::explodeAt(const Vec3& position, float radius, int ownerId) {
    Vec3 outward = normalize(position - world_.center());
    if (length(outward) < 0.001f) {
        outward = {0.0f, -1.0f, 0.0f};
    }
    explosions_.push_back({position, radius, 0.55f, outward});

    for (int y = 0; y < world_.sizeY(); ++y) {
        for (int x = 0; x < world_.sizeX(); ++x) {
            Vec3i cell{x, y, world_.sliceZ()};
            Vec3 cellCenter = toVec3(cell) + Vec3{0.5f, 0.5f, 0.0f};
            if (distance(cellCenter, position) <= radius) {
                Block block = world_.get(cell);
                if ((block.type == BlockType::PlayerWall || block.type == BlockType::ArmourBlock) && wallResistsExplosion(cell)) {
                    continue;
                }
                world_.set(cell, BlockType::Air);
            }
        }
    }

    for (Player& player : players_) {
        Vec3 playerPos = toVec3(player.position) + Vec3{0.5f, 0.5f, 0.0f};
        float d = distance(playerPos, position);
        if (d <= radius + 0.5f) {
            int damage = static_cast<int>(std::round((1.0f - (d / (radius + 0.5f))) * 65.0f));
            if (player.id != ownerId) {
                damage += 10;
            }
            damage = static_cast<int>(std::round(static_cast<float>(std::max(8, damage)) * wallDamageReductionBetween(position, playerPos)));
            applyDamage(player, damage, ownerId);
        }
    }
}

void Game::updateExplosions(float dt) {
    for (Explosion& e : explosions_) {
        e.ttl -= dt;
    }
    explosions_.erase(
        std::remove_if(explosions_.begin(), explosions_.end(), [](const Explosion& e) { return e.ttl <= 0.0f; }),
        explosions_.end()
    );
}

Vec3i Game::initialSiloPosition(int playerId) const {
    if (playerId == 0) {
        return {world_.planetMin() - 1, World::kSizeY / 2, world_.sliceZ()};
    }
    return {world_.planetMax() + 1, World::kSizeY / 2, world_.sliceZ()};
}

std::vector<Vec3> Game::predictArcPartial(Missile missile, const Wind& wind, float dt, int totalSamples) const {
    std::vector<Vec3> points;
    int visibleSamples = std::max(1, static_cast<int>((totalSamples / 3.0f) * 0.7f));

    // Show only the opening part of the flight so aiming still has uncertainty.
    for (int i = 0; i < visibleSamples; ++i) {
        updateMissilePhysics(missile, wind, dt);
        points.push_back(missile.position);
        if (missileHasCollided(missile)) {
            break;
        }
    }

    return points;
}

Missile Game::makePreviewMissile(const Player& player) const {
    Missile preview;
    std::optional<Vec3i> silo = findNearbyOwnedSilo(player);
    if (!silo.has_value()) {
        return preview;
    }

    preview.ownerId = player.id;
    preview.state = MissileState::Flying;
    preview.position = toVec3(*silo) + Vec3{0.5f, 0.5f, 0.0f};
    preview.position += outwardNormalForPlayer(player.id) * kLaunchClearance;
    preview.launchDirection = vecFromAngle(player.id, player.aimAngleDeg);
    preview.velocity = preview.launchDirection * player.launchPower;
    preview.blastRadius = kExplosionRadius;
    return preview;
}

std::string Game::predictedImpactText(const Player& player) const {
    if (currentPhase() != TurnPhase::AttackDefend || player.tool != ToolType::Fire) {
        return "impact: n/a";
    }

    Missile probe = makePreviewMissile(player);
    if (probe.state != MissileState::Flying) {
        return "impact: no silo";
    }

    for (int i = 0; i < 420; ++i) {
        for (int step = 0; step < kMissileSubsteps; ++step) {
            updateMissilePhysics(probe, wind_, 0.035f / static_cast<float>(kMissileSubsteps));
            if (missileHasCollided(probe)) {
                Vec3i cell{
                    static_cast<int>(std::floor(probe.position.x)),
                    static_cast<int>(std::floor(probe.position.y)),
                    world_.sliceZ()
                };

                int leftBand = world_.planetMin() + world_.planetSize() / 3;
                int rightBand = world_.planetMax() - world_.planetSize() / 3;
                if (cell.x <= leftBand) {
                    return "impact: left side";
                }
                if (cell.x >= rightBand) {
                    return "impact: right side";
                }
                return "impact: core/middle";
            }
        }
    }

    return "impact: no hit";
}

std::string Game::contextualCostText(const Player& player) const {
    switch (player.tool) {
        case ToolType::Mine:
            return currentPhase() == TurnPhase::MineBuild
                ? "cost: manual mining only"
                : "cost: unavailable in attack";
        case ToolType::Wall:
            return "cost: 1 metal";
        case ToolType::Armour:
            return "cost: 4 metal (upgrade wall)";
        case ToolType::Silo:
            return currentPhase() == TurnPhase::MineBuild
                ? "cost: 30 metal"
                : "cost: unavailable in attack";
        case ToolType::Fire:
            return currentPhase() == TurnPhase::AttackDefend
                ? "cost: 15 fuel, 20 metal"
                : "cost: unavailable in mining";
        case ToolType::Drill:
            return "cost: unused";
    }
    return "cost: n/a";
}

void Game::draw() const {
    drawWorldSlice();
    drawExplosions();
    drawMissiles();
    drawPlayers();
    drawPreviewArc(players_[0]);
    drawPreviewArc(players_[1]);
    drawHud();
    if (showHelp_) {
        drawHelp();
    }
}

void Game::drawWorldSlice() const {
    drawNebulaBackdrop();

    drawPlanetAtmosphere();
    drawOrbitalBeltField();

    float cell = worldCellSize();
    int drawMinX = std::max(0, world_.planetMin() - 2);
    int drawMaxX = std::min(world_.sizeX() - 1, world_.planetMax() + 2);
    int drawMinY = world_.planetMin();
    int drawMaxY = world_.planetMax();

    for (int y = drawMinY; y <= drawMaxY; ++y) {
        for (int x = drawMinX; x <= drawMaxX; ++x) {
            Vec3i p{x, y, world_.sliceZ()};
            Vector2 topLeft = cellTopLeft(p);
            Rectangle cellRect{topLeft.x, topLeft.y, std::ceil(cell) - 1.0f, std::ceil(cell) - 1.0f};
            bool isExteriorBand = x < world_.planetMin() || x > world_.planetMax();
            Block block = world_.inBounds(p) ? world_.get(p) : Block{BlockType::Air, -1};

            if (isExteriorBand && block.type == BlockType::Air) {
                continue;
            }

            Color c = colorForBlock(block);
            if (block.type == BlockType::Air) {
                float vignette = (std::sin((x + y) * 0.55f) + 1.0f) * 0.5f;
                DrawRectangleRec(cellRect, lerpColor(Color{13, 20, 33, 255}, Color{18, 28, 44, 255}, vignette));
                DrawRectangleRec(
                    Rectangle{cellRect.x + cellRect.width * 0.15f, cellRect.y + cellRect.height * 0.15f, cellRect.width * 0.12f, cellRect.height * 0.12f},
                    Fade(SKYBLUE, 0.035f)
                );
                continue;
            }

            Color topGlow = scaleColor(c, 1.16f);
            Color baseShade = scaleColor(c, 0.80f);
            DrawRectangleGradientV(
                static_cast<int>(cellRect.x),
                static_cast<int>(cellRect.y),
                static_cast<int>(cellRect.width),
                static_cast<int>(cellRect.height),
                topGlow,
                baseShade
            );

            if (block.type == BlockType::FuelOre || block.type == BlockType::MetalOre) {
                Color aura = block.type == BlockType::FuelOre ? Fade(Color{255, 214, 82, 255}, 0.24f) : Fade(Color{92, 232, 255, 255}, 0.26f);
                DrawRectangleRec(
                    Rectangle{cellRect.x - 1.5f, cellRect.y - 1.5f, cellRect.width + 3.0f, cellRect.height + 3.0f},
                    aura
                );
                DrawCircleV(
                    {cellRect.x + cellRect.width * 0.5f, cellRect.y + cellRect.height * 0.5f},
                    cellRect.width * 0.18f,
                    block.type == BlockType::FuelOre ? Fade(RAYWHITE, 0.75f) : Fade(WHITE, 0.72f)
                );
                DrawCircleV(
                    {cellRect.x + cellRect.width * 0.5f, cellRect.y + cellRect.height * 0.5f},
                    cellRect.width * 0.34f,
                    block.type == BlockType::FuelOre ? Fade(GOLD, 0.20f) : Fade(SKYBLUE, 0.22f)
                );
            }

            if (block.type == BlockType::PlayerWall || block.type == BlockType::ArmourBlock || block.type == BlockType::MissileSilo) {
                DrawRectangleRec(
                    Rectangle{cellRect.x + 1.0f, cellRect.y + 1.0f, cellRect.width - 2.0f, cellRect.height - 2.0f},
                    Fade(WHITE, 0.10f)
                );
                DrawRectangleRec(
                    Rectangle{cellRect.x + 2.0f, cellRect.y + 2.0f, cellRect.width - 4.0f, cellRect.height * 0.22f},
                    Fade(WHITE, 0.14f)
                );
                DrawRectangleRec(
                    Rectangle{cellRect.x - 1.0f, cellRect.y - 1.0f, cellRect.width + 2.0f, cellRect.height + 2.0f},
                    Fade(c, 0.10f)
                );
                if (block.type == BlockType::ArmourBlock) {
                    DrawRectangleLinesEx(
                        Rectangle{cellRect.x + 3.0f, cellRect.y + 3.0f, cellRect.width - 6.0f, cellRect.height - 6.0f},
                        2.0f,
                        Fade(Color{255, 244, 180, 255}, 0.55f)
                    );
                    DrawLineEx(
                        {cellRect.x + 4.0f, cellRect.y + cellRect.height * 0.5f},
                        {cellRect.x + cellRect.width - 4.0f, cellRect.y + cellRect.height * 0.5f},
                        2.0f,
                        Fade(Color{255, 244, 180, 255}, 0.5f)
                    );
                }
            }

            DrawRectangle(
                static_cast<int>(cellRect.x),
                static_cast<int>(cellRect.y),
                static_cast<int>(cellRect.width),
                2,
                Fade(WHITE, 0.18f)
            );
            DrawRectangleLinesEx(cellRect, 1.0f, Fade(BLACK, 0.16f));
            DrawRectangleRec(
                Rectangle{cellRect.x + 1.0f, cellRect.y + cellRect.height - 3.0f, cellRect.width - 2.0f, 2.0f},
                Fade(BLACK, 0.10f)
            );
        }
    }

    for (int playerId = 0; playerId < 2; ++playerId) {
        Vec3i silo = initialSiloPosition(playerId);
        Vector2 topLeft = cellTopLeft(silo);
        Rectangle cellRect{topLeft.x, topLeft.y, std::ceil(cell) - 1.0f, std::ceil(cell) - 1.0f};
        Color c = colorForBlock({BlockType::MissileSilo, playerId});
        DrawRectangleGradientV(
            static_cast<int>(cellRect.x),
            static_cast<int>(cellRect.y),
            static_cast<int>(cellRect.width),
            static_cast<int>(cellRect.height),
            scaleColor(c, 1.16f),
            scaleColor(c, 0.82f)
        );
        DrawRectangleLinesEx(cellRect, 1.3f, Fade(WHITE, 0.18f));
        DrawCircleV(
            {cellRect.x + cellRect.width * 0.5f, cellRect.y + cellRect.height * 0.34f},
            cellRect.width * 0.13f,
            Fade(WHITE, 0.6f)
        );
        DrawCircleV(
            {cellRect.x + cellRect.width * 0.5f, cellRect.y + cellRect.height * 0.55f},
            cellRect.width * 0.28f,
            Fade(c, 0.18f)
        );
    }
}

void Game::drawPlayers() const {
    float cell = worldCellSize();
    for (const Player& player : players_) {
        if (currentPhase() == TurnPhase::MineBuild && player.tool == ToolType::Mine) {
            Vec3i target = frontCell(player);
            Block targetBlock = world_.get(target);
            if (world_.inBounds(target) && targetBlock.type != BlockType::Air) {
                Vector2 targetTopLeft = cellTopLeft(target);
                Color focus = player.id == 0 ? Fade(SKYBLUE, 0.85f) : Fade(PINK, 0.85f);
                Vector2 center{targetTopLeft.x + cell * 0.5f, targetTopLeft.y + cell * 0.5f};
                DrawRectangleRec(
                    Rectangle{targetTopLeft.x + 1.0f, targetTopLeft.y + 1.0f, cell - 2.0f, cell - 2.0f},
                    Fade(focus, 0.08f)
                );
                DrawRectangleLinesEx(
                    Rectangle{targetTopLeft.x - 1.0f, targetTopLeft.y - 1.0f, cell + 2.0f, cell + 2.0f},
                    1.8f,
                    focus
                );
                DrawLineEx(
                    {targetTopLeft.x + 4.0f, targetTopLeft.y + 4.0f},
                    {targetTopLeft.x + cell - 4.0f, targetTopLeft.y + cell - 4.0f},
                    2.0f,
                    Fade(focus, 0.9f)
                );
                DrawLineEx(
                    {targetTopLeft.x + cell - 4.0f, targetTopLeft.y + 4.0f},
                    {targetTopLeft.x + 4.0f, targetTopLeft.y + cell - 4.0f},
                    2.0f,
                    Fade(focus, 0.9f)
                );
                DrawCircleLines(
                    static_cast<int>(center.x),
                    static_cast<int>(center.y),
                    cell * 0.18f,
                    Fade(WHITE, 0.55f)
                );
            }
        }

        if (player.tool == ToolType::Wall) {
            Vec3i target = wallPlacementCell(player);
            const bool onExteriorLane =
                (player.id == 0 && player.position.x == world_.planetMin() - 1) ||
                (player.id == 1 && player.position.x == world_.planetMax() + 1);
            const bool isOwnFaceBlock =
                (player.id == 0 && target.x == world_.planetMin()) ||
                (player.id == 1 && target.x == world_.planetMax());
            const bool canAttachToFace = onExteriorLane && isOwnFaceBlock;
            if (world_.inBounds(target) && (world_.isAir(target) || canAttachToFace)) {
                Vector2 targetTopLeft = cellTopLeft(target);
                Color focus = player.id == 0 ? Fade(SKYBLUE, 0.75f) : Fade(PINK, 0.75f);
                Rectangle ghostRect{targetTopLeft.x + 3.0f, targetTopLeft.y + 3.0f, cell - 6.0f, cell - 6.0f};
                DrawRectangleLinesEx(
                    Rectangle{targetTopLeft.x - 1.0f, targetTopLeft.y - 1.0f, cell + 2.0f, cell + 2.0f},
                    2.0f,
                    focus
                );
                DrawRectangleRec(
                    ghostRect,
                    Fade(focus, 0.20f)
                );
                DrawRectangleLinesEx(ghostRect, 1.0f, Fade(WHITE, 0.45f));
                DrawRectangleRec(
                    Rectangle{ghostRect.x, ghostRect.y, ghostRect.width, 3.0f},
                    Fade(WHITE, 0.20f)
                );
                DrawCircleV(
                    {targetTopLeft.x + cell * 0.5f, targetTopLeft.y + cell * 0.5f},
                    cell * 0.36f,
                    Fade(focus, 0.10f)
                );
            }
        }

        if (player.tool == ToolType::Armour) {
            Vec3i target = wallPlacementCell(player);
            Block targetBlock = world_.get(target);
            if (world_.inBounds(target) && targetBlock.type == BlockType::PlayerWall && targetBlock.ownerId == player.id) {
                Vector2 targetTopLeft = cellTopLeft(target);
                Color focus = player.id == 0 ? Fade(Color{255, 239, 150, 255}, 0.88f) : Fade(Color{255, 214, 120, 255}, 0.88f);
                DrawRectangleLinesEx(
                    Rectangle{targetTopLeft.x - 1.0f, targetTopLeft.y - 1.0f, cell + 2.0f, cell + 2.0f},
                    2.0f,
                    focus
                );
                DrawCircleLines(
                    static_cast<int>(targetTopLeft.x + cell * 0.5f),
                    static_cast<int>(targetTopLeft.y + cell * 0.5f),
                    cell * 0.28f,
                    focus
                );
            }
        }

        Vector2 topLeft = cellTopLeft(player.position);
        Vector2 p{topLeft.x + cell * 0.5f, topLeft.y + cell * 0.5f};
        Color tint = player.id == 0 ? Color{108, 216, 255, 255} : Color{255, 142, 142, 255};
        tint = lerpColor(tint, WHITE, clampf(player.damageFlash * 0.55f, 0.0f, 0.55f));
        float radius = std::max(5.0f, cell * 0.33f);
        float pulse = 0.88f + 0.12f * std::sin(GetTime() * 4.6f + player.id * 1.4f);
        DrawCircleV(p, radius * 2.6f, Fade(tint, 0.10f * pulse));
        DrawCircleV(p, radius * 1.95f, Fade(tint, 0.18f));
        DrawCircleV(p, radius * 3.2f, Fade(tint, 0.05f * pulse));
        DrawCircleV(p, radius, tint);
        DrawCircleV({p.x, p.y + radius * 0.1f}, radius * 0.72f, scaleColor(tint, 0.92f));
        DrawCircleLines(static_cast<int>(p.x), static_cast<int>(p.y), radius, WHITE);
        DrawCircleV({p.x - radius * 0.25f, p.y - radius * 0.3f}, radius * 0.24f, Fade(WHITE, 0.7f));
        DrawCircleV({p.x + radius * 0.16f, p.y + radius * 0.14f}, radius * 0.12f, Fade(BLACK, 0.18f));
        if (player.damageFlash > 0.0f) {
            DrawCircleLines(static_cast<int>(p.x), static_cast<int>(p.y), radius * (1.7f + player.damageFlash * 0.7f), Fade(WHITE, 0.75f * player.damageFlash));
        }
    }
}

void Game::drawMissiles() const {
    for (const Missile& missile : missiles_) {
        if (missile.state != MissileState::Flying) {
            continue;
        }
        Vector2 p = worldPointToScreen(missile.position);
        Color tint = missile.ownerId == 0 ? SKYBLUE : PINK;
        for (int trail = 4; trail >= 1; --trail) {
            Vector2 tail{
                p.x - missile.velocity.x * 0.22f * trail,
                p.y - missile.velocity.y * 0.22f * trail
            };
            DrawCircleV(tail, 2.0f + trail, Fade(tint, 0.06f * trail));
        }
        DrawCircleV(p, 24.0f, Fade(tint, 0.08f));
        DrawCircleV(p, 16.0f, Fade(tint, 0.14f));
        DrawCircleV(p, 30.0f, Fade(tint, 0.04f));
        DrawCircleV(p, 6.5f, tint);
        DrawCircleV(p, 3.0f, WHITE);
        DrawCircleLines(static_cast<int>(p.x), static_cast<int>(p.y), 8.5f, Fade(WHITE, 0.42f));
    }
}

void Game::drawExplosions() const {
    for (const Explosion& e : explosions_) {
        Vector2 p = worldPointToScreen(e.position);
        float radiusPx = e.radius * worldCellSize();
        float life = clampf(1.0f - (e.ttl / 0.55f), 0.0f, 1.0f);
        float plumeRise = radiusPx * (2.8f + life * 6.0f);
        Vector2 dir{e.normal.x, e.normal.y};
        float dirLen = std::sqrt(dir.x * dir.x + dir.y * dir.y);
        if (dirLen < 0.001f) {
            dir = {0.0f, -1.0f};
            dirLen = 1.0f;
        }
        dir.x /= dirLen;
        dir.y /= dirLen;
        Vector2 perp{-dir.y, dir.x};

        Vector2 stemCenter{p.x + dir.x * plumeRise * 0.42f, p.y + dir.y * plumeRise * 0.42f};
        Vector2 capCenter{p.x + dir.x * plumeRise * 1.02f, p.y + dir.y * plumeRise * 1.02f};

        DrawCircleV(p, radiusPx * (0.78f + life * 0.22f), Fade(WHITE, 1.15f * e.ttl));
        DrawCircleV(p, radiusPx * (2.45f + life * 0.70f), Fade(Color{255, 148, 56, 255}, 0.52f * e.ttl));
        DrawCircleV(p, radiusPx * (3.55f + life * 1.00f), Fade(Color{255, 208, 120, 255}, 0.24f * e.ttl));
        DrawCircleV(p, radiusPx * (4.85f + life * 1.35f), Fade(Color{170, 220, 255, 255}, 0.14f * e.ttl));
        DrawCircleV(p, radiusPx * (6.10f + life * 1.55f), Fade(Color{255, 110, 216, 255}, 0.07f * e.ttl));
        DrawCircleLines(static_cast<int>(p.x), static_cast<int>(p.y), radiusPx * (3.10f + life * 0.40f), Fade(Color{255, 246, 188, 255}, 0.72f * e.ttl));

        Vector2 stemA{p.x + perp.x * radiusPx * 0.40f, p.y + perp.y * radiusPx * 0.40f};
        Vector2 stemB{p.x - perp.x * radiusPx * 0.40f, p.y - perp.y * radiusPx * 0.40f};
        Vector2 stemC{
            stemCenter.x - perp.x * radiusPx * 0.52f + dir.x * plumeRise * 0.44f,
            stemCenter.y - perp.y * radiusPx * 0.52f + dir.y * plumeRise * 0.44f
        };
        Vector2 stemD{
            stemCenter.x + perp.x * radiusPx * 0.52f + dir.x * plumeRise * 0.44f,
            stemCenter.y + perp.y * radiusPx * 0.52f + dir.y * plumeRise * 0.44f
        };
        DrawTriangle(stemA, stemB, stemC, Fade(Color{255, 176, 92, 255}, 0.36f * e.ttl));
        DrawTriangle(stemA, stemC, stemD, Fade(Color{255, 176, 92, 255}, 0.36f * e.ttl));
        DrawTriangle(stemA, stemB, stemD, Fade(Color{120, 210, 255, 255}, 0.13f * e.ttl));
        DrawCircleV(stemCenter, radiusPx * 0.52f, Fade(Color{255, 236, 182, 255}, 0.28f * e.ttl));

        DrawCircleV(capCenter, radiusPx * (1.72f + life * 0.40f), Fade(Color{255, 222, 154, 255}, 0.42f * e.ttl));
        DrawCircleV(
            {capCenter.x - perp.x * radiusPx * 1.18f - dir.x * 14.0f, capCenter.y - perp.y * radiusPx * 1.18f - dir.y * 14.0f},
            radiusPx * 1.18f,
            Fade(Color{255, 188, 108, 255}, 0.38f * e.ttl)
        );
        DrawCircleV(
            {capCenter.x + perp.x * radiusPx * 1.18f - dir.x * 14.0f, capCenter.y + perp.y * radiusPx * 1.18f - dir.y * 14.0f},
            radiusPx * 1.18f,
            Fade(Color{255, 188, 108, 255}, 0.38f * e.ttl)
        );
        DrawCircleV(
            {capCenter.x - dir.x * radiusPx * 0.12f, capCenter.y - dir.y * radiusPx * 0.12f},
            radiusPx * 0.92f,
            Fade(Color{255, 246, 204, 255}, 0.30f * e.ttl)
        );
        DrawCircleLines(static_cast<int>(capCenter.x), static_cast<int>(capCenter.y), radiusPx * (2.20f + life * 0.34f), Fade(WHITE, 0.74f * e.ttl));
        DrawCircleLines(static_cast<int>(p.x), static_cast<int>(p.y), radiusPx * (3.35f + life * 0.48f), Fade(ORANGE, 1.0f * e.ttl));
        DrawCircleLines(static_cast<int>(p.x), static_cast<int>(p.y), radiusPx * (4.85f + life * 0.70f), Fade(Color{255, 226, 168, 255}, 0.62f * e.ttl));
        DrawCircleLines(static_cast<int>(p.x), static_cast<int>(p.y), radiusPx * (6.00f + life * 0.95f), Fade(Color{170, 220, 255, 255}, 0.28f * e.ttl));
    }
}

void Game::drawPreviewArc(const Player& player) const {
    if (currentPhase() != TurnPhase::AttackDefend || player.tool != ToolType::Fire) {
        return;
    }
    Missile preview = makePreviewMissile(player);
    if (preview.state != MissileState::Flying) {
        return;
    }

    std::vector<Vec3> points = predictArcPartial(preview, wind_, 0.035f, 420);
    for (int i = 0; i < static_cast<int>(points.size()); ++i) {
        float t = static_cast<float>(i) / std::max(1, static_cast<int>(points.size()) - 1);
        float alpha = 0.92f - t * 0.75f;
        float jitter = t * 5.0f;
        float wobbleX = std::sin((i + 1) * 1.9f + player.id) * jitter;
        float wobbleY = std::cos((i + 1) * 1.3f + player.id) * jitter;
        Vector2 p = worldPointToScreen(points[i]);
        Color c = player.id == 0 ? Fade(SKYBLUE, alpha) : Fade(PINK, alpha);
        DrawCircleV({p.x + wobbleX, p.y + wobbleY}, 5.8f - t * 2.2f, Fade(c, 0.24f));
        DrawCircleV({p.x + wobbleX, p.y + wobbleY}, 3.5f - t * 1.5f, c);
    }
}

void Game::drawHud() const {
    int hudX = GetScreenWidth() - 344;
    DrawRectangleRounded(Rectangle{static_cast<float>(hudX), 24.0f, 320.0f, 730.0f}, 0.03f, 8, Fade(Color{10, 14, 30, 255}, 0.88f));
    DrawRectangleRoundedLines(Rectangle{static_cast<float>(hudX), 24.0f, 320.0f, 730.0f}, 0.03f, 8, Fade(Color{188, 224, 255, 255}, 0.20f));
    DrawRectangleGradientV(hudX + 1, 25, 318, 90, Fade(Color{40, 74, 112, 255}, 0.78f), Fade(BLANK, 0.0f));
    DrawCircleGradient(hudX + 220, 86, 120.0f, Fade(Color{255, 200, 92, 80}, 0.12f), Fade(BLANK, 0.0f));

    DrawText("INTERPLANETARY", hudX + 18, 24, 28, RAYWHITE);

    char line[256];
    std::snprintf(line, sizeof(line), "Turn %d", turnSystem_.turnNumber);
    DrawText(line, hudX + 18, 78, 26, GOLD);
    DrawText(phaseName(currentPhase()), hudX + 18, 108, 24, WHITE);
    std::snprintf(line, sizeof(line), "Timer %.0fs", std::ceil(turnSystem_.timeRemaining));
    DrawText(line, hudX + 18, 138, 24, WHITE);

    if (currentPhase() == TurnPhase::AttackDefend) {
        std::snprintf(line, sizeof(line), "Wind %.1f  dir(%.2f, %.2f)", wind_.strength, wind_.direction.x, wind_.direction.y);
        DrawText(line, hudX + 18, 176, 18, Color{160, 215, 255, 255});
    } else {
        DrawText("Wind dormant during mining.", hudX + 18, 176, 18, Fade(RAYWHITE, 0.65f));
    }

    int y = 224;
    const int cardHeight = 242;
    for (const Player& player : players_) {
        Color tint = player.id == 0 ? Color{108, 216, 255, 255} : Color{255, 142, 142, 255};
        DrawRectangleRounded(Rectangle{static_cast<float>(hudX + 14), static_cast<float>(y - 8), 292.0f, static_cast<float>(cardHeight)}, 0.025f, 6, Fade(tint, 0.07f));
        DrawRectangleGradientV(hudX + 14, y - 8, 292, 54, Fade(tint, 0.12f), Fade(BLANK, 0.0f));
        DrawRectangleRoundedLines(Rectangle{static_cast<float>(hudX + 14), static_cast<float>(y - 8), 292.0f, static_cast<float>(cardHeight)}, 0.025f, 6, Fade(tint, 0.12f));
        DrawText(player.name.c_str(), hudX + 18, y, 24, tint);
        std::snprintf(line, sizeof(line), "HP %d / %d", player.health, player.maxHealth);
        DrawText(line, hudX + 18, y + 32, 20, WHITE);
        std::snprintf(line, sizeof(line), "Fuel %d", player.fuel);
        DrawText(line, hudX + 18, y + 62, 20, WHITE);
        std::snprintf(line, sizeof(line), "Metal %d", player.metal);
        DrawText(line, hudX + 18, y + 86, 20, WHITE);
        std::snprintf(line, sizeof(line), "Tool %s", toolName(player.tool));
        DrawText(line, hudX + 18, y + 112, 20, WHITE);
        std::snprintf(line, sizeof(line), "Aim %.0f  Power %.0f", player.aimAngleDeg, player.launchPower);
        DrawText(line, hudX + 18, y + 138, 18, Fade(RAYWHITE, 0.8f));
        DrawText(predictedImpactText(player).c_str(), hudX + 18, y + 162, 18, Fade(RAYWHITE, 0.8f));
        DrawText(contextualCostText(player).c_str(), hudX + 18, y + 186, 18, GOLD);
        if (currentPhase() == TurnPhase::MineBuild && player.tool == ToolType::Mine) {
            Vec3i target = frontCell(player);
            Block targetBlock = world_.get(target);
            if (world_.inBounds(target) && targetBlock.type != BlockType::Air) {
                float totalTime = mineTimeForBlock(targetBlock.type);
                float pct = totalTime > 0.0f ? clampf(player.mining.progress / totalTime, 0.0f, 1.0f) : 0.0f;
                std::snprintf(line, sizeof(line), "Mining %d,%d  %d%%", target.x, target.y, static_cast<int>(pct * 100.0f));
                DrawText(line, hudX + 18, y + 210, 18, WHITE);
            } else {
                DrawText("Mining: face a solid block", hudX + 18, y + 210, 18, WHITE);
            }
        } else if (currentPhase() == TurnPhase::MineBuild) {
            DrawText("Mining is manual only", hudX + 18, y + 210, 18, WHITE);
        }
        y += cardHeight + 14;
    }

    if (gameOver_) {
        const Player& winner = players_[winnerId_];
        DrawRectangle(190, 250, 560, 120, Fade(BLACK, 0.55f));
        DrawRectangleLines(190, 250, 560, 120, Fade(RAYWHITE, 0.25f));
        std::snprintf(line, sizeof(line), "%s wins the cubic planet duel", winner.name.c_str());
        DrawText(line, 220, 288, 28, winner.id == 0 ? SKYBLUE : PINK);
        DrawText("Press ENTER to reset", 320, 326, 20, WHITE);
    }
}

void Game::drawHelp() const {
    int helpWidth = GetScreenWidth() - 420;
    const float helpY = static_cast<float>(GetScreenHeight() - 126);
    const float helpHeight = 96.0f;
    DrawRectangleRounded(Rectangle{34.0f, helpY, static_cast<float>(helpWidth), helpHeight}, 0.03f, 8, Fade(Color{10, 14, 30, 255}, 0.84f));
    DrawRectangleRoundedLines(Rectangle{34.0f, helpY, static_cast<float>(helpWidth), helpHeight}, 0.03f, 8, Fade(Color{188, 224, 255, 255}, 0.18f));
    DrawRectangleGradientV(35, static_cast<int>(helpY + 1.0f), helpWidth - 2, 28, Fade(Color{60, 132, 220, 255}, 0.10f), Fade(BLANK, 0.0f));
    DrawText("P1  WASD move  Q/E tools  F use  R place  T/G angle  Y/H power", 48, static_cast<int>(helpY + 18.0f), 20, Color{140, 225, 255, 255});
    DrawText("P2  Arrows move  ,/. tools  O use  P place  I/K angle  U/J power", 48, static_cast<int>(helpY + 50.0f), 20, Color{255, 168, 168, 255});
}
