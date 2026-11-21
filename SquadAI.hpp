#pragma once

#include "raylib.h"
#include "raymath.h"
#include "World.hpp"
#include "Player.hpp"
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>

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

enum class PingType {
    NONE,
    MOVE_COVER,
    FOCUS_TARGET
};

struct TacticalPing {
    PingType type = PingType::NONE;
    Vector3 worldPos = { 0.0f, 0.0f, 0.0f };
    int assignedAllyId = -1; // Aliado mais próximo enviado para a cobertura
    int targetEnemyIdx = -1; // Inimigo marcado para foco de fogo do esquadrão
    float timer = 0.0f;
    float pulseAnim = 0.0f;

    void Clear() {
        type = PingType::NONE;
        assignedAllyId = -1;
        targetEnemyIdx = -1;
        timer = 0.0f;
    }
};

struct SquadAlly {
    int id;
    std::string name;
    Vector3 pos;
    Vector3 anchorPos;
    Vector3 coverPos;
    float hp;
    float aggro;
    float fireTimer;
    float coverSearchTimer;
    Color color;

    // Estados dinâmicos (com valores padrão para compatibilidade de inicialização)
    float maxHp = 100.0f;
    float lastHp = 100.0f;
    float suppression = 0.0f;
    bool isSuppressed = false;
    bool isDowned = false;
    float bleedoutTimer = 25.0f;
    float reviveProgress = 0.0f;
    bool isBeingDragged = false;
    bool hasPingOrder = false;

    Vector3 GetEyePos() const {
        float h = isDowned ? 0.45f : (isSuppressed ? 1.05f : 1.55f);
        return { pos.x, pos.y + h, pos.z };
    }

    BoundingBox GetAABB() const {
        float h = isDowned ? 0.55f : (isSuppressed ? 1.25f : 1.8f);
        return {
            { pos.x - 0.4f, pos.y, pos.z - 0.4f },
            { pos.x + 0.4f, pos.y + h, pos.z + 0.4f }
        };
    }
};

struct EnemyBot {
    int id;
    Vector3 pos;
    Vector3 coverPos;
    float hp = 100.0f;
    float maxHp = 100.0f;
    float fireTimer = 0.0f;
    float coverSearchTimer = 0.0f;
    float suppression = 0.0f;
    bool isSuppressed = false;
    int currentTargetId = -1; // -1: nenhum, 0: jogador, 1..N: aliado id

    bool IsAlive() const { return hp > 0.0f; }

    Vector3 GetEyePos() const {
        float h = isSuppressed ? 1.05f : 1.65f;
        return { pos.x, pos.y + h, pos.z };
    }

    BoundingBox GetAABB() const {
        float h = isSuppressed ? 1.25f : 1.9f;
        return {
            { pos.x - 0.5f, pos.y, pos.z - 0.5f },
            { pos.x + 0.5f, pos.y + h, pos.z + 0.5f }
        };
    }
};

inline void ResetWave(std::vector<EnemyBot>& enemies) {
    enemies = {
        { 1, { -8.0f, 0.0f, -15.0f }, { -8.0f, 0.0f, -15.0f }, 100.0f, 100.0f, 0.5f, 0.0f, 0.0f, false, -1 },
        { 2, { -2.5f, 0.0f, -17.5f }, { -2.5f, 0.0f, -17.5f }, 100.0f, 100.0f, 1.1f, 0.5f, 0.0f, false, -1 },
        { 3, {  3.5f, 0.0f, -17.5f }, {  3.5f, 0.0f, -17.5f }, 100.0f, 100.0f, 0.8f, 1.0f, 0.0f, false, -1 },
        { 4, {  9.0f, 0.0f, -14.5f }, {  9.0f, 0.0f, -14.5f }, 100.0f, 100.0f, 1.4f, 1.5f, 0.0f, false, -1 }
    };
}

