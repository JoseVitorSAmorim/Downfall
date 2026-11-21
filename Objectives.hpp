#pragma once

#include "raylib.h"
#include "raymath.h"
#include "World.hpp"
#include "Player.hpp"
#include "Weapon.hpp"
#include "SquadAI.hpp"
#include "Audio.hpp"
#include "AssetManager.hpp"
#include <vector>
#include <string>
#include <algorithm>
#include <cmath>

enum class MissionState {
    SANDBOX,
    IN_PROGRESS,
    VICTORY,
    DEFEAT
};

enum class ObjectivePhase {
    HACK_TERMINALS,
    CLEAR_HOSTILES,
    EXTRACT_SQUAD
};

struct ExplosiveBarrel {
    Vector3 pos;
    float hp = 35.0f;
    bool exploded = false;

    BoundingBox GetAABB() const {
        return {
            { pos.x - 0.42f, pos.y, pos.z - 0.42f },
            { pos.x + 0.42f, pos.y + 1.25f, pos.z + 0.42f }
        };
    }
};

struct AmmoCrate {
    Vector3 pos;
    float cooldownTimer = 0.0f;
    float maxCooldown = 12.0f;

    bool IsReady() const { return cooldownTimer <= 0.0f; }
};

struct HackTerminal {
    int id;
    std::string label;
    Vector3 pos;
    float progress = 0.0f;
    bool isHacked = false;
};

struct ExtractionZone {
    Vector3 pos = { 0.0f, 0.0f, 16.5f };
    float radius = 3.2f;
    bool isActive = false;
};

struct MissionStats {
    float elapsedTime = 0.0f;
    int enemiesEliminated = 0;
    int headshotsLanded = 0;
    int barrelsDetonated = 0;
    int terminalsHacked = 0;
};

class MissionManager {
public:
    MissionState state = MissionState::SANDBOX;
    ObjectivePhase phase = ObjectivePhase::HACK_TERMINALS;
    std::string defeatReason = "";

    std::vector<ExplosiveBarrel> barrels;
    std::vector<AmmoCrate> crates;
    std::vector<HackTerminal> terminals;
    ExtractionZone extraction;
    MissionStats stats;

    float playerDownedTimer = 0.0f;
    float pulseTimer = 0.0f;
    bool reinforcementsSpawned = false;

    void SetupForMode(bool isCampaignMission, bool isCombatSandbox) {
        barrels.clear();
        crates.clear();
        terminals.clear();
        stats = MissionStats();
        playerDownedTimer = 0.0f;
        pulseTimer = 0.0f;
        reinforcementsSpawned = false;
        defeatReason = "";

        if (!isCampaignMission && !isCombatSandbox) {
            state = MissionState::SANDBOX;
            return;
        }

        crates = {
            { { -7.5f, 0.0f,  9.5f }, 0.0f, 12.0f },
            { {  7.5f, 0.0f,  9.5f }, 0.0f, 12.0f }
        };

        barrels = {
            { { -3.2f, 0.0f, -13.5f }, 35.0f, false },
            { {  4.2f, 0.0f, -13.5f }, 35.0f, false },
            { { -9.5f, 0.0f,  -8.0f }, 35.0f, false },
            { {  9.5f, 0.0f,  -8.0f }, 35.0f, false }
        };

        if (isCampaignMission) {
            state = MissionState::IN_PROGRESS;
            phase = ObjectivePhase::HACK_TERMINALS;

            terminals = {
                { 1, "Terminal Alpha (Oeste)", { -13.0f, 0.0f, -4.8f }, 0.0f, false },
                { 2, "Terminal Bravo (Leste)", {  13.0f, 0.0f, -4.8f }, 0.0f, false }
            };

            extraction.pos = { 0.0f, 0.0f, 16.5f };
            extraction.radius = 3.2f;
            extraction.isActive = false;
        } else {
            state = MissionState::SANDBOX;
        }
    }

    int CheckBarrelShot(Ray ray, float maxDist) {
        int hitIdx = -1;
        float closest = maxDist;
        for (size_t i = 0; i < barrels.size(); i++) {
            if (barrels[i].exploded) continue;
            RayCollision col = GetRayCollisionBox(ray, barrels[i].GetAABB());
            if (col.hit && col.distance < closest) {
                closest = col.distance;
                hitIdx = (int)i;
            }
        }
        return hitIdx;
    }

    void DamageBarrel(int idx, float amount, std::vector<ExplosionEffect>& explosions) {
        if (idx < 0 || idx >= (int)barrels.size()) return;
        ExplosiveBarrel& b = barrels[idx];
        if (b.exploded) return;

        b.hp -= amount;
        if (b.hp <= 0.0f) {
            b.exploded = true;
            stats.barrelsDetonated++;
            Vector3 boomCenter = { b.pos.x, b.pos.y + 0.65f, b.pos.z };
            explosions.push_back({ boomCenter, false, 0.50f, 0.50f, 6.4f });
        }
    }

    void CheckExplosionChain(const ExplosionEffect& exp, std::vector<ExplosionEffect>& explosions) {
        if (exp.isFlashbang) return;
        for (size_t i = 0; i < barrels.size(); i++) {
            if (barrels[i].exploded) continue;
            Vector3 center = { barrels[i].pos.x, barrels[i].pos.y + 0.65f, barrels[i].pos.z };
            if (Vector3Distance(center, exp.pos) <= exp.maxRadius) {
                DamageBarrel((int)i, 100.0f, explosions);
            }
        }
    }

    void Update(float dt, SourcePlayer& player, ViewmodelWeapon& weapon,
                std::vector<SquadAlly>& allies, std::vector<EnemyBot>& enemies,
                bool hasNearbyDownedAlly, ProceduralAudio& audio) {
        pulseTimer += dt;

        for (auto& crate : crates) {
            if (crate.cooldownTimer > 0.0f) {
                crate.cooldownTimer = std::max(0.0f, crate.cooldownTimer - dt);
            }
        }

        if (state == MissionState::VICTORY || state == MissionState::DEFEAT) {
            return;
        }

        if (state == MissionState::IN_PROGRESS) {
            stats.elapsedTime += dt;
        }

        if (!player.isDowned && !hasNearbyDownedAlly && IsKeyPressed(KEY_F)) {
            for (auto& crate : crates) {
                if (crate.IsReady() && Vector3Distance(player.pos, crate.pos) <= 2.6f) {
                    for (size_t w = 0; w < (size_t)WeaponId::COUNT; w++) {
                        WeaponId wid = (WeaponId)w;
                        const WeaponStats& st = GetWeaponStats(wid);
                        if (!st.isMelee) {
                            weapon.ammo[w] = st.magSize;
                        }
                    }
                    weapon.isReloading = false;
                    weapon.reloadTimer = 0.0f;
                    player.hp = std::min(100.0f, player.hp + 25.0f);
                    crate.cooldownTimer = crate.maxCooldown;
                    audio.PlayReload();
                    break;
                }
            }
        }

        if (state != MissionState::IN_PROGRESS) return;

        bool anyAllyStanding = false;
        for (const auto& a : allies) {
            if (!a.isDowned) {
                anyAllyStanding = true;
                break;
            }
        }

        if (player.isDowned) {
            playerDownedTimer += dt;
            if (!anyAllyStanding && !allies.empty()) {
                state = MissionState::DEFEAT;
                defeatReason = "TODO O ESQUADRAO FOI ABATIDO EM COMBATE";
                audio.PlayDownedAlert();
                return;
            }
            if (playerDownedTimer >= 25.0f) {
                state = MissionState::DEFEAT;
                defeatReason = "VOCE SANGROU SEM RECEBER RESGATE A TEMPO";
                audio.PlayDownedAlert();
                return;
            }
        } else {
            playerDownedTimer = 0.0f;
        }

        if (phase == ObjectivePhase::HACK_TERMINALS) {
            if (!player.isDowned && !hasNearbyDownedAlly && IsKeyDown(KEY_F)) {
                for (auto& term : terminals) {
                    if (term.isHacked) continue;
                    if (Vector3Distance(player.pos, term.pos) <= 2.7f) {
                        term.progress += dt * 0.30f;
                        player.aggro = std::min(100.0f, player.aggro + 14.0f * dt);

                        if (term.progress >= 1.0f) {
                            term.progress = 1.0f;
                            term.isHacked = true;
                            stats.terminalsHacked++;
                            audio.PlayRevive();
                        }
                        break;
                    }
                }
            }

            bool allHacked = true;
            for (const auto& term : terminals) {
                if (!term.isHacked) {
                    allHacked = false;
                    break;
                }
            }

            if (allHacked) {
                phase = ObjectivePhase::CLEAR_HOSTILES;
                if (!reinforcementsSpawned) {
                    reinforcementsSpawned = true;
                    int nextId = (int)enemies.size() + 1;
                    enemies.push_back({ nextId,     { -11.0f, 0.0f, -19.0f }, { -11.0f, 0.0f, -19.0f }, 100.0f, 100.0f, 0.6f, 0.0f, 0.0f, false, -1 });
                    enemies.push_back({ nextId + 1, {  11.0f, 0.0f, -19.0f }, {  11.0f, 0.0f, -19.0f }, 100.0f, 100.0f, 0.9f, 0.4f, 0.0f, false, -1 });
                    audio.PlayOrder();
                }
            }
        } else if (phase == ObjectivePhase::CLEAR_HOSTILES) {
            int aliveCount = 0;
            for (const auto& e : enemies) {
                if (e.IsAlive()) aliveCount++;
            }
            if (aliveCount == 0) {
                phase = ObjectivePhase::EXTRACT_SQUAD;
                extraction.isActive = true;
                audio.PlayRevive();
            }
        } else if (phase == ObjectivePhase::EXTRACT_SQUAD) {
            if (!player.isDowned && Vector3Distance(player.pos, extraction.pos) <= extraction.radius) {
                state = MissionState::VICTORY;
                audio.PlayRevive();
            }
        }
    }

    void Draw3D(const AssetManager& assets) const {
        // Barris Explosivos (.glb ou cubos)
        for (const auto& b : barrels) {
            if (b.exploded) continue;
            if (assets.HasPropModel(PropAssetId::BARREL)) {
                assets.DrawPropModel(PropAssetId::BARREL, b.pos, 0.0f, 1.0f, WHITE);
            } else {
                DrawCube({ b.pos.x, b.pos.y + 0.625f, b.pos.z }, 0.78f, 1.25f, 0.78f, { 195, 42, 38, 255 });
                DrawCubeWires({ b.pos.x, b.pos.y + 0.625f, b.pos.z }, 0.78f, 1.25f, 0.78f, BLACK);
                DrawCube({ b.pos.x, b.pos.y + 0.65f, b.pos.z }, 0.81f, 0.22f, 0.81f, GOLD);
            }
        }

        // Caixas de Suprimento (.glb ou cubos)
        for (const auto& c : crates) {
            if (assets.HasPropModel(PropAssetId::CRATE)) {
                Color tint = c.IsReady() ? WHITE : Fade(GRAY, 0.65f);
                assets.DrawPropModel(PropAssetId::CRATE, c.pos, 0.0f, 1.0f, tint);
            } else {
                Color baseCol = c.IsReady() ? Color{ 58, 92, 58, 255 } : Color{ 65, 68, 72, 255 };
                Color trimCol = c.IsReady() ? LIME : GRAY;
                DrawCube({ c.pos.x, c.pos.y + 0.40f, c.pos.z }, 1.15f, 0.80f, 0.75f, baseCol);
                DrawCubeWires({ c.pos.x, c.pos.y + 0.40f, c.pos.z }, 1.15f, 0.80f, 0.75f, BLACK);
                DrawCube({ c.pos.x, c.pos.y + 0.82f, c.pos.z }, 0.65f, 0.06f, 0.35f, trimCol);
            }
        }

        // Terminais de Hack (.glb ou cubos)
        for (const auto& t : terminals) {
            Color screenCol = t.isHacked ? GREEN : SKYBLUE;
            if (assets.HasPropModel(PropAssetId::TERMINAL)) {
                assets.DrawPropModel(PropAssetId::TERMINAL, t.pos, 0.0f, 1.0f, WHITE);
            } else {
                DrawCube({ t.pos.x, t.pos.y + 0.65f, t.pos.z }, 0.75f, 1.30f, 0.55f, { 35, 40, 48, 255 });
                DrawCubeWires({ t.pos.x, t.pos.y + 0.65f, t.pos.z }, 0.75f, 1.30f, 0.55f, screenCol);
                DrawCube({ t.pos.x, t.pos.y + 1.05f, t.pos.z + 0.28f }, 0.52f, 0.36f, 0.05f, screenCol);
            }

            if (!t.isHacked) {
                float ringR = 0.85f + sinf(pulseTimer * 4.0f) * 0.12f;
                DrawCircle3D({ t.pos.x, 0.04f, t.pos.z }, ringR, { 1.0f, 0.0f, 0.0f }, 90.0f, Fade(SKYBLUE, 0.65f));
            }
        }

        // Zona de Extração
        if (extraction.isActive) {
            float pulse = extraction.radius + sinf(pulseTimer * 4.5f) * 0.22f;
            DrawCircle3D({ extraction.pos.x, 0.05f, extraction.pos.z }, pulse, { 1.0f, 0.0f, 0.0f }, 90.0f, GREEN);
            DrawCircle3D({ extraction.pos.x, 0.06f, extraction.pos.z }, extraction.radius * 0.65f, { 1.0f, 0.0f, 0.0f }, 90.0f, LIME);
            DrawLine3D({ extraction.pos.x, 0.05f, extraction.pos.z }, { extraction.pos.x, 4.5f, extraction.pos.z }, Fade(GREEN, 0.55f));
        }
    }

    void DrawHUD(int screenWidth, int screenHeight, const Camera3D& camera,
                 const SourcePlayer& player, bool hasNearbyDownedAlly) const {
        Vector3 eyePos = player.GetEyePosition();
        Vector3 forward = player.GetForward3D();

        for (const auto& c : crates) {
            Vector3 top = { c.pos.x, c.pos.y + 1.25f, c.pos.z };
            if (Vector3DotProduct(forward, Vector3Subtract(top, eyePos)) <= 0.0f) continue;
            Vector2 sc = GetWorldToScreen(top, camera);
            if (sc.x <= 0 || sc.x >= screenWidth || sc.y <= 0 || sc.y >= screenHeight) continue;

            if (c.IsReady()) {
                const char* label = "SUPRIMENTOS";
                DrawText(label, (int)sc.x - MeasureText(label, 13) / 2, (int)sc.y - 14, 13, LIME);
                if (!player.isDowned && !hasNearbyDownedAlly && Vector3Distance(player.pos, c.pos) <= 2.6f) {
                    const char* prompt = "[F] REABASTECER MUNICAO E HP";
                    DrawText(prompt, (int)sc.x - MeasureText(prompt, 14) / 2, (int)sc.y + 4, 14, YELLOW);
                }
            } else {
                const char* cdTxt = TextFormat("RECARGA: %.0fs", c.cooldownTimer);
                DrawText(cdTxt, (int)sc.x - MeasureText(cdTxt, 12) / 2, (int)sc.y - 10, 12, GRAY);
            }
        }

        for (const auto& t : terminals) {
            Vector3 top = { t.pos.x, t.pos.y + 1.75f, t.pos.z };
            if (Vector3DotProduct(forward, Vector3Subtract(top, eyePos)) <= 0.0f) continue;
            Vector2 sc = GetWorldToScreen(top, camera);
            if (sc.x <= 0 || sc.x >= screenWidth || sc.y <= 0 || sc.y >= screenHeight) continue;

            const char* status = t.isHacked ? TextFormat("%s [HACKEADO]", t.label.c_str()) : t.label.c_str();
            DrawText(status, (int)sc.x - MeasureText(status, 13) / 2, (int)sc.y - 24, 13, t.isHacked ? GREEN : SKYBLUE);

            if (!t.isHacked && state == MissionState::IN_PROGRESS) {
                if (!player.isDowned && !hasNearbyDownedAlly && Vector3Distance(player.pos, t.pos) <= 2.7f) {
                    const char* prompt = "[SEGURE F] HACKEAR TERMINAL";
                    DrawText(prompt, (int)sc.x - MeasureText(prompt, 14) / 2, (int)sc.y - 6, 14, YELLOW);
                }
                if (t.progress > 0.0f) {
                    DrawRectangle((int)sc.x - 45, (int)sc.y + 14, 90, 10, BLACK);
                    DrawRectangle((int)sc.x - 43, (int)sc.y + 16, (int)(86 * t.progress), 6, SKYBLUE);
                }
            }
        }

        if (extraction.isActive) {
            Vector3 top = { extraction.pos.x, 2.2f, extraction.pos.z };
            if (Vector3DotProduct(forward, Vector3Subtract(top, eyePos)) > 0.0f) {
                Vector2 sc = GetWorldToScreen(top, camera);
                if (sc.x > 0 && sc.x < screenWidth && sc.y > 0 && sc.y < screenHeight) {
                    const char* extTxt = "ZONA DE EXTRACAO";
                    DrawText(extTxt, (int)sc.x - MeasureText(extTxt, 15) / 2, (int)sc.y, 15, GREEN);
                }
            }
        }

        if (state == MissionState::IN_PROGRESS) {
            int boxX = 20;
            int boxY = 152;
            DrawRectangle(boxX, boxY, 315, 58, Color{ 16, 20, 28, 210 });
            DrawRectangleLinesEx({ (float)boxX, (float)boxY, 315.0f, 58.0f }, 1.2f, SKYBLUE);

            DrawText(TextFormat("MISSAO TATICA  |  TEMPO: %.1fs", stats.elapsedTime), boxX + 10, boxY + 8, 13, SKYBLUE);

            if (phase == ObjectivePhase::HACK_TERMINALS) {
                DrawText(TextFormat("[1/3] Hackear Terminais (%d / %d)", stats.terminalsHacked, (int)terminals.size()),
                         boxX + 10, boxY + 30, 15, WHITE);
            } else if (phase == ObjectivePhase::CLEAR_HOSTILES) {
                DrawText("[2/3] Eliminar Reforcos Inimigos", boxX + 10, boxY + 30, 15, ORANGE);
            } else if (phase == ObjectivePhase::EXTRACT_SQUAD) {
                DrawText("[3/3] Ir para a Zona de Extracao", boxX + 10, boxY + 30, 15, LIME);
            }
        }

        if (state == MissionState::VICTORY || state == MissionState::DEFEAT) {
            bool won = (state == MissionState::VICTORY);
            Color accent = won ? GREEN : RED;

            DrawRectangle(0, 0, screenWidth, screenHeight, Fade(BLACK, 0.72f));

            int modalW = 540;
            int modalH = 310;
            int mx = screenWidth / 2 - modalW / 2;
            int my = screenHeight / 2 - modalH / 2;

            DrawRectangle(mx, my, modalW, modalH, Color{ 20, 24, 34, 245 });
            DrawRectangleLinesEx({ (float)mx, (float)my, (float)modalW, (float)modalH }, 2.5f, accent);

            const char* header = won ? "MISSAO CONCLUIDA!" : "MISSAO FRACASSADA";
            DrawText(header, screenWidth / 2 - MeasureText(header, 32) / 2, my + 24, 32, accent);

            const char* sub = won ? "Esquadrao extraido com sucesso." : defeatReason.c_str();
            DrawText(sub, screenWidth / 2 - MeasureText(sub, 16) / 2, my + 66, 16, LIGHTGRAY);

            DrawLine(mx + 30, my + 98, mx + modalW - 30, my + 98, Fade(accent, 0.4f));

            DrawText(TextFormat("Tempo Total: %.1f segundos", stats.elapsedTime), mx + 45, my + 116, 18, WHITE);
            DrawText(TextFormat("Inimigos Eliminados: %d", stats.enemiesEliminated), mx + 45, my + 144, 18, WHITE);
            DrawText(TextFormat("Headshots Criticos: %d", stats.headshotsLanded), mx + 45, my + 172, 18, GOLD);
            DrawText(TextFormat("Terminais Hackeados: %d  |  Barris Detonados: %d", stats.terminalsHacked, stats.barrelsDetonated),
                     mx + 45, my + 200, 17, SKYBLUE);

            const char* footer = "[F5] Jogar Novamente    |    [ESC] Voltar ao Menu Principal";
            DrawText(footer, screenWidth / 2 - MeasureText(footer, 16) / 2, my + 260, 16, YELLOW);
        }
    }
};