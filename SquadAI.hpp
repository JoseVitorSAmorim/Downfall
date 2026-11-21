#pragma once

#include "raylib.h"
#include "raymath.h"
#include "World.hpp"
#include "Player.hpp"
#include <vector>
#include <string>
#include <algorithm>
#include <cmath>

enum class SquadOrder {
    ADVANCE,
    HOLD,
    SUPPRESS
};

inline const char* GetOrderLabel(SquadOrder order) {
    switch (order) {
        case SquadOrder::ADVANCE:  return "[F1] AVANCAR";
        case SquadOrder::HOLD:     return "[F2] MANTER POSICAO";
        case SquadOrder::SUPPRESS: return "[F3] FOGO DE SUPRESSAO (PUXAR AGGRO)";
    }
    return "";
}

struct SquadAlly {
    int id;
    std::string name;
    Vector3 pos;
    Vector3 homePos;
    Vector3 anchorPos;
    float hp = 100.0f;
    float aggro = 20.0f;
    float shootCooldown = 0.0f;
    float strafePhase = 0.0f;
    Color color;

    // Estados de Downed & Revive
    bool isDowned = false;
    bool isBeingDragged = false;
    float reviveProgress = 0.0f;
    float bleedoutTimer = 25.0f;

    // Sistema de Supressão e Busca de Cobertura
    float lastHp = 100.0f;
    float suppression = 0.0f;
    bool isSuppressed = false;
    bool hasCoverPos = false;
    Vector3 coverPos = { 0.0f, 0.0f, 0.0f };
    float coverSearchTimer = 0.0f;

    BoundingBox GetAABB() const {
        float h = isDowned ? 0.65f : (isSuppressed ? 1.25f : 1.8f);
        return {
            { pos.x - 0.4f, pos.y,     pos.z - 0.4f },
            { pos.x + 0.4f, pos.y + h, pos.z + 0.4f }
        };
    }

    Vector3 GetEyePos() const {
        float eyeY = isDowned ? 0.45f : (isSuppressed ? 1.05f : 1.55f);
        return { pos.x, pos.y + eyeY, pos.z };
    }
};

struct EnemyBot {
    int id = 0;
    Vector3 pos = { 0.0f, 0.0f, 0.0f };
    Vector3 vel = { 0.0f, 0.0f, 0.0f };
    float hp = 150.0f;
    float maxHp = 150.0f;
    float lastHp = 150.0f;
    int currentTargetId = -1; // -1 = Nenhum/Suprimido, 0 = Jogador, 1..3 = Aliados
    float shootTimer = 0.0f;
    float strafeDir = 1.0f;
    float strafeTimer = 0.0f;

    // Sistema de Supressão e Busca de Cobertura
    float suppression = 0.0f;
    bool isSuppressed = false;
    bool hasCoverPos = false;
    Vector3 coverPos = { 0.0f, 0.0f, 0.0f };
    float coverSearchTimer = 0.0f;

    bool IsAlive() const { return hp > 0.0f; }

    BoundingBox GetAABB() const {
        float h = isSuppressed ? 1.25f : 1.9f;
        return {
            { pos.x - 0.55f, pos.y,     pos.z - 0.55f },
            { pos.x + 0.55f, pos.y + h, pos.z + 0.55f }
        };
    }

    Vector3 GetEyePos() const {
        return { pos.x, pos.y + (isSuppressed ? 1.05f : 1.5f), pos.z };
    }
};