inline void MoveAllyWithCollision(SquadAlly& ally, Vector3 target, float speed, float dt, const std::vector<WorldBlock>& world) {
    Vector3 diff = Vector3Subtract(target, ally.pos);
    diff.y = 0.0f;
    float dist = Vector3Length(diff);
    if (dist < 0.08f) return;

    Vector3 dir = Vector3Scale(diff, 1.0f / dist);
    float step = std::min(dist, speed * dt);
    float r = 0.42f;

    // Eixo X
    ally.pos.x += dir.x * step;
    BoundingBox box = { { ally.pos.x - r, ally.pos.y + 0.1f, ally.pos.z - r }, { ally.pos.x + r, ally.pos.y + 1.7f, ally.pos.z + r } };
    for (const auto& b : world) {
        if (b.bounds.max.y <= ally.pos.y + 0.1f) continue;
        if (CheckOverlapStrict(box, b.bounds)) {
            ally.pos.x = (dir.x > 0.0f) ? (b.bounds.min.x - r) : (b.bounds.max.x + r);
            break;
        }
    }

    // Eixo Z
    ally.pos.z += dir.z * step;
    box = { { ally.pos.x - r, ally.pos.y + 0.1f, ally.pos.z - r }, { ally.pos.x + r, ally.pos.y + 1.7f, ally.pos.z + r } };
    for (const auto& b : world) {
        if (b.bounds.max.y <= ally.pos.y + 0.1f) continue;
        if (CheckOverlapStrict(box, b.bounds)) {
            ally.pos.z = (dir.z > 0.0f) ? (b.bounds.min.z - r) : (b.bounds.max.z + r);
            break;
        }
    }
}

inline void MoveEnemyWithCollision(EnemyBot& enemy, Vector3 target, float speed, float dt, const std::vector<WorldBlock>& world) {
    Vector3 diff = Vector3Subtract(target, enemy.pos);
    diff.y = 0.0f;
    float dist = Vector3Length(diff);
    if (dist < 0.08f) return;

    Vector3 dir = Vector3Scale(diff, 1.0f / dist);
    float step = std::min(dist, speed * dt);
    float r = 0.50f;

    enemy.pos.x += dir.x * step;
    BoundingBox box = { { enemy.pos.x - r, enemy.pos.y + 0.1f, enemy.pos.z - r }, { enemy.pos.x + r, enemy.pos.y + 1.8f, enemy.pos.z + r } };
    for (const auto& b : world) {
        if (b.bounds.max.y <= enemy.pos.y + 0.1f) continue;
        if (CheckOverlapStrict(box, b.bounds)) {
            enemy.pos.x = (dir.x > 0.0f) ? (b.bounds.min.x - r) : (b.bounds.max.x + r);
            break;
        }
    }

    enemy.pos.z += dir.z * step;
    box = { { enemy.pos.x - r, enemy.pos.y + 0.1f, enemy.pos.z - r }, { enemy.pos.x + r, enemy.pos.y + 1.8f, enemy.pos.z + r } };
    for (const auto& b : world) {
        if (b.bounds.max.y <= enemy.pos.y + 0.1f) continue;
        if (CheckOverlapStrict(box, b.bounds)) {
            enemy.pos.z = (dir.z > 0.0f) ? (b.bounds.min.z - r) : (b.bounds.max.z + r);
            break;
        }
    }
}

inline Vector3 FindProtectedCoverPos(Vector3 origin, Vector3 threatPos, const std::vector<WorldBlock>& world) {
    Vector3 bestSpot = origin;
    float bestScore = 99999.0f;

    for (const auto& b : world) {
        if (b.size.y < 1.1f || b.size.x > 16.0f || b.size.z > 16.0f) continue;

        Vector3 awayDir = Vector3Subtract(b.pos, threatPos);
        awayDir.y = 0.0f;
        if (Vector3Length(awayDir) < 0.01f) awayDir = { 0.0f, 0.0f, 1.0f };
        awayDir = Vector3Normalize(awayDir);

        float offset = std::max(b.size.x, b.size.z) * 0.5f + 0.95f;
        Vector3 candidate = {
            std::clamp(b.pos.x + awayDir.x * offset, -22.0f, 22.0f),
            0.0f,
            std::clamp(b.pos.z + awayDir.z * offset, -22.0f, 22.0f)
        };

        Vector3 candidateEye = { candidate.x, 1.1f, candidate.z };
        Vector3 threatEye = { threatPos.x, 1.5f, threatPos.z };

        if (!HasLineOfSight(candidateEye, threatEye, world)) {
            float dist = Vector3Distance(origin, candidate);
            if (dist < bestScore) {
                bestScore = dist;
                bestSpot = candidate;
            }
        }
    }
    return bestSpot;
}

// =============================================================================
// PING TÁTICO CONTEXTUAL (MOUSE_BUTTON_MIDDLE)
// =============================================================================
inline bool ProcessTacticalPing(Vector3 eyePos, Vector3 forward,
                                const std::vector<WorldBlock>& world,
                                std::vector<SquadAlly>& allies,
                                const std::vector<EnemyBot>& enemies,
                                TacticalPing& ping) {
    Ray ray = { eyePos, forward };
    float closestDist = 90.0f;
    int hitEnemyIdx = -1;
    int hitBlockIdx = -1;
    Vector3 hitPoint = { 0.0f, 0.0f, 0.0f };

    // 1. Verifica se o jogador mirou em algum inimigo
    for (size_t i = 0; i < enemies.size(); i++) {
        if (!enemies[i].IsAlive()) continue;
        RayCollision col = GetRayCollisionBox(ray, enemies[i].GetAABB());
        if (col.hit && col.distance < closestDist) {
            closestDist = col.distance;
            hitEnemyIdx = (int)i;
            hitPoint = col.point;
        }
    }

    // 2. Verifica se mirou em algum bloco do cenário (cobertura ou chão)
    if (hitEnemyIdx == -1) {
        for (size_t b = 0; b < world.size(); b++) {
            RayCollision col = GetRayCollisionBox(ray, world[b].bounds);
            if (col.hit && col.distance < closestDist) {
                closestDist = col.distance;
                hitBlockIdx = (int)b;
                hitPoint = col.point;
            }
        }
    }

    if (hitEnemyIdx == -1 && hitBlockIdx == -1) return false;

    // Reseta ordens de ping individuais anteriores
    for (auto& ally : allies) {
        ally.hasPingOrder = false;
    }

    // Caso A: Ping em Inimigo -> Esquadrão foca fogo nesse alvo
    if (hitEnemyIdx != -1) {
        ping.type = PingType::FOCUS_TARGET;
        ping.targetEnemyIdx = hitEnemyIdx;
        ping.assignedAllyId = -1;
        ping.worldPos = enemies[hitEnemyIdx].pos;
        ping.timer = 12.0f;
        ping.pulseAnim = 0.0f;
        return true;
    }

    // Caso B: Ping em Bloco/Cobertura -> Aliado mais próximo corre para trás daquela cobertura
    if (allies.empty()) return false;

    const WorldBlock& block = world[hitBlockIdx];
    Vector3 coverSpot = { std::clamp(hitPoint.x, -22.0f, 22.0f), 0.0f, std::clamp(hitPoint.z, -22.0f, 22.0f) };

    // Se o bloco for uma barreira vertical (não o chão nem muro externo gigante), posiciona atrás dela
    if (block.size.y >= 1.1f && block.size.x <= 18.0f && block.size.z <= 18.0f) {
        Vector3 threatCenter = { 0.0f, 1.5f, -15.0f };
        int aliveEnemies = 0;
        Vector3 sumEnemies = { 0.0f, 0.0f, 0.0f };
        for (const auto& e : enemies) {
            if (e.IsAlive()) {
                sumEnemies = Vector3Add(sumEnemies, e.pos);
                aliveEnemies++;
            }
        }
        if (aliveEnemies > 0) {
            threatCenter = Vector3Scale(sumEnemies, 1.0f / (float)aliveEnemies);
        } else {
            // Sem inimigos ativos (ex.: pista de movimento), posiciona no lado oposto ao olhar do jogador
            threatCenter = Vector3Add(block.pos, Vector3Scale(forward, 5.0f));
        }

        Vector3 awayDir = Vector3Subtract(block.pos, threatCenter);
        awayDir.y = 0.0f;
        if (Vector3Length(awayDir) < 0.01f) awayDir = Vector3Negate(forward);
        awayDir.y = 0.0f;
        awayDir = Vector3Normalize(awayDir);

        // Escolhe a face da caixa mais protegida contra a ameaça
        float offsetX = (fabsf(awayDir.x) >= fabsf(awayDir.z))
            ? ((awayDir.x >= 0.0f ? 1.0f : -1.0f) * (block.size.x * 0.5f + 0.90f))
            : std::clamp(hitPoint.x - block.pos.x, -block.size.x * 0.35f, block.size.x * 0.35f);

        float offsetZ = (fabsf(awayDir.z) > fabsf(awayDir.x))
            ? ((awayDir.z >= 0.0f ? 1.0f : -1.0f) * (block.size.z * 0.5f + 0.90f))
            : std::clamp(hitPoint.z - block.pos.z, -block.size.z * 0.35f, block.size.z * 0.35f);

        coverSpot = {
            std::clamp(block.pos.x + offsetX, -22.0f, 22.0f),
            0.0f,
            std::clamp(block.pos.z + offsetZ, -22.0f, 22.0f)
        };
    }

    // Seleciona o aliado vivo mais próximo do ponto marcado
    int bestAllyIdx = -1;
    float minDist = 99999.0f;
    for (size_t i = 0; i < allies.size(); i++) {
        if (allies[i].isDowned) continue;
        float d = Vector3Distance(allies[i].pos, coverSpot);
        if (d < minDist) {
            minDist = d;
            bestAllyIdx = (int)i;
        }
    }

    if (bestAllyIdx == -1) return false;

    allies[bestAllyIdx].anchorPos = coverSpot;
    allies[bestAllyIdx].coverPos = coverSpot;
    allies[bestAllyIdx].hasPingOrder = true;

    ping.type = PingType::MOVE_COVER;
    ping.worldPos = coverSpot;
    ping.assignedAllyId = allies[bestAllyIdx].id;
    ping.targetEnemyIdx = -1;
    ping.timer = 12.0f;
    ping.pulseAnim = 0.0f;
    return true;
}

inline void DrawTacticalPing3D(const TacticalPing& ping, const std::vector<SquadAlly>& allies, const std::vector<EnemyBot>& enemies) {
    if (ping.type == PingType::NONE) return;

    float pulse = 0.55f + sinf(ping.pulseAnim * 6.0f) * 0.20f;

    if (ping.type == PingType::MOVE_COVER) {
        Vector3 p = ping.worldPos;
        DrawCircle3D({ p.x, 0.05f, p.z }, pulse, { 1.0f, 0.0f, 0.0f }, 90.0f, SKYBLUE);
        DrawCircle3D({ p.x, 0.06f, p.z }, 0.25f, { 1.0f, 0.0f, 0.0f }, 90.0f, WHITE);
        DrawLine3D({ p.x, 0.05f, p.z }, { p.x, 1.85f, p.z }, Fade(SKYBLUE, 0.85f));
        DrawSphere({ p.x, 1.90f, p.z }, 0.12f, SKYBLUE);

        // Linha guia pontilhada/sutil entre o aliado designado e a cobertura
        for (const auto& ally : allies) {
            if (ally.id == ping.assignedAllyId && !ally.isDowned) {
                DrawLine3D({ ally.pos.x, 0.15f, ally.pos.z }, { p.x, 0.15f, p.z }, Fade(SKYBLUE, 0.45f));
                break;
            }
        }
    } else if (ping.type == PingType::FOCUS_TARGET) {
        if (ping.targetEnemyIdx >= 0 && ping.targetEnemyIdx < (int)enemies.size() && enemies[ping.targetEnemyIdx].IsAlive()) {
            Vector3 ep = enemies[ping.targetEnemyIdx].pos;
            DrawCircle3D({ ep.x, 0.06f, ep.z }, pulse + 0.25f, { 1.0f, 0.0f, 0.0f }, 90.0f, RED);
            DrawSphere({ ep.x, ep.y + 2.45f + sinf(ping.pulseAnim * 5.0f) * 0.12f, ep.z }, 0.15f, YELLOW);
        }
    }
}

// =============================================================================
// ATUALIZAÇÃO DOS ALIADOS (SEM ESPECIALIZAÇÃO FIXA - LIVRES PARA MULTIPLAYER)
// =============================================================================
inline void UpdateAllies(float dt, std::vector<SquadAlly>& allies, std::vector<EnemyBot>& enemies,
                         SourcePlayer& player, SquadOrder currentOrder,
                         const std::vector<WorldBlock>& world, std::vector<Tracer>& tracers,
                         TacticalPing& ping) {
    // Atualiza ciclo de vida do Ping Tático
    if (ping.type != PingType::NONE) {
        ping.timer -= dt;
        ping.pulseAnim += dt;

        if (ping.timer <= 0.0f) {
            for (auto& a : allies) a.hasPingOrder = false;
            ping.Clear();
        } else if (ping.type == PingType::FOCUS_TARGET) {
            if (ping.targetEnemyIdx < 0 || ping.targetEnemyIdx >= (int)enemies.size() || !enemies[ping.targetEnemyIdx].IsAlive()) {
                ping.Clear();
            } else {
                ping.worldPos = enemies[ping.targetEnemyIdx].pos;
            }
        } else if (ping.type == PingType::MOVE_COVER) {
            bool allyValid = false;
            for (const auto& a : allies) {
                if (a.id == ping.assignedAllyId && !a.isDowned && a.hasPingOrder) {
                    allyValid = true;
                    break;
                }
            }
            if (!allyValid) ping.Clear();
        }
    }

    // Verifica se algum aliado livre deve resgatar o jogador caído
    int playerRescuerIdx = -1;
    if (player.isDowned) {
        float bestDist = 99999.0f;
        for (size_t i = 0; i < allies.size(); i++) {
            if (allies[i].isDowned || allies[i].isSuppressed) continue;
            float d = Vector3Distance(allies[i].pos, player.pos);
            if (d < bestDist) {
                bestDist = d;
                playerRescuerIdx = (int)i;
            }
        }
    }

    for (size_t i = 0; i < allies.size(); i++) {
        SquadAlly& ally = allies[i];

        // Estado CAÍDO (Sangramento)
        if (ally.isDowned) {
            ally.aggro = 0.0f;
            ally.isSuppressed = false;
            ally.hasPingOrder = false;
            ally.bleedoutTimer -= dt;
            if (ally.bleedoutTimer <= 0.0f) {
                // Respawn tático recuado após sangramento no protótipo
                ally.isDowned = false;
                ally.hp = 80.0f;
                ally.lastHp = 80.0f;
                ally.bleedoutTimer = 25.0f;
                ally.pos = { -5.0f + (float)i * 5.0f, 0.0f, 12.0f };
                ally.anchorPos = ally.pos;
            }
            continue;
        }

        // Detecta dano recebido para alimentar supressão e aggro
        if (ally.hp < ally.lastHp) {
            float dmgTaken = ally.lastHp - ally.hp;
            ally.suppression = std::min(100.0f, ally.suppression + dmgTaken * 1.6f);
            ally.lastHp = ally.hp;
        }

        if (ally.hp <= 0.0f) {
            ally.hp = 0.0f;
            ally.isDowned = true;
            ally.bleedoutTimer = 25.0f;
            ally.reviveProgress = 0.0f;
            ally.hasPingOrder = false;
            continue;
        }

        // Decaimento da supressão
        ally.suppression = std::max(0.0f, ally.suppression - 16.0f * dt);
        ally.isSuppressed = (ally.suppression >= 65.0f);
        ally.aggro = std::max(0.0f, ally.aggro - 7.5f * dt);

        // Resgate automático do jogador caído
        if (player.isDowned && (int)i == playerRescuerIdx) {
            float dPlayer = Vector3Distance(ally.pos, player.pos);
            if (dPlayer > 1.7f) {
                MoveAllyWithCollision(ally, player.pos, 5.6f, dt, world);
            } else {
                player.reviveProgress += dt * 0.45f;
                if (player.reviveProgress >= 1.0f) {
                    player.isDowned = false;
                    player.hp = 75.0f;
                    player.reviveProgress = 0.0f;
                }
            }
            continue;
        }

        // Escolha do alvo inimigo (prioriza inimigo marcado com Ping Tático!)
        int targetIdx = -1;
        if (ping.type == PingType::FOCUS_TARGET &&
            ping.targetEnemyIdx >= 0 && ping.targetEnemyIdx < (int)enemies.size() &&
            enemies[ping.targetEnemyIdx].IsAlive()) {
            targetIdx = ping.targetEnemyIdx;
        } else {
            float bestEnemyDist = 99999.0f;
            for (size_t e = 0; e < enemies.size(); e++) {
                if (!enemies[e].IsAlive()) continue;
                float d = Vector3Distance(ally.pos, enemies[e].pos);
                bool los = HasLineOfSight(ally.GetEyePos(), enemies[e].GetEyePos(), world);
                float score = d - (los ? 8.0f : 0.0f);
                if (score < bestEnemyDist) {
                    bestEnemyDist = score;
                    targetIdx = (int)e;
                }
            }
        }

        // Movimentação Tática
        if (ally.isSuppressed) {
            // Sob supressão pesada, busca cobertura próxima
            ally.coverSearchTimer -= dt;
            if (ally.coverSearchTimer <= 0.0f) {
                Vector3 threat = (targetIdx != -1) ? enemies[targetIdx].pos : Vector3{ 0.0f, 0.0f, -15.0f };
                ally.coverPos = FindProtectedCoverPos(ally.pos, threat, world);
                ally.coverSearchTimer = 1.2f;
            }
            MoveAllyWithCollision(ally, ally.coverPos, 4.8f, dt, world);
        } else if (ally.hasPingOrder) {
            // Ping de Cobertura: corre diretamente até o bloco marcado pelo jogador
            MoveAllyWithCollision(ally, ally.anchorPos, 6.2f, dt, world);
        } else if (currentOrder == SquadOrder::ADVANCE && targetIdx != -1) {
            Vector3 toEnemy = Vector3Subtract(enemies[targetIdx].pos, ally.pos);
            toEnemy.y = 0.0f;
            float dist = Vector3Length(toEnemy);
            if (dist > 7.5f) {
                MoveAllyWithCollision(ally, enemies[targetIdx].pos, 4.6f, dt, world);
            }
        } else if (currentOrder == SquadOrder::HOLD || currentOrder == SquadOrder::SUPPRESS) {
            // Se há um inimigo marcado por Ping e o aliado não tem visão limpa, ajusta levemente o ângulo
            if (ping.type == PingType::FOCUS_TARGET && targetIdx != -1 &&
                !HasLineOfSight(ally.GetEyePos(), enemies[targetIdx].GetEyePos(), world)) {
                MoveAllyWithCollision(ally, enemies[targetIdx].pos, 3.8f, dt, world);
            } else {
                MoveAllyWithCollision(ally, ally.anchorPos, 4.2f, dt, world);
            }
        }

        // Disparo contra inimigos
        ally.fireTimer -= dt;
        if (!ally.isSuppressed && targetIdx != -1 && ally.fireTimer <= 0.0f) {
            Vector3 from = ally.GetEyePos();
            Vector3 to = enemies[targetIdx].GetEyePos();

            if (HasLineOfSight(from, to, world)) {
                bool suppressMode = (currentOrder == SquadOrder::SUPPRESS);
                ally.fireTimer = suppressMode ? (0.28f + (float)GetRandomValue(0, 12) / 100.0f)
                                              : (0.55f + (float)GetRandomValue(0, 25) / 100.0f);

                // Pequena dispersão realista
                Vector3 aimOffset = {
                    (float)GetRandomValue(-18, 18) / 100.0f,
                    (float)GetRandomValue(-15, 15) / 100.0f,
                    (float)GetRandomValue(-18, 18) / 100.0f
                };
                Vector3 endHit = Vector3Add(to, aimOffset);
                tracers.push_back({ from, endHit, ally.color, 0.08f });

                float hitChance = suppressMode ? 0.55f : 0.72f;
                if ((float)GetRandomValue(1, 100) / 100.0f <= hitChance) {
                    float dmg = suppressMode ? 11.0f : 16.0f;
                    enemies[targetIdx].hp -= dmg;
                    enemies[targetIdx].suppression = std::min(100.0f, enemies[targetIdx].suppression + (suppressMode ? 24.0f : 12.0f));
                }

                ally.aggro = std::min(100.0f, ally.aggro + (suppressMode ? 14.0f : 7.0f));
            }
        }
    }
}

// =============================================================================
// ATUALIZAÇÃO DOS INIMIGOS (AGGRO BILATERAL + SUPRESSÃO + COBERTURA)
// =============================================================================
inline void UpdateEnemies(float dt, std::vector<EnemyBot>& enemies, SourcePlayer& player,
                          std::vector<SquadAlly>& allies, const std::vector<WorldBlock>& world,
                          std::vector<Tracer>& tracers) {
    for (auto& enemy : enemies) {
        if (!enemy.IsAlive()) continue;

        enemy.suppression = std::max(0.0f, enemy.suppression - 14.0f * dt);
        enemy.isSuppressed = (enemy.suppression >= 65.0f);

        // Seleção de alvo baseada em Aggro + Linha de Visão
        int chosenId = -1;
        float highestThreat = -1.0f;

        if (!player.isDowned) {
            bool losPlayer = HasLineOfSight(enemy.GetEyePos(), player.GetEyePosition(), world);
            float distPlayer = Vector3Distance(enemy.pos, player.pos);
            float threat = player.aggro + (losPlayer ? 25.0f : -15.0f) - distPlayer * 0.6f;
            if (threat > highestThreat) {
                highestThreat = threat;
                chosenId = 0;
            }
        }

        for (const auto& ally : allies) {
            if (ally.isDowned) continue;
            bool losAlly = HasLineOfSight(enemy.GetEyePos(), ally.GetEyePos(), world);
            float distAlly = Vector3Distance(enemy.pos, ally.pos);
            float threat = ally.aggro + (losAlly ? 25.0f : -15.0f) - distAlly * 0.6f;
            if (threat > highestThreat) {
                highestThreat = threat;
                chosenId = ally.id;
            }
        }

        enemy.currentTargetId = chosenId;

        Vector3 targetPos = { 0.0f, 0.0f, 8.0f };
        Vector3 targetEye = { 0.0f, 1.5f, 8.0f };
        if (chosenId == 0) {
            targetPos = player.pos;
            targetEye = player.GetEyePosition();
        } else if (chosenId > 0) {
            targetPos = allies[chosenId - 1].pos;
            targetEye = allies[chosenId - 1].GetEyePos();
        }

        // Movimentação: foge para cobertura quando suprimido ou avança cautelosamente
        if (enemy.isSuppressed) {
            enemy.coverSearchTimer -= dt;
            if (enemy.coverSearchTimer <= 0.0f) {
                enemy.coverPos = FindProtectedCoverPos(enemy.pos, targetPos, world);
                enemy.coverSearchTimer = 1.1f;
            }
            MoveEnemyWithCollision(enemy, enemy.coverPos, 4.5f, dt, world);
        } else if (chosenId != -1) {
            float dist = Vector3Distance(enemy.pos, targetPos);
            if (dist > 14.0f) {
                MoveEnemyWithCollision(enemy, targetPos, 3.2f, dt, world);
            }
        }

        // Disparo do inimigo
        enemy.fireTimer -= dt;
        if (!enemy.isSuppressed && chosenId != -1 && enemy.fireTimer <= 0.0f) {
            Vector3 from = enemy.GetEyePos();
            if (HasLineOfSight(from, targetEye, world)) {
                enemy.fireTimer = 0.75f + (float)GetRandomValue(0, 35) / 100.0f;

                Vector3 spread = {
                    (float)GetRandomValue(-26, 26) / 100.0f,
                    (float)GetRandomValue(-20, 20) / 100.0f,
                    (float)GetRandomValue(-26, 26) / 100.0f
                };
                tracers.push_back({ from, Vector3Add(targetEye, spread), RED, 0.09f });

                if ((float)GetRandomValue(1, 100) / 100.0f <= 0.58f) {
                    if (chosenId == 0 && !player.isDowned) {
                        player.hp -= 12.0f;
                        if (player.hp <= 0.0f) {
                            player.hp = 0.0f;
                            player.isDowned = true;
                            player.reviveProgress = 0.0f;
                        }
                    } else if (chosenId > 0) {
                        allies[chosenId - 1].hp -= 13.0f;
                    }
                }
            }
        }
    }
}