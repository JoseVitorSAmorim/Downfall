#pragma once

#include "raylib.h"
#include "raymath.h"
#include "World.hpp"
#include "Player.hpp"
#include "Navigation.hpp"
#include "Events.hpp"
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>

enum class SquadOrder {
    ADVANCE = 0,
    HOLD,
    SUPPRESS
};

inline const char* GetOrderLabel(SquadOrder order) {
    switch (order) {
        case SquadOrder::ADVANCE:  return "AVANCAR [F1]";
        case SquadOrder::HOLD:     return "MANTER POSICAO [F2]";
        case SquadOrder::SUPPRESS: return "FOGO DE SUPRESSAO [F3]";
        default:                   return "MANTER POSICAO";
    }
}

enum class PingType {
    NONE = 0,
    MOVE_COVER,
    FOCUS_TARGET
};

struct TacticalPing {
    PingType type = PingType::NONE;
    Vector3 worldPos = { 0.0f, 0.0f, 0.0f };
    int assignedAllyId = -1;
    int targetEnemyIdx = -1;
    float timer = 0.0f;

    void Clear() {
        type = PingType::NONE;
        assignedAllyId = -1;
        targetEnemyIdx = -1;
        timer = 0.0f;
    }
};

struct DamageIndicator {
    Vector3 attackerPos;
    float timer;
};

struct SquadAlly {
    int id = 1;
    std::string name = "Bravo";
    Vector3 pos = { 0.0f, 0.0f, 0.0f };
    Vector3 spawnPos = { 0.0f, 0.0f, 0.0f };
    Vector3 anchorPos = { 0.0f, 0.0f, 0.0f };
    float hp = 100.0f;
    float aggro = 50.0f;
    float shootCooldown = 0.5f;
    float strafePhase = 0.0f;
    Color color = SKYBLUE;

    bool isDowned = false;
    bool isBeingDragged = false;
    float reviveProgress = 0.0f;
    float bleedoutTimer = 25.0f;

    float lastHp = 100.0f;
    float suppression = 0.0f;
    bool isSuppressed = false;
    bool hasCoverPos = false;
    Vector3 coverPos = { 0.0f, 0.0f, 0.0f };
    float coverSearchTimer = 0.0f;

    bool hasPingOrder = false;
    Vector3 pingTargetPos = { 0.0f, 0.0f, 0.0f };

    std::vector<Vector3> navPath;
    int navWaypointIdx = 0;
    float navRepathTimer = 0.0f;
    Vector3 navGoalPos = { 0.0f, 0.0f, 0.0f };

    Vector3 GetEyePos() const {
        float eyeH = isDowned ? 0.55f : (isSuppressed ? 1.15f : 1.65f);
        return { pos.x, pos.y + eyeH, pos.z };
    }
};

struct EnemyBot {
    int id = 1;
    Vector3 pos = { 0.0f, 0.0f, 0.0f };
    Vector3 spawnPos = { 0.0f, 0.0f, 0.0f };
    float hp = 150.0f;
    float maxHp = 150.0f;
    float shootTimer = 0.8f;
    float strafePhase = 0.0f;
    float suppression = 0.0f;
    bool isSuppressed = false;
    int currentTargetId = -1;

    float lastHp = -1.0f;
    bool hasCoverPos = false;
    Vector3 coverPos = { 0.0f, 0.0f, 0.0f };
    float coverSearchTimer = 0.0f;

    std::vector<Vector3> navPath;
    int navWaypointIdx = 0;
    float navRepathTimer = 0.0f;
    Vector3 navGoalPos = { 0.0f, 0.0f, 0.0f };

    bool IsAlive() const { return hp > 0.0f; }

    Vector3 GetEyePos() const {
        float eyeH = isSuppressed ? 1.15f : 1.65f;
        return { pos.x, pos.y + eyeH, pos.z };
    }

    BoundingBox GetTorsoAABB() const {
        float h = isSuppressed ? 1.00f : 1.50f;
        return {
            { pos.x - 0.36f, pos.y, pos.z - 0.36f },
            { pos.x + 0.36f, pos.y + h, pos.z + 0.36f }
        };
    }

    BoundingBox GetHeadAABB() const {
        float baseH = isSuppressed ? 1.00f : 1.50f;
        return {
            { pos.x - 0.20f, pos.y + baseH, pos.z - 0.20f },
            { pos.x + 0.20f, pos.y + baseH + 0.38f, pos.z + 0.20f }
        };
    }
};

inline void ResetWave(std::vector<EnemyBot>& enemies) {
    enemies.clear();
    enemies.push_back({ 1, { -8.0f, 0.0f, -11.0f }, { -8.0f, 0.0f, -11.0f }, 150.0f, 150.0f, 0.5f, 0.0f, 0.0f, false, -1 });
    enemies.push_back({ 2, { -2.8f, 0.0f, -13.5f }, { -2.8f, 0.0f, -13.5f }, 150.0f, 150.0f, 0.8f, 1.2f, 0.0f, false, -1 });
    enemies.push_back({ 3, {  2.8f, 0.0f, -13.5f }, {  2.8f, 0.0f, -13.5f }, 150.0f, 150.0f, 1.0f, 2.4f, 0.0f, false, -1 });
    enemies.push_back({ 4, {  8.0f, 0.0f, -11.0f }, {  8.0f, 0.0f, -11.0f }, 150.0f, 150.0f, 0.7f, 3.6f, 0.0f, false, -1 });
}

inline void MoveCharacterWithCollision(Vector3& pos, float radius, float height,
                                       Vector3 targetPos, float speed, float dt,
                                       const std::vector<WorldBlock>& world,
                                       const SpatialHashGrid& grid) {
    Vector3 diff = { targetPos.x - pos.x, 0.0f, targetPos.z - pos.z };
    float dist = std::sqrt(diff.x * diff.x + diff.z * diff.z);
    if (dist < 0.08f) return;

    float step = std::min(dist, speed * dt);
    Vector3 dir = { diff.x / dist, 0.0f, diff.z / dist };
    Vector3 boxSize = { radius * 2.0f, height, radius * 2.0f };
    constexpr float BOT_MAX_STEP = 0.60f;

    auto tryAxisMove = [&](float dx, float dz) -> bool {
        float nx = pos.x + dx;
        float nz = pos.z + dz;
        BoundingBox testBox = {
            { nx - radius, pos.y,          nz - radius },
            { nx + radius, pos.y + height, nz + radius }
        };

        bool blocked = false;
        float liftY = pos.y;

        grid.QueryAABB({ nx, pos.y + height * 0.5f, nz }, { radius * 2.1f, height + BOT_MAX_STEP, radius * 2.1f }, [&](int idx) -> bool {
            const WorldBlock& blk = world[idx];
            BoundingBox bb = blk.GetAABB();
            if (!CheckOverlapStrict(testBox, bb)) return true;

            if (blk.IsSlope()) {
                float surfY = blk.GetMaxSurfaceUnderBox(testBox);
                if (pos.y >= surfY - BOT_MAX_STEP) {
                    liftY = std::max(liftY, surfY);
                    return true;
                }
            } else {
                float stepDelta = bb.max.y - pos.y;
                if (stepDelta > 0.0f && stepDelta <= BOT_MAX_STEP) {
                    liftY = std::max(liftY, bb.max.y);
                    return true;
                }
            }
            blocked = true;
            return false;
        });

        if (!blocked) {
            if (liftY > pos.y + 1e-4f) {
                if (!OverlapsAnyBlock({ nx, liftY + height * 0.5f, nz }, boxSize, world, grid)) {
                    pos.x = nx;
                    pos.z = nz;
                    pos.y = liftY;
                    return true;
                }
                return false;
            }
            pos.x = nx;
            pos.z = nz;
            return true;
        }
        return false;
    };

    if (!tryAxisMove(dir.x * step, 0.0f)) {
        float slideSign = (dir.z >= 0.0f) ? 1.0f : -1.0f;
        tryAxisMove(0.0f, slideSign * step * 0.70f);
    }

    if (!tryAxisMove(0.0f, dir.z * step)) {
        float slideSign = (dir.x >= 0.0f) ? 1.0f : -1.0f;
        tryAxisMove(slideSign * step * 0.70f, 0.0f);
    }

    BoundingBox groundProbe = {
        { pos.x - radius * 0.85f, pos.y - 1.5f, pos.z - radius * 0.85f },
        { pos.x + radius * 0.85f, pos.y + 0.1f, pos.z + radius * 0.85f }
    };
    float bestFloorY = -1e30f;
    bool foundFloor = false;

    grid.QueryAABB({ pos.x, pos.y - 0.5f, pos.z }, { radius * 2.0f, 1.8f, radius * 2.0f }, [&](int idx) -> bool {
        const WorldBlock& blk = world[idx];
        BoundingBox bb = blk.GetAABB();
        if (!CheckOverlapStrict(groundProbe, bb)) return true;

        float floorY = blk.IsSlope() ? blk.GetMaxSurfaceUnderBox(groundProbe) : bb.max.y;
        if (floorY <= pos.y + 0.05f && floorY > bestFloorY) {
            bestFloorY = floorY;
            foundFloor = true;
        }
        return true;
    });

    if (foundFloor) {
        float drop = pos.y - bestFloorY;
        if (drop <= 0.35f) {
            pos.y = bestFloorY;
        } else {
            pos.y = std::max(bestFloorY, pos.y - 9.5f * dt);
        }
    }
}

inline void MoveCharacterWithCollision(Vector3& pos, float radius, float height,
                                       Vector3 targetPos, float speed, float dt,
                                       const std::vector<WorldBlock>& world) {
    MoveCharacterWithCollision(pos, radius, height, targetPos, speed, dt, world, GetWorldSpatialGrid(world));
}

inline void MoveAllyWithCollision(SquadAlly& ally, Vector3 targetPos, float speed, float dt,
                                  const std::vector<WorldBlock>& world) {
    float h = ally.isDowned ? 0.70f : (ally.isSuppressed ? 1.25f : 1.80f);
    MoveCharacterWithCollision(ally.pos, 0.38f, h, targetPos, speed, dt, world, GetWorldSpatialGrid(world));
}

inline void NavigateAgentTo(Vector3& pos, float radius, float height,
                            Vector3 goalPos, float speed, float dt,
                            std::vector<Vector3>& navPath, int& navWaypointIdx,
                            float& navRepathTimer, Vector3& cachedGoal,
                            const std::vector<WorldBlock>& world,
                            const SpatialHashGrid& spatialGrid,
                            const NavGrid& navGrid) {
    navRepathTimer -= dt;
    float goalShift = Vector3Distance(goalPos, cachedGoal);

    if (navPath.empty() || navWaypointIdx >= static_cast<int>(navPath.size()) ||
        goalShift > 0.95f || navRepathTimer <= 0.0f) {
        cachedGoal = goalPos;
        navRepathTimer = 0.45f;
        navWaypointIdx = 0;
        navGrid.FindPath(pos, goalPos, world, spatialGrid, navPath);
    }

    if (navPath.empty()) {
        MoveCharacterWithCollision(pos, radius, height, goalPos, speed, dt, world, spatialGrid);
        return;
    }

    while (navWaypointIdx < static_cast<int>(navPath.size()) - 1) {
        Vector3 wp = navPath[navWaypointIdx];
        float dx = wp.x - pos.x;
        float dz = wp.z - pos.z;
        if (std::sqrt(dx * dx + dz * dz) < 0.42f && std::fabs(wp.y - pos.y) < 0.85f) {
            navWaypointIdx++;
        } else {
            break;
        }
    }

    Vector3 activeTarget = navPath[navWaypointIdx];
    MoveCharacterWithCollision(pos, radius, height, activeTarget, speed, dt, world, spatialGrid);
}