// Movimentação AABB unificada em X e Z com deslize nas quinas de obstáculos
inline void MoveCharacterWithCollision(Vector3& pos, float halfW, float height, Vector3 desiredDelta,
                                       float dt, const std::vector<WorldBlock>& world) {
    bool blockedX = false;
    bool blockedZ = false;

    // Eixo X
    pos.x += desiredDelta.x;
    BoundingBox box = { { pos.x - halfW, pos.y, pos.z - halfW },
                        { pos.x + halfW, pos.y + height, pos.z + halfW } };
    for (const auto& block : world) {
        if (CheckOverlapStrict(box, block.bounds)) {
            blockedX = true;
            if (pos.x < block.pos.x) pos.x = block.bounds.min.x - halfW;
            else pos.x = block.bounds.max.x + halfW;
            box = { { pos.x - halfW, pos.y, pos.z - halfW },
                    { pos.x + halfW, pos.y + height, pos.z + halfW } };
        }
    }

    // Eixo Z
    pos.z += desiredDelta.z;
    box = { { pos.x - halfW, pos.y, pos.z - halfW },
            { pos.x + halfW, pos.y + height, pos.z + halfW } };
    for (const auto& block : world) {
        if (CheckOverlapStrict(box, block.bounds)) {
            blockedZ = true;
            if (pos.z < block.pos.z) pos.z = block.bounds.min.z - halfW;
            else pos.z = block.bounds.max.z + halfW;
            box = { { pos.x - halfW, pos.y, pos.z - halfW },
                    { pos.x + halfW, pos.y + height, pos.z + halfW } };
        }
    }

    // Deslize lateral para contornar quinas ao correr para cobertura
    if (blockedX && fabsf(desiredDelta.z) < 0.001f) {
        pos.z += (pos.z >= 0.0f ? -1.0f : 1.0f) * 2.5f * dt;
    } else if (blockedZ && fabsf(desiredDelta.x) < 0.001f) {
        pos.x += (pos.x >= 0.0f ? -1.0f : 1.0f) * 2.5f * dt;
    }
}

// Mantida para compatibilidade direta ao arrastar aliados caídos no main.cpp
inline void MoveAllyWithCollision(SquadAlly& ally, Vector3 targetPos, float lerpSpeed, float dt,
                                  const std::vector<WorldBlock>& world) {
    float h = ally.isDowned ? 0.65f : (ally.isSuppressed ? 1.25f : 1.8f);
    float factor = std::min(lerpSpeed * dt, 1.0f);
    Vector3 delta = {
        (targetPos.x - ally.pos.x) * factor,
        0.0f,
        (targetPos.z - ally.pos.z) * factor
    };
    MoveCharacterWithCollision(ally.pos, 0.4f, h, delta, dt, world);
}

// Avalia os blocos do mapa e encontra o ponto mais próximo que bloqueia a linha de visão da ameaça
inline bool FindBestCoverPoint(Vector3 entityPos, Vector3 threatEyePos,
                               const std::vector<WorldBlock>& world, Vector3& outCoverPos) {
    float bestScore = 99999.0f;
    bool found = false;
    float margin = 0.95f;

    for (const auto& block : world) {
        // Ignora o chão e as paredes externas da arena
        if (block.bounds.max.y < 1.1f || block.bounds.min.y > 0.5f) continue;
        if (block.size.x > 25.0f || block.size.z > 25.0f) continue;

        Vector3 candidates[4] = {
            { block.pos.x, 0.0f, block.bounds.min.z - margin }, // Norte (-Z)
            { block.pos.x, 0.0f, block.bounds.max.z + margin }, // Sul (+Z)
            { block.bounds.min.x - margin, 0.0f, block.pos.z }, // Oeste (-X)
            { block.bounds.max.x + margin, 0.0f, block.pos.z }  // Leste (+X)
        };

        for (const auto& cand : candidates) {
            if (fabsf(cand.x) > 22.5f || fabsf(cand.z) > 22.5f) continue;

            BoundingBox testBox = {
                { cand.x - 0.5f, 0.05f, cand.z - 0.5f },
                { cand.x + 0.5f, 1.30f, cand.z + 0.5f }
            };
            bool insideWall = false;
            for (const auto& other : world) {
                if (CheckOverlapStrict(testBox, other.bounds)) {
                    insideWall = true;
                    break;
                }
            }
            if (insideWall) continue;

            Vector3 candEye = { cand.x, 1.05f, cand.z };
            if (!HasLineOfSight(candEye, threatEyePos, world)) {
                float distToEntity = Vector3Distance(entityPos, cand);
                if (distToEntity < bestScore) {
                    bestScore = distToEntity;
                    outCoverPos = cand;
                    found = true;
                }
            }
        }
    }
    return found;
}

inline void ResetWave(std::vector<EnemyBot>& enemies) {
    enemies.clear();
    Vector3 spawns[4] = {
        { -8.0f, 0.0f, -15.0f },
        { -2.5f, 0.0f, -18.0f },
        {  3.0f, 0.0f, -17.0f },
        {  8.5f, 0.0f, -14.0f }
    };

    for (int i = 0; i < 4; i++) {
        EnemyBot bot;
        bot.id = i + 1;
        bot.pos = spawns[i];
        bot.shootTimer = 0.6f + i * 0.25f;
        bot.strafeDir = (i % 2 == 0) ? 1.0f : -1.0f;
        bot.strafeTimer = 1.2f + i * 0.3f;
        enemies.push_back(bot);
    }
}

inline void UpdateAllies(float dt, std::vector<SquadAlly>& allies, std::vector<EnemyBot>& enemies,
                         SourcePlayer& player, SquadOrder currentOrder,
                         const std::vector<WorldBlock>& world, std::vector<Tracer>& tracers) {
    int rescuerIdx = -1;
    if (player.isDowned) {
        float bestDist = 9999.0f;
        for (size_t i = 0; i < allies.size(); i++) {
            if (allies[i].isDowned) continue;
            float d = Vector3Distance(allies[i].pos, player.pos);
            if (d < bestDist) {
                bestDist = d;
                rescuerIdx = (int)i;
            }
        }
    }

    for (size_t i = 0; i < allies.size(); i++) {
        SquadAlly& ally = allies[i];

        if (ally.isDowned) {
            ally.aggro = 0.0f;
            ally.suppression = 0.0f;
            ally.isSuppressed = false;
            ally.hasCoverPos = false;
            if (!ally.isBeingDragged && ally.reviveProgress <= 0.0f) {
                ally.bleedoutTimer -= dt;
                if (ally.bleedoutTimer <= 0.0f) {
                    ally.isDowned = false;
                    ally.hp = 60.0f;
                    ally.lastHp = 60.0f;
                    ally.bleedoutTimer = 25.0f;
                    ally.reviveProgress = 0.0f;
                    ally.pos = ally.homePos;
                    ally.anchorPos = ally.homePos;
                }
            }
            continue;
        }

        // Converte dano recebido em supressão (menor impacto se o esquadrão estiver em Fogo de Supressão)
        if (ally.hp < ally.lastHp) {
            float dmgTaken = ally.lastHp - ally.hp;
            float moraleFactor = (currentOrder == SquadOrder::SUPPRESS) ? 0.65f : 1.1f;
            ally.suppression = std::min(100.0f, ally.suppression + dmgTaken * moraleFactor);
            ally.lastHp = ally.hp;
        }

        // Decaimento da supressão e regeneração leve de HP quando fora de perigo
        float decayRate = (currentOrder == SquadOrder::SUPPRESS) ? 18.0f : 14.0f;
        ally.suppression = std::max(0.0f, ally.suppression - decayRate * dt);
        if (!ally.isSuppressed && ally.hp < 100.0f) {
            ally.hp = std::min(100.0f, ally.hp + 2.5f * dt);
            ally.lastHp = ally.hp;
        }

        // Histerese de supressão dos aliados: entra em cobertura >= 45%, sai quando < 20%
        if (!ally.isSuppressed && ally.suppression >= 45.0f) {
            ally.isSuppressed = true;
            ally.coverSearchTimer = 0.0f;
        } else if (ally.isSuppressed && ally.suppression < 20.0f) {
            ally.isSuppressed = false;
            ally.hasCoverPos = false;
        }

        // Prioridade máxima: resgatar o jogador caído (ignora cobertura para salvar o líder)
        if ((int)i == rescuerIdx && player.isDowned) {
            Vector3 diff = Vector3Subtract(player.pos, ally.pos);
            diff.y = 0.0f;
            float dist = Vector3Length(diff);

            if (dist > 1.6f) {
                Vector3 stepDelta = Vector3Scale(Vector3Normalize(diff), 5.6f * dt);
                MoveCharacterWithCollision(ally.pos, 0.4f, 1.8f, stepDelta, dt, world);
            } else {
                player.reviveProgress += dt * 0.40f;
                if (player.reviveProgress >= 1.0f) {
                    player.isDowned = false;
                    player.hp = 70.0f;
                    player.reviveProgress = 0.0f;
                }
            }
            continue;
        }

        // Localiza a ameaça inimiga mais próxima para orientar cobertura e disparos
        Vector3 nearestThreatEye = { 0.0f, 1.5f, -16.0f };
        float minEnemyDist = 99999.0f;
        bool hasVisibleEnemy = false;
        for (const auto& enemy : enemies) {
            if (!enemy.IsAlive()) continue;
            float d = Vector3Distance(ally.pos, enemy.pos);
            bool los = HasLineOfSight(ally.GetEyePos(), enemy.GetEyePos(), world);
            if (los && (!hasVisibleEnemy || d < minEnemyDist)) {
                hasVisibleEnemy = true;
                minEnemyDist = d;
                nearestThreatEye = enemy.GetEyePos();
            } else if (!hasVisibleEnemy && d < minEnemyDist) {
                minEnemyDist = d;
                nearestThreatEye = enemy.GetEyePos();
            }
        }

        // MODO SUPRIMIDO DO ALIADO: Corre para trás de um bloco do mapa
        if (ally.isSuppressed) {
            ally.coverSearchTimer -= dt;
            if (!ally.hasCoverPos || ally.coverSearchTimer <= 0.0f) {
                ally.hasCoverPos = FindBestCoverPoint(ally.pos, nearestThreatEye, world, ally.coverPos);
                ally.coverSearchTimer = 0.65f;
            }

            if (ally.hasCoverPos) {
                Vector3 toCover = Vector3Subtract(ally.coverPos, ally.pos);
                toCover.y = 0.0f;
                float dist = Vector3Length(toCover);
                if (dist > 0.35f) {
                    Vector3 moveStep = Vector3Scale(Vector3Normalize(toCover), 4.8f * dt);
                    MoveCharacterWithCollision(ally.pos, 0.4f, 1.25f, moveStep, dt, world);
                }
            }

            // Enquanto protegido atrás da cobertura, suspende tiros e deixa o Aggro cair
            if (!HasLineOfSight(ally.GetEyePos(), nearestThreatEye, world)) {
                ally.aggro = std::max(10.0f, ally.aggro - 12.0f * dt);
                continue;
            }
        } else {
            // MODO COMBATE NORMAL: Executa ordens táticas (1: Avançar, 2: Manter, 3: Supressão)
            ally.strafePhase += dt * (currentOrder == SquadOrder::ADVANCE ? 2.2f : 1.2f);
            Vector3 desiredPos = ally.anchorPos;
            float moveSpeed = 4.0f;

            if (currentOrder == SquadOrder::ADVANCE) {
                float advanceZ = std::max(-9.5f, ally.homePos.z - 8.5f);
                desiredPos = { ally.homePos.x + sinf(ally.strafePhase) * 1.8f, ally.homePos.y, advanceZ };
                moveSpeed = 5.5f;
            } else if (currentOrder == SquadOrder::HOLD) {
                desiredPos = { ally.anchorPos.x + sinf(ally.strafePhase) * 0.45f, ally.anchorPos.y, ally.anchorPos.z };
                moveSpeed = 4.5f;
            } else if (currentOrder == SquadOrder::SUPPRESS) {
                desiredPos = { ally.anchorPos.x + sinf(ally.strafePhase) * 0.3f, ally.anchorPos.y, ally.anchorPos.z };
                moveSpeed = 3.5f;
            }

            MoveAllyWithCollision(ally, desiredPos, moveSpeed, dt, world);
        }

        // Parâmetros de tiro conforme postura (suprimido vs. ordem ativa)
        float minAggroFloor = (currentOrder == SquadOrder::SUPPRESS && !ally.isSuppressed) ? 65.0f : 25.0f;
        float aggroPerShot  = (currentOrder == SquadOrder::SUPPRESS) ? 26.0f : 15.0f;
        float shotDamage    = (currentOrder == SquadOrder::ADVANCE) ? 14.0f : ((currentOrder == SquadOrder::SUPPRESS) ? 8.0f : 12.0f);
        int minCooldownMs   = ally.isSuppressed ? 140 : ((currentOrder == SquadOrder::SUPPRESS) ? 24 : 75);
        int maxCooldownMs   = ally.isSuppressed ? 210 : ((currentOrder == SquadOrder::SUPPRESS) ? 46 : 140);

        ally.aggro = std::max(minAggroFloor, ally.aggro - 4.0f * dt);
        ally.shootCooldown -= dt;

        if (currentOrder == SquadOrder::SUPPRESS && !ally.isSuppressed) {
            for (auto& enemy : enemies) {
                if (!enemy.IsAlive()) continue;
                if (HasLineOfSight(ally.GetEyePos(), enemy.GetEyePos(), world)) {
                    enemy.suppression = std::min(100.0f, enemy.suppression + 14.0f * dt);
                }
            }
        }

        if (ally.shootCooldown <= 0.0f) {
            for (auto& enemy : enemies) {
                if (!enemy.IsAlive()) continue;
                if (HasLineOfSight(ally.GetEyePos(), enemy.GetEyePos(), world)) {
                    enemy.hp -= shotDamage;
                    enemy.lastHp = enemy.hp;

                    float suppressImpulse = (currentOrder == SquadOrder::SUPPRESS) ? 46.0f : 20.0f;
                    enemy.suppression = std::min(100.0f, enemy.suppression + suppressImpulse);

                    ally.aggro = std::min(100.0f, ally.aggro + aggroPerShot);
                    Color tracerCol = (currentOrder == SquadOrder::SUPPRESS) ? ORANGE : ally.color;
                    tracers.push_back({ ally.GetEyePos(), enemy.GetEyePos(), tracerCol, 0.08f });
                    ally.shootCooldown = (float)GetRandomValue(minCooldownMs, maxCooldownMs) / 100.0f;
                    break;
                }
            }
        }
    }
}

inline void UpdateEnemies(float dt, std::vector<EnemyBot>& enemies, SourcePlayer& player,
                          std::vector<SquadAlly>& allies, const std::vector<WorldBlock>& world,
                          std::vector<Tracer>& tracers) {
    Vector3 eyePos = player.GetEyePosition();

    for (auto& enemy : enemies) {
        if (!enemy.IsAlive()) continue;

        if (enemy.hp < enemy.lastHp) {
            float dmgTaken = enemy.lastHp - enemy.hp;
            enemy.suppression = std::min(100.0f, enemy.suppression + dmgTaken * 1.1f);
            enemy.lastHp = enemy.hp;
        }

        float suppressDecay = enemy.isSuppressed ? 14.0f : 18.0f;
        enemy.suppression = std::max(0.0f, enemy.suppression - suppressDecay * dt);

        if (!enemy.isSuppressed && enemy.suppression >= 45.0f) {
            enemy.isSuppressed = true;
            enemy.coverSearchTimer = 0.0f;
        } else if (enemy.isSuppressed && enemy.suppression < 20.0f) {
            enemy.isSuppressed = false;
            enemy.hasCoverPos = false;
        }

        int bestTarget = -1;
        float highestThreat = -1.0f;

        if (!player.isDowned) {
            float playerThreat = player.aggro * (HasLineOfSight(enemy.GetEyePos(), eyePos, world) ? 1.0f : 0.4f);
            highestThreat = playerThreat;
            bestTarget = 0;
        }

        for (const auto& ally : allies) {
            if (ally.isDowned) continue;
            bool canSeeAlly = HasLineOfSight(enemy.GetEyePos(), ally.GetEyePos(), world);
            float effectiveThreat = ally.aggro * (canSeeAlly ? 1.0f : 0.4f);
            if (effectiveThreat > highestThreat) {
                highestThreat = effectiveThreat;
                bestTarget = ally.id;
            }
        }

        Vector3 primaryThreatEye = (bestTarget == 0)
            ? eyePos
            : ((bestTarget > 0) ? allies[bestTarget - 1].GetEyePos() : eyePos);

        // MODO SUPRIMIDO DO INIMIGO: Busca cobertura atrás de um bloco
        if (enemy.isSuppressed) {
            enemy.coverSearchTimer -= dt;
            if (!enemy.hasCoverPos || enemy.coverSearchTimer <= 0.0f) {
                enemy.hasCoverPos = FindBestCoverPoint(enemy.pos, primaryThreatEye, world, enemy.coverPos);
                enemy.coverSearchTimer = 0.65f;
            }

            if (enemy.hasCoverPos) {
                Vector3 toCover = Vector3Subtract(enemy.coverPos, enemy.pos);
                toCover.y = 0.0f;
                float dist = Vector3Length(toCover);

                if (dist > 0.35f) {
                    Vector3 moveStep = Vector3Scale(Vector3Normalize(toCover), 4.6f * dt);
                    MoveCharacterWithCollision(enemy.pos, 0.55f, 1.25f, moveStep, dt, world);
                }
            }

            if (!HasLineOfSight(enemy.GetEyePos(), primaryThreatEye, world)) {
                enemy.currentTargetId = -1;
                continue;
            }
        } else {
            enemy.strafeTimer -= dt;
            if (enemy.strafeTimer <= 0.0f) {
                enemy.strafeDir = -enemy.strafeDir;
                enemy.strafeTimer = (float)GetRandomValue(15, 30) / 10.0f;
            }
            Vector3 strafeDelta = { enemy.strafeDir * 2.2f * dt, 0.0f, 0.0f };
            MoveCharacterWithCollision(enemy.pos, 0.55f, 1.9f, strafeDelta, dt, world);
        }

        enemy.currentTargetId = bestTarget;
        if (enemy.currentTargetId == -1) continue;

        // Pressão contínua de supressão no aliado focado sob linha de visão
        if (enemy.currentTargetId > 0) {
            SquadAlly& focusedAlly = allies[enemy.currentTargetId - 1];
            if (!focusedAlly.isDowned && HasLineOfSight(enemy.GetEyePos(), focusedAlly.GetEyePos(), world)) {
                focusedAlly.suppression = std::min(100.0f, focusedAlly.suppression + 11.0f * dt);
            }
        }

        enemy.shootTimer -= dt;
        if (enemy.shootTimer <= 0.0f) {
            Vector3 targetPos = (enemy.currentTargetId == 0)
                ? eyePos
                : allies[enemy.currentTargetId - 1].GetEyePos();

            if (HasLineOfSight(enemy.GetEyePos(), targetPos, world)) {
                tracers.push_back({ enemy.GetEyePos(), targetPos, RED, 0.12f });

                if (enemy.currentTargetId == 0) {
                    player.hp -= 10.0f;
                    if (player.hp <= 0.0f && !player.isDowned) {
                        player.hp = 0.0f;
                        player.isDowned = true;
                        player.aggro = 0.0f;
                        player.reviveProgress = 0.0f;
                    }
                } else {
                    SquadAlly& targetAlly = allies[enemy.currentTargetId - 1];
                    targetAlly.hp -= 13.0f;
                    targetAlly.lastHp = targetAlly.hp;
                    targetAlly.suppression = std::min(100.0f, targetAlly.suppression + 26.0f);

                    if (targetAlly.hp <= 0.0f && !targetAlly.isDowned) {
                        targetAlly.hp = 0.0f;
                        targetAlly.isDowned = true;
                        targetAlly.aggro = 0.0f;
                        targetAlly.reviveProgress = 0.0f;
                        targetAlly.bleedoutTimer = 25.0f;
                    }
                }
            }
            enemy.shootTimer = enemy.isSuppressed ? 1.8f : 1.0f;
        }
    }
}