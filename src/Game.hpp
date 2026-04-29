#pragma once

#include "World.hpp"

class Game {
public:
    Game();

    void update(float dt);
    void draw() const;

private:
    TurnPhase currentPhase() const;
    void reset();
    void updateTurnSystem(float dt);
    void randomizeWind();

    void updatePlayer(float dt, Player& player, int leftKey, int rightKey, int upKey, int downKey,
                      int prevToolKey, int nextToolKey, int useKey, int placeKey,
                      int angleUpKey, int angleDownKey, int powerUpKey, int powerDownKey);
    void handleMovement(Player& player, int dx, int dy);
    void handleUse(Player& player, bool pressed, bool held, float dt);
    void handlePlace(Player& player);

    std::vector<ToolType> availableToolsForPhase(TurnPhase phase) const;
    void cycleTool(Player& player, int direction);

    Vec3i buildCellForPlayer(const Player& player) const;
    Vec3i frontCell(const Player& player) const;
    Vec3i wallPlacementCell(const Player& player) const;
    float mineTimeForBlock(BlockType type) const;
    bool collectBlockResource(Player& player, BlockType type);
    void applyDamage(Player& player, int damage, int sourceOwnerId);
    bool spend(Player& player, int fuelCost, int metalCost);
    bool canPlayerStand(const Player& player, const Vec3i& cell) const;

    void updateMiningBlocks(float dt);
    void fireMissile(Player& player);
    std::optional<Vec3i> findNearbyOwnedSilo(const Player& player) const;
    void updateMissiles(float dt);
    void explodeAt(const Vec3& position, float radius, int ownerId);
    bool wallResistsExplosion(const Vec3i& cell) const;
    float wallDamageReductionBetween(const Vec3& from, const Vec3& to) const;
    void updateExplosions(float dt);
    Vec3i initialSiloPosition(int playerId) const;
    std::vector<Vec3> predictArcPartial(Missile missile, const Wind& wind, float dt, int totalSamples) const;
    Missile makePreviewMissile(const Player& player) const;
    std::string predictedImpactText(const Player& player) const;
    std::string contextualCostText(const Player& player) const;
    bool missileHasCollided(const Missile& missile) const;
    void updateMissilePhysics(Missile& missile, const Wind& wind, float dt) const;

    void drawWorldSlice() const;
    void drawPlayers() const;
    void drawMissiles() const;
    void drawExplosions() const;
    void drawHud() const;
    void drawPreviewArc(const Player& player) const;
    void drawHelp() const;

    World world_;
    Player players_[2];
    TurnSystem turnSystem_;
    Wind wind_;
    std::vector<MiningBlockEntity> miningBlocks_;
    std::vector<Missile> missiles_;
    std::vector<Explosion> explosions_;

    bool showHelp_ = true;
    bool gameOver_ = false;
    int winnerId_ = -1;
};