// Busca 8 pontos distribuídos ao redor de cada cobertura e valida proteção agachada (y + 0.85m)
inline bool FindBestCoverPoint(Vector3 charPos, Vector3 threatPos,
                               const std::vector<WorldBlock>& world,
                               Vector3& outCover) {
    const SpatialHashGrid& grid = GetWorldSpatialGrid(world);
    const NavGrid& navGrid = GetWorldNavGrid(world);
    float bestScore = 1e30f;
    bool found = false;

    const float searchRadius = 18.0f;
    Vector3 searchSize = { searchRadius * 2.0f, 6.0f, searchRadius * 2.0f };

    grid.QueryAABB(charPos, searchSize, [&](int idx) -> bool {
        const WorldBlock& b = world[idx];
        if (b.IsSlope() || b.size.y < 0.85f || b.size.x > 25.0f || b.size.z > 25.0f) return true;

        float margin = 0.78f;
        float offX = std::min(0.85f, b.size.x * 0.28f);
        float offZ = std::min(0.85f, b.size.z * 0.28f);

        Vector3 candidates[8] = {
            { b.pos.x + b.size.x * 0.5f + margin, charPos.y, b.pos.z - offZ },
            { b.pos.x + b.size.x * 0.5f + margin, charPos.y, b.pos.z + offZ },
            { b.pos.x - b.size.x * 0.5f - margin, charPos.y, b.pos.z - offZ },
            { b.pos.x - b.size.x * 0.5f - margin, charPos.y, b.pos.z + offZ },
            { b.pos.x - offX, charPos.y, b.pos.z + b.size.z * 0.5f + margin },
            { b.pos.x + offX, charPos.y, b.pos.z + b.size.z * 0.5f + margin },
            { b.pos.x - offX, charPos.y, b.pos.z - b.size.z * 0.5f - margin },
            { b.pos.x + offX, charPos.y, b.pos.z - b.size.z * 0.5f - margin }
        };

        for (Vector3 cand : candidates) {
            int nIdx = navGrid.FindNearestNode(cand);
            if (nIdx == -1) continue;

            Vector3 nodePos = navGrid.GetNodePosition(nIdx);
            if (std::fabs(nodePos.x - cand.x) > 1.35f || std::fabs(nodePos.z - cand.z) > 1.35f) continue;
            cand.y = nodePos.y;

            // Verifica folga livre para o corpo agachado atrás da cobertura
            if (OverlapsAnyBlock({ cand.x, cand.y + 0.65f, cand.z }, { 0.68f, 1.15f, 0.68f }, world, grid)) continue;

            // Testa proteção na altura agachada (0.85m) contra a linha de tiro da ameaça
            Vector3 crouchedEye = { cand.x, cand.y + 0.85f, cand.z };
            if (!HasLineOfSight(threatPos, crouchedEye, world, grid)) {
                float distToChar = Vector3Distance(charPos, cand);
                float distToThreat = Vector3Distance(threatPos, cand);
                float tallCoverBonus = (b.size.y >= 1.4f) ? -0.8f : 0.0f;
                float score = distToChar - distToThreat * 0.12f + tallCoverBonus;
                if (score < bestScore) {
                    bestScore = score;
                    outCover = cand;
                    found = true;
                }
            }
        }
        return true;
    });

    return found;
}

inline bool ProcessTacticalPing(Vector3 eyePos, Vector3 forward,
                                const std::vector<WorldBlock>& world,
                                std::vector<SquadAlly>& allies,
                                const std::vector<EnemyBot>& enemies,
                                TacticalPing& ping) {
    const SpatialHashGrid& grid = GetWorldSpatialGrid(world);
    const NavGrid& navGrid = GetWorldNavGrid(world);
    Ray ray = { eyePos, forward };

    int hitEnemy = -1;
    float bestEnemyDist = 80.0f;
    for (size_t i = 0; i < enemies.size(); i++) {
        if (!enemies[i].IsAlive()) continue;
        RayCollision colTorso = GetRayCollisionBox(ray, enemies[i].GetTorsoAABB());
        RayCollision colHead = GetRayCollisionBox(ray, enemies[i].GetHeadAABB());
        float d = 1e30f;
        if (colTorso.hit && colTorso.distance < d) d = colTorso.distance;
        if (colHead.hit && colHead.distance < d) d = colHead.distance;

        if (d < bestEnemyDist && HasLineOfSight(eyePos, enemies[i].GetEyePos(), world, grid)) {
            bestEnemyDist = d;
            hitEnemy = static_cast<int>(i);
        }
    }

    if (hitEnemy != -1) {
        ping.type = PingType::FOCUS_TARGET;
        ping.targetEnemyIdx = hitEnemy;
        ping.worldPos = enemies[hitEnemy].pos;
        ping.timer = 8.0f;
        return true;
    }

    float closestBlockDist = 75.0f;
    Vector3 hitPoint = { 0.0f, 0.0f, 0.0f };
    int hitBlockIdx = -1;

    grid.TraverseRayXZ(eyePos, forward, closestBlockDist, [&](int idx) -> bool {
        RayCollision col = GetRayCollisionBox(ray, world[idx].GetAABB());
        if (col.hit && col.distance > 0.4f && col.distance < closestBlockDist) {
            closestBlockDist = col.distance;
            hitPoint = col.point;
            hitBlockIdx = idx;
        }
        return true;
    });

    if (hitBlockIdx == -1 || allies.empty()) return false;

    Vector3 targetCover = { hitPoint.x, 0.0f, hitPoint.z };
    int nearNode = navGrid.FindNearestNode(hitPoint);
    if (nearNode != -1) {
        targetCover.y = navGrid.GetNodePosition(nearNode).y;
    }

    Vector3 threatRef = { 0.0f, 1.5f, -14.0f };
    for (const auto& e : enemies) {
        if (e.IsAlive()) {
            threatRef = e.GetEyePos();
            break;
        }
    }

    const WorldBlock& blk = world[hitBlockIdx];
    if (!blk.IsSlope() && blk.size.y >= 0.85f && blk.size.x < 25.0f && blk.size.z < 25.0f) {
        Vector3 bestSide = targetCover;
        if (FindBestCoverPoint(targetCover, threatRef, world, bestSide) &&
            Vector3Distance(targetCover, bestSide) < 4.5f) {
            targetCover = bestSide;
        }
    }

    int bestAllyIdx = -1;
    float bestAllyDist = 1e30f;
    for (size_t i = 0; i < allies.size(); i++) {
        if (allies[i].isDowned) continue;
        float d = Vector3Distance(allies[i].pos, targetCover);
        if (d < bestAllyDist) {
            bestAllyDist = d;
            bestAllyIdx = static_cast<int>(i);
        }
    }

    if (bestAllyIdx == -1) return false;

    for (auto& a : allies) a.hasPingOrder = false;
    allies[bestAllyIdx].hasPingOrder = true;
    allies[bestAllyIdx].pingTargetPos = targetCover;
    allies[bestAllyIdx].anchorPos = targetCover;
    allies[bestAllyIdx].navPath.clear();
    allies[bestAllyIdx].navRepathTimer = 0.0f;

    ping.type = PingType::MOVE_COVER;
    ping.worldPos = targetCover;
    ping.assignedAllyId = allies[bestAllyIdx].id;
    ping.targetEnemyIdx = -1;
    ping.timer = 10.0f;
    return true;
}

inline void UpdateAllies(float dt, std::vector<SquadAlly>& allies,
                         std::vector<EnemyBot>& enemies, SourcePlayer& player,
                         SquadOrder order, const std::vector<WorldBlock>& world,
                         std::vector<Tracer>& tracers, TacticalPing& ping) {
    const SpatialHashGrid& grid = GetWorldSpatialGrid(world);
    const NavGrid& navGrid = GetWorldNavGrid(world);

    if (ping.timer > 0.0f) {
        ping.timer -= dt;
        if (ping.timer <= 0.0f) {
            ping.Clear();
            for (auto& a : allies) a.hasPingOrder = false;
        } else if (ping.type == PingType::FOCUS_TARGET) {
            if (ping.targetEnemyIdx < 0 || ping.targetEnemyIdx >= static_cast<int>(enemies.size()) ||
                !enemies[ping.targetEnemyIdx].IsAlive()) {
                ping.Clear();
            } else {
                ping.worldPos = enemies[ping.targetEnemyIdx].pos;
            }
        }
    }

    int rescuerIdx = -1;
    if (player.isDowned) {
        float minD = 1e30f;
        for (size_t i = 0; i < allies.size(); i++) {
            if (allies[i].isDowned) continue;
            float d = Vector3Distance(allies[i].pos, player.pos);
            if (d < minD) {
                minD = d;
                rescuerIdx = static_cast<int>(i);
            }
        }
    }

    for (size_t i = 0; i < allies.size(); i++) {
        SquadAlly& ally = allies[i];
        int allySlot = static_cast<int>(i) + 1;

        if (ally.isDowned) {
            ally.aggro = 0.0f;
            ally.suppression = 0.0f;
            ally.isSuppressed = false;
            ally.bleedoutTimer = std::max(0.0f, ally.bleedoutTimer - dt);
            continue;
        }

        if (ally.hp < ally.lastHp) {
            ally.suppression = std::min(100.0f, ally.suppression + (ally.lastHp - ally.hp) * 2.8f);
        }
        ally.lastHp = ally.hp;

        bool runningToCover = ally.isSuppressed && ally.hasCoverPos && Vector3Distance(ally.pos, ally.coverPos) > 0.75f;
        float suppDecay = runningToCover ? 5.5f : 12.0f;
        ally.suppression = std::max(0.0f, ally.suppression - suppDecay * dt);

        if (!ally.isSuppressed && ally.suppression >= 40.0f) {
            ally.isSuppressed = true;
            ally.coverSearchTimer = 0.0f;
            ally.navPath.clear();
        } else if (ally.isSuppressed && ally.suppression < 18.0f) {
            ally.isSuppressed = false;
            ally.hasCoverPos = false;
            ally.navPath.clear();
        }

        float aggroFloor = (order == SquadOrder::SUPPRESS) ? 65.0f : 12.0f;
        ally.aggro = std::max(aggroFloor, ally.aggro - 7.5f * dt);
        ally.strafePhase += dt * 1.9f;

        if (static_cast<int>(i) == rescuerIdx && player.isDowned) {
            float dToPlayer = Vector3Distance(ally.pos, player.pos);
            if (dToPlayer > 1.75f) {
                NavigateAgentTo(ally.pos, 0.38f, 1.80f, player.pos, 5.4f, dt,
                                ally.navPath, ally.navWaypointIdx, ally.navRepathTimer, ally.navGoalPos,
                                world, grid, navGrid);
            } else {
                player.reviveProgress += dt * 0.40f;
                if (player.reviveProgress >= 1.0f) {
                    player.isDowned = false;
                    player.hp = 75.0f;
                    player.reviveProgress = 0.0f;
                    EventManager::Emit(CombatantRevivedEvent{ allySlot, 0, "Alpha", player.pos });
                }
            }
            continue;
        }

        if (ally.isSuppressed) {
            ally.coverSearchTimer -= dt;
            if (!ally.hasCoverPos || ally.coverSearchTimer <= 0.0f) {
                ally.coverSearchTimer = 1.6f;
                Vector3 threat = { 0.0f, 1.5f, -14.0f };
                for (const auto& e : enemies) {
                    if (e.IsAlive()) {
                        threat = e.GetEyePos();
                        break;
                    }
                }
                if (FindBestCoverPoint(ally.pos, threat, world, ally.coverPos)) {
                    ally.hasCoverPos = true;
                    ally.navPath.clear();
                }
            }
            if (ally.hasCoverPos) {
                NavigateAgentTo(ally.pos, 0.38f, 1.25f, ally.coverPos, 4.8f, dt,
                                ally.navPath, ally.navWaypointIdx, ally.navRepathTimer, ally.navGoalPos,
                                world, grid, navGrid);
            }
            continue;
        }

        if (ally.hasPingOrder) {
            NavigateAgentTo(ally.pos, 0.38f, 1.80f, ally.pingTargetPos, 5.2f, dt,
                            ally.navPath, ally.navWaypointIdx, ally.navRepathTimer, ally.navGoalPos,
                            world, grid, navGrid);
        } else if (order == SquadOrder::ADVANCE) {
            Vector3 advanceTarget = { ally.spawnPos.x, ally.pos.y, -9.5f };
            NavigateAgentTo(ally.pos, 0.38f, 1.80f, advanceTarget, 5.5f, dt,
                            ally.navPath, ally.navWaypointIdx, ally.navRepathTimer, ally.navGoalPos,
                            world, grid, navGrid);
        } else {
            Vector3 strafeTarget = {
                ally.anchorPos.x + std::sin(ally.strafePhase) * 1.15f,
                ally.anchorPos.y,
                ally.anchorPos.z
            };
            MoveCharacterWithCollision(ally.pos, 0.38f, 1.80f, strafeTarget, 3.2f, dt, world, grid);
        }

        int targetIdx = -1;
        if (ping.type == PingType::FOCUS_TARGET && ping.targetEnemyIdx >= 0 &&
            ping.targetEnemyIdx < static_cast<int>(enemies.size()) &&
            enemies[ping.targetEnemyIdx].IsAlive() &&
            HasLineOfSight(ally.GetEyePos(), enemies[ping.targetEnemyIdx].GetEyePos(), world, grid)) {
            targetIdx = ping.targetEnemyIdx;
        } else {
            float bestDist = 55.0f;
            for (size_t e = 0; e < enemies.size(); e++) {
                if (!enemies[e].IsAlive()) continue;
                float d = Vector3Distance(ally.pos, enemies[e].pos);
                if (d < bestDist && HasLineOfSight(ally.GetEyePos(), enemies[e].GetEyePos(), world, grid)) {
                    bestDist = d;
                    targetIdx = static_cast<int>(e);
                }
            }
        }

        ally.shootCooldown -= dt;
        if (targetIdx != -1 && ally.shootCooldown <= 0.0f) {
            EnemyBot& target = enemies[targetIdx];
            bool wasAlive = target.IsAlive();
            bool suppressMode = (order == SquadOrder::SUPPRESS);
            ally.shootCooldown = suppressMode
                ? (0.24f + static_cast<float>(GetRandomValue(0, 22)) * 0.01f)
                : (0.48f + static_cast<float>(GetRandomValue(0, 35)) * 0.01f);

            float dmg = suppressMode ? 14.0f : 19.0f;
            float suppAdd = suppressMode ? 28.0f : 16.0f;
            target.hp -= dmg;
            target.suppression = std::min(100.0f, target.suppression + suppAdd);

            ally.aggro = std::min(100.0f, ally.aggro + (suppressMode ? 26.0f : 14.0f));

            Vector3 muzzle = ally.GetEyePos();
            Vector3 targetEye = target.GetEyePos();
            tracers.push_back({ muzzle, targetEye, ally.color, 0.09f });

            EventManager::Emit(WeaponFiredEvent{
                allySlot, 2, muzzle, targetEye, false, false, false, ally.color
            });

            EventManager::Emit(DamageDealtEvent{
                allySlot, target.id, true, dmg, false, false, false, false, targetEye, ally.pos
            });

            if (wasAlive && !target.IsAlive()) {
                EventManager::Emit(EnemyKilledEvent{
                    allySlot, target.id, target.pos, false, false, false
                });
            }
        }
    }
}

inline void UpdateEnemies(float dt, std::vector<EnemyBot>& enemies,
                          SourcePlayer& player, std::vector<SquadAlly>& allies,
                          const std::vector<WorldBlock>& world,
                          std::vector<Tracer>& tracers,
                          std::vector<DamageIndicator>& damageIndicators) {
    (void)damageIndicators;
    const SpatialHashGrid& grid = GetWorldSpatialGrid(world);
    const NavGrid& navGrid = GetWorldNavGrid(world);

    for (auto& enemy : enemies) {
        if (!enemy.IsAlive()) continue;

        if (enemy.lastHp < 0.0f) enemy.lastHp = enemy.hp;
        if (enemy.hp < enemy.lastHp) {
            enemy.suppression = std::min(100.0f, enemy.suppression + (enemy.lastHp - enemy.hp) * 2.2f);
        }
        enemy.lastHp = enemy.hp;

        bool runningToCover = enemy.isSuppressed && enemy.hasCoverPos && Vector3Distance(enemy.pos, enemy.coverPos) > 0.75f;
        float suppDecay = runningToCover ? 5.5f : 11.5f;
        enemy.suppression = std::max(0.0f, enemy.suppression - suppDecay * dt);

        if (!enemy.isSuppressed && enemy.suppression >= 40.0f) {
            enemy.isSuppressed = true;
            enemy.coverSearchTimer = 0.0f;
            enemy.navPath.clear();
        } else if (enemy.isSuppressed && enemy.suppression < 18.0f) {
            enemy.isSuppressed = false;
            enemy.hasCoverPos = false;
            enemy.navPath.clear();
        }

        int bestTargetId = -1;
        float bestThreat = -1.0f;
        Vector3 bestThreatPos = player.GetEyePosition();
        Vector3 bestTargetFeet = player.pos;

        if (!player.isDowned) {
            bool los = HasLineOfSight(enemy.GetEyePos(), player.GetEyePosition(), world, grid);
            float threat = (player.aggro + 10.0f) * (los ? 1.0f : 0.4f);
            if (threat > bestThreat) {
                bestThreat = threat;
                bestTargetId = 0;
                bestThreatPos = player.GetEyePosition();
                bestTargetFeet = player.pos;
            }
        }

        for (size_t i = 0; i < allies.size(); i++) {
            if (allies[i].isDowned) continue;
            bool los = HasLineOfSight(enemy.GetEyePos(), allies[i].GetEyePos(), world, grid);
            float threat = (allies[i].aggro + 10.0f) * (los ? 1.0f : 0.4f);
            if (threat > bestThreat) {
                bestThreat = threat;
                bestTargetId = static_cast<int>(i) + 1;
                bestThreatPos = allies[i].GetEyePos();
                bestTargetFeet = allies[i].pos;
            }
        }

        enemy.currentTargetId = bestTargetId;

        if (enemy.isSuppressed) {
            enemy.coverSearchTimer -= dt;
            if (!enemy.hasCoverPos || enemy.coverSearchTimer <= 0.0f) {
                enemy.coverSearchTimer = 1.6f;
                if (FindBestCoverPoint(enemy.pos, bestThreatPos, world, enemy.coverPos)) {
                    enemy.hasCoverPos = true;
                    enemy.navPath.clear();
                }
            }
            if (enemy.hasCoverPos) {
                NavigateAgentTo(enemy.pos, 0.38f, 1.25f, enemy.coverPos, 4.7f, dt,
                                enemy.navPath, enemy.navWaypointIdx, enemy.navRepathTimer, enemy.navGoalPos,
                                world, grid, navGrid);
            }
            continue;
        }

        if (bestTargetId == -1) continue;

        Vector3 targetAim = (bestTargetId == 0)
            ? player.GetEyePosition()
            : allies[bestTargetId - 1].GetEyePos();

        bool hasLos = HasLineOfSight(enemy.GetEyePos(), targetAim, world, grid);
        if (!hasLos) {
            NavigateAgentTo(enemy.pos, 0.38f, 1.80f, bestTargetFeet, 3.4f, dt,
                            enemy.navPath, enemy.navWaypointIdx, enemy.navRepathTimer, enemy.navGoalPos,
                            world, grid, navGrid);
            continue;
        }

        enemy.shootTimer -= dt;
        if (enemy.shootTimer <= 0.0f) {
            enemy.shootTimer = 0.65f + static_cast<float>(GetRandomValue(0, 45)) * 0.01f;
            Vector3 muzzle = enemy.GetEyePos();
            tracers.push_back({ muzzle, targetAim, RED, 0.10f });

            EventManager::Emit(WeaponFiredEvent{
                99, 0, muzzle, targetAim, false, false, true, RED
            });

            if (bestTargetId == 0) {
                float dmg = 10.0f;
                player.hp -= dmg;
                EventManager::Emit(DamageDealtEvent{
                    99, 0, false, dmg, false, false, false, false, targetAim, enemy.pos
                });

                if (player.hp <= 0.0f) {
                    player.hp = 0.0f;
                    player.isDowned = true;
                    player.draggingAllyId = -1;
                    EventManager::Emit(CombatantDownedEvent{ 0, "Alpha", player.pos });
                }
            } else {
                SquadAlly& targetAlly = allies[bestTargetId - 1];
                float dmg = 13.0f;
                targetAlly.hp -= dmg;
                EventManager::Emit(DamageDealtEvent{
                    99, bestTargetId, false, dmg, false, false, false, false, targetAim, enemy.pos
                });

                if (targetAlly.hp <= 0.0f) {
                    targetAlly.hp = 0.0f;
                    targetAlly.isDowned = true;
                    targetAlly.bleedoutTimer = 25.0f;
                    EventManager::Emit(CombatantDownedEvent{ bestTargetId, targetAlly.name, targetAlly.pos });
                }
            }
        }
    }
}

inline void DrawTacticalPing3D(const TacticalPing& ping,
                               const std::vector<SquadAlly>& allies,
                               const std::vector<EnemyBot>& enemies) {
    (void)allies;
    if (ping.type == PingType::NONE || ping.timer <= 0.0f) return;

    if (ping.type == PingType::MOVE_COVER) {
        DrawCylinderWires(ping.worldPos, 0.35f, 0.35f, 2.0f, 8, SKYBLUE);
        DrawCircle3D({ ping.worldPos.x, ping.worldPos.y + 0.04f, ping.worldPos.z }, 0.65f, { 1.0f, 0.0f, 0.0f }, 90.0f, SKYBLUE);
    } else if (ping.type == PingType::FOCUS_TARGET &&
               ping.targetEnemyIdx >= 0 && ping.targetEnemyIdx < static_cast<int>(enemies.size()) &&
               enemies[ping.targetEnemyIdx].IsAlive()) {
        Vector3 ePos = enemies[ping.targetEnemyIdx].pos;
        DrawCircle3D({ ePos.x, ePos.y + 0.05f, ePos.z }, 0.85f, { 1.0f, 0.0f, 0.0f }, 90.0f, YELLOW);
    }
}