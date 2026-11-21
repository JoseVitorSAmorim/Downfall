#include "World.hpp"
#include "Player.hpp"
#include "Weapon.hpp"
#include "SquadAI.hpp"
#include "Menu.hpp"
#include "Audio.hpp"
#include "Objectives.hpp"
#include "MapEditor.hpp"
#include "AssetManager.hpp"
#include "Cutscene.hpp"

struct HitmarkerFeedback {
    float timer = 0.0f;
    float maxDuration = 0.20f;
    Color color = WHITE;
    const char* callout = "";
};

void InitializeTestMode(GameMode mode, std::vector<WorldBlock>& world, SourcePlayer& player,
                        ViewmodelWeapon& weapon, std::vector<SquadAlly>& allies,
                        std::vector<EnemyBot>& enemies, std::vector<Tracer>& tracers,
                        std::vector<ThrownGrenade>& grenades, std::vector<ExplosionEffect>& explosions,
                        std::vector<DamageIndicator>& damageIndicators,
                        SquadOrder& currentOrder, TacticalPing& tacticalPing,
                        HitmarkerFeedback& hitmarker, MissionManager& mission,
                        CutsceneManager& cutscenes, Camera3D& camera, ProceduralAudio& audio) {
    player = SourcePlayer();
    weapon = ViewmodelWeapon();
    tracers.clear();
    grenades.clear();
    explosions.clear();
    damageIndicators.clear();
    allies.clear();
    enemies.clear();
    currentOrder = SquadOrder::HOLD;
    tacticalPing.Clear();
    hitmarker.timer = 0.0f;

    if (mode == GameMode::MOVEMENT_TEST) {
        world = LevelLoader::LoadFromFile("mapa_movimento.txt", MapPreset::MOVEMENT_COURSE);
        player.pos = { 0.0f, 0.5f, 20.0f };
        player.yaw = PI;
        mission.SetupForMode(false, false);
    } else if (mode == GameMode::SUPPRESSION_RANGE) {
        world = LevelLoader::LoadFromFile("mapa_treino.txt", MapPreset::FIRING_RANGE);
        player.pos = { 0.0f, 0.5f, 14.0f };
        allies = {
            { 1, "Bravo",   { -4.5f, 0.0f, 6.5f }, { -4.5f, 0.0f, 6.5f }, { -4.5f, 0.0f, 6.5f }, 100.0f, 50.0f, 0.4f, 0.0f, SKYBLUE },
            { 2, "Charlie", {  4.5f, 0.0f, 6.5f }, {  4.5f, 0.0f, 6.5f }, {  4.5f, 0.0f, 6.5f }, 100.0f, 50.0f, 0.9f, 2.1f, LIME }
        };
        ResetWave(enemies);
        mission.SetupForMode(false, true);
    } else {
        world = LevelLoader::LoadFromFile("mapa_squad.txt", MapPreset::SQUAD_ARENA);
        player.pos = { 0.0f, 0.5f, 14.0f };
        allies = {
            { 1, "Bravo",   { -5.5f, 0.0f,  4.0f }, { -5.5f, 0.0f,  4.0f }, { -5.5f, 0.0f,  4.0f }, 100.0f, 50.0f, 0.4f, 0.0f, SKYBLUE },
            { 2, "Charlie", {  5.5f, 0.0f,  4.0f }, {  5.5f, 0.0f,  4.0f }, {  5.5f, 0.0f,  4.0f }, 100.0f, 50.0f, 0.9f, 2.1f, LIME },
            { 3, "Delta",   {  9.0f, 0.0f, -2.0f }, {  9.0f, 0.0f, -2.0f }, {  9.0f, 0.0f, -2.0f }, 100.0f, 50.0f, 1.3f, 4.2f, GOLD }
        };
        ResetWave(enemies);
        mission.SetupForMode(true, false);
    }

    Vector3 eye = player.GetEyePosition();
    Vector3 target = Vector3Add(eye, player.GetForward3D());
    camera.position = eye;
    camera.target = target;
    camera.fovy = 75.0f;

    bool isCampaign = (mode != GameMode::MOVEMENT_TEST && mode != GameMode::SUPPRESSION_RANGE);
    cutscenes.SetupForMode(isCampaign, eye, target);
    if (isCampaign) {
        cutscenes.Play(0, eye, target, 75.0f, audio);
    }
}

Vector3 ApplySpreadToDirection(Vector3 dir, float spread) {
    if (spread <= 0.0f) return dir;
    float rx = ((float)GetRandomValue(-1000, 1000) / 1000.0f) * spread;
    float ry = ((float)GetRandomValue(-1000, 1000) / 1000.0f) * spread;
    float rz = ((float)GetRandomValue(-1000, 1000) / 1000.0f) * spread;
    return Vector3Normalize({ dir.x + rx, dir.y + ry, dir.z + rz });
}

int main() {
    const int largura = 1280;
    const int altura = 720;
    InitWindow(largura, altura, "Squad Tactical Shooter - Cutscenes & Trigger Zones (SAVEPOINT_17)");
    SetTargetFPS(144);
    SetExitKey(KEY_NULL);

    ProceduralAudio audio;
    audio.Init();

    AssetManager assets;
    assets.Init();

    MainMenu menu;
    InGameMapEditor mapEditor;
    CutsceneManager cutscenes;
    GameMode currentMode = GameMode::MAIN_MENU;
    bool shouldQuit = false;

    std::vector<WorldBlock> world;
    SourcePlayer player;
    ViewmodelWeapon weapon;
    std::vector<SquadAlly> allies;
    std::vector<EnemyBot> enemies;
    std::vector<Tracer> tracers;
    std::vector<ThrownGrenade> grenades;
    std::vector<ExplosionEffect> explosions;
    std::vector<DamageIndicator> damageIndicators;

    SquadOrder currentOrder = SquadOrder::HOLD;
    TacticalPing tacticalPing;
    HitmarkerFeedback hitmarker;
    MissionManager mission;
    float orderBannerTimer = 0.0f;
    float animTime = 0.0f;
    float reloadAssetsBannerTimer = 0.0f;
    float playerStepTimer = 0.0f;
    float botStepTimer = 0.0f;

    Camera3D camera = {};
    camera.up = { 0.0f, 1.0f, 0.0f };
    camera.fovy = 75.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    while (!WindowShouldClose() && !shouldQuit) {
        float dt = std::min(GetFrameTime(), 0.05f);
        animTime += dt;

        if (reloadAssetsBannerTimer > 0.0f) reloadAssetsBannerTimer -= dt;

        if (IsKeyPressed(KEY_M)) {
            audio.ToggleMute();
        }

        // Hot-Reload de modelos (.glb) e sons externos (.ogg / .wav) com F7
        if (IsKeyPressed(KEY_F7)) {
            assets.ReloadAll();
            audio.ReloadExternalAssets();
            reloadAssetsBannerTimer = 2.6f;
            audio.PlayOrder();
        }

        // =====================================================================
        // ESTADO 1: MENU DE INICIALIZAÇÃO
        // =====================================================================
        if (currentMode == GameMode::MAIN_MENU) {
            if (IsCursorHidden()) EnableCursor();

            audio.UpdateDynamicMusic(dt, 0.0f, false);

            if (IsKeyPressed(KEY_ESCAPE)) {
                shouldQuit = true;
                continue;
            }

            GameMode chosen = menu.Update(largura, altura);
            if (chosen == GameMode::EXIT_GAME) {
                shouldQuit = true;
                continue;
            } else if (chosen == GameMode::MAP_EDITOR) {
                currentMode = GameMode::MAP_EDITOR;
                mapEditor.Enter(world);
                audio.PlayOrder();
                DisableCursor();
            } else if (chosen != GameMode::MAIN_MENU) {
                currentMode = chosen;
                InitializeTestMode(currentMode, world, player, weapon, allies, enemies, tracers, grenades, explosions, damageIndicators, currentOrder, tacticalPing, hitmarker, mission, cutscenes, camera, audio);
                DisableCursor();
            }

            BeginDrawing();
                menu.Draw(largura, altura);
                const char* audioTag = audio.muted
                    ? "[M] Som: MUDO"
                    : TextFormat("[M] Audio 3D: ON (%d SFX / %d Musicas .ogg/.wav)", audio.externalSfxCount, audio.externalMusicCount);
                DrawText(audioTag, 24, 24, 15, audio.muted ? RED : GREEN);

                const char* assetTag = TextFormat("[F7] Modelos Blender (.glb): %d / %d carregados", assets.loadedCount, assets.totalSlots);
                DrawText(assetTag, 24, 48, 15, (assets.loadedCount > 0) ? SKYBLUE : LIGHTGRAY);
            EndDrawing();
            continue;
        }

        // =====================================================================
        // ESTADO 2: EDITOR DE MAPAS IN-GAME
        // =====================================================================
        if (currentMode == GameMode::MAP_EDITOR) {
            audio.UpdateDynamicMusic(dt, 0.0f, false);

            if (IsKeyPressed(KEY_ESCAPE)) {
                currentMode = GameMode::MAIN_MENU;
                EnableCursor();
                continue;
            }

            mapEditor.Update(dt, world, audio);

            BeginDrawing();
                ClearBackground({ 18, 22, 30, 255 });
                BeginMode3D(mapEditor.cam);
                    mapEditor.Draw3D(world);
                EndMode3D();
                mapEditor.DrawHUD(largura, altura, world);
            EndDrawing();
            continue;
        }

        // =====================================================================
        // ESTADO 3: MODOS DE TESTE E MISSÃO EM 3D
        // =====================================================================
        if (IsKeyPressed(KEY_ESCAPE)) {
            cutscenes.Skip();
            currentMode = GameMode::MAIN_MENU;
            EnableCursor();
            continue;
        }

        if (orderBannerTimer > 0.0f) orderBannerTimer -= dt;
        if (hitmarker.timer > 0.0f) hitmarker.timer -= dt;

        if (IsKeyPressed(KEY_F5)) {
            InitializeTestMode(currentMode, world, player, weapon, allies, enemies, tracers, grenades, explosions, damageIndicators, currentOrder, tacticalPing, hitmarker, mission, cutscenes, camera, audio);
        }

        // Teclas de debug/teste das cutscenes e trigger zones
        if (IsKeyPressed(KEY_F6) && !cutscenes.IsPlaying()) {
            Vector3 eye = player.GetEyePosition();
            Vector3 tgt = Vector3Add(eye, player.GetForward3D());
            cutscenes.Play(0, eye, tgt, camera.fovy, audio);
        }
        if (IsKeyPressed(KEY_F8)) {
            cutscenes.showDebugTriggers = !cutscenes.showDebugTriggers;
            audio.PlayOrder();
        }

        if (IsKeyPressed(KEY_T) && mission.state == MissionState::SANDBOX && currentMode != GameMode::MOVEMENT_TEST && !cutscenes.IsPlaying()) {
            ResetWave(enemies);
            tacticalPing.Clear();
            audio.PlayOrder();
        }

        bool missionEnded = (mission.state == MissionState::VICTORY || mission.state == MissionState::DEFEAT);

        Vector3 preEye = player.GetEyePosition();
        Vector3 preTarget = Vector3Add(preEye, player.GetForward3D());
        cutscenes.Update(dt, player.pos, preEye, preTarget, camera.fovy, mission.extraction.isActive, audio);

        bool inCutscene = cutscenes.IsPlaying();

        if (!missionEnded && !inCutscene) {
            if (IsKeyPressed(KEY_ONE))   weapon.SelectSlot(1);
            if (IsKeyPressed(KEY_TWO))   weapon.SelectSlot(2);
            if (IsKeyPressed(KEY_THREE)) weapon.SelectSlot(3);
            if (IsKeyPressed(KEY_FOUR))  weapon.SelectSlot(4);
            if (IsKeyPressed(KEY_FIVE))  weapon.SelectSlot(5);

            float wheel = GetMouseWheelMove();
            if (wheel > 0.0f) weapon.CycleNextWeapon(-1);
            else if (wheel < 0.0f) weapon.CycleNextWeapon(1);

            if (IsKeyPressed(KEY_R)) {
                if (weapon.StartReload()) {
                    audio.PlayReload();
                }
            }

            if (!player.isDowned && !allies.empty()) {
                if (IsKeyPressed(KEY_F1)) {
                    currentOrder = SquadOrder::ADVANCE;
                    orderBannerTimer = 2.2f;
                    for (auto& ally : allies) ally.hasPingOrder = false;
                    if (tacticalPing.type == PingType::MOVE_COVER) tacticalPing.Clear();
                    audio.PlayOrder();
                } else if (IsKeyPressed(KEY_F2)) {
                    currentOrder = SquadOrder::HOLD;
                    orderBannerTimer = 2.2f;
                    for (auto& ally : allies) {
                        ally.hasPingOrder = false;
                        if (!ally.isDowned) ally.anchorPos = ally.pos;
                    }
                    if (tacticalPing.type == PingType::MOVE_COVER) tacticalPing.Clear();
                    audio.PlayOrder();
                } else if (IsKeyPressed(KEY_F3)) {
                    currentOrder = SquadOrder::SUPPRESS;
                    orderBannerTimer = 2.2f;
                    for (auto& ally : allies) {
                        ally.hasPingOrder = false;
                        if (!ally.isDowned) ally.anchorPos = ally.pos;
                    }
                    if (tacticalPing.type == PingType::MOVE_COVER) tacticalPing.Clear();
                    audio.PlayOrder();
                }
            }

            bool wasGrounded = player.isGrounded;
            Vector2 mouseDelta = GetMouseDelta();
            player.UpdateLook(mouseDelta, 0.0022f);
            player.Update(dt, world);

            if (wasGrounded && !player.isGrounded && player.vel.y > 1.0f) {
                audio.PlayJump();
            }

            if (player.pos.y < -20.0f) {
                player.pos = (currentMode == GameMode::MOVEMENT_TEST) ? Vector3{ 0.0f, 0.5f, 20.0f } : Vector3{ 0.0f, 0.5f, 14.0f };
                player.vel = { 0.0f, 0.0f, 0.0f };
            }

            float horizSpeed = sqrtf(player.vel.x * player.vel.x + player.vel.z * player.vel.z);
            float targetFov = (horizSpeed > 7.5f) ? 83.0f : 75.0f;
            camera.fovy += (targetFov - camera.fovy) * std::min(10.0f * dt, 1.0f);

            Vector3 eyePos = player.GetEyePosition();
            Vector3 forward = player.GetForward3D();
            Vector3 listenerRight = Vector3Normalize(Vector3CrossProduct(forward, camera.up));
            audio.UpdateListener(eyePos, forward, listenerRight);

            if (player.isGrounded && horizSpeed > 1.8f && !player.isDowned) {
                float stepRate = player.isSprinting ? 0.30f : (player.isCrouched ? 0.55f : 0.40f);
                playerStepTimer += dt;
                if (playerStepTimer >= stepRate) {
                    playerStepTimer = 0.0f;
                    audio.Play2D(SfxId::FOOTSTEP, player.isCrouched ? 0.14f : 0.28f, 0.95f + (float)GetRandomValue(0, 10) * 0.01f);
                }
            } else {
                playerStepTimer = 0.25f;
            }

            for (auto& ally : allies) {
                ally.isBeingDragged = false;
            }

            int nearbyDownedIdx = -1;
            float minDownedDist = 2.8f;
            for (size_t i = 0; i < allies.size(); i++) {
                if (!allies[i].isDowned) continue;
                float d = Vector3Distance(player.pos, allies[i].pos);
                if (d < minDownedDist) {
                    minDownedDist = d;
                    nearbyDownedIdx = (int)i;
                }
            }

            if (!player.isDowned && !allies.empty()) {
                if (IsKeyDown(KEY_E)) {
                    if (player.draggingAllyId == -1 && nearbyDownedIdx != -1) {
                        player.draggingAllyId = allies[nearbyDownedIdx].id;
                    }
                    if (player.draggingAllyId != -1) {
                        SquadAlly& dragged = allies[player.draggingAllyId - 1];
                        if (!dragged.isDowned || Vector3Distance(player.pos, dragged.pos) > 4.2f) {
                            player.draggingAllyId = -1;
                        } else {
                            dragged.isBeingDragged = true;
                            Vector3 horizForward = { sinf(player.yaw), 0.0f, cosf(player.yaw) };
                            Vector3 holdPos = Vector3Add(player.pos, Vector3Scale(horizForward, 1.35f));
                            MoveAllyWithCollision(dragged, holdPos, 14.0f, dt, world);
                        }
                    }
                } else {
                    player.draggingAllyId = -1;
                }

                if (IsKeyDown(KEY_F) && player.draggingAllyId == -1 && nearbyDownedIdx != -1) {
                    SquadAlly& target = allies[nearbyDownedIdx];
                    target.reviveProgress += dt * 0.42f;
                    if (target.reviveProgress >= 1.0f) {
                        target.isDowned = false;
                        target.hp = 75.0f;
                        target.lastHp = 75.0f;
                        target.reviveProgress = 0.0f;
                        target.bleedoutTimer = 25.0f;
                        target.anchorPos = target.pos;
                        audio.PlayRevive();
                    }
                } else {
                    for (auto& ally : allies) {
                        if (ally.isDowned) ally.reviveProgress = std::max(0.0f, ally.reviveProgress - dt * 0.5f);
                    }
                }
            }

            mission.Update(dt, player, weapon, allies, enemies, (nearbyDownedIdx != -1), audio);
            weapon.Update(dt, mouseDelta, horizSpeed, player.isGrounded);

            camera.position = eyePos;
            camera.target = Vector3Add(eyePos, forward);

            if (IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE) && !player.isDowned && !allies.empty()) {
                if (ProcessTacticalPing(eyePos, forward, world, allies, enemies, tacticalPing)) {
                    if (audio.initialized) {
                        SetSoundPitch(audio.sfxOrder, (tacticalPing.type == PingType::FOCUS_TARGET) ? 1.28f : 0.92f);
                        audio.PlayOrder();
                    }
                }
            }

            float maxAllyAggro = 0.0f;
            for (const auto& ally : allies) {
                if (!ally.isDowned && ally.aggro > maxAllyAggro) maxAllyAggro = ally.aggro;
            }
            bool isStealthFlank = (!allies.empty() && !player.isDowned && player.aggro < 25.0f && maxAllyAggro > 50.0f);

            const WeaponStats& wStats = weapon.GetCurrentStats();
            bool triggerPulled = wStats.isAutomatic
                ? IsMouseButtonDown(MOUSE_BUTTON_LEFT)
                : IsMouseButtonPressed(MOUSE_BUTTON_LEFT);

            if (triggerPulled) {
                if (weapon.CanFire()) {
                    weapon.ConsumeAndAnimateFire();
                    audio.PlayWeaponAction(wStats.id, isStealthFlank);

                    if (!player.isDowned && !enemies.empty()) {
                        player.aggro = std::min(100.0f, player.aggro + wStats.aggroOnFire);
                    }

                    if (wStats.isGrenade) {
                        bool isFlash = (wStats.id == WeaponId::GADGET_FLASH);
                        Vector3 spawnPos = Vector3Add(eyePos, Vector3Scale(forward, 0.65f));
                        Vector3 throwVel = Vector3Add(Vector3Scale(forward, 17.5f), { 0.0f, 4.2f, 0.0f });
                        throwVel = Vector3Add(throwVel, Vector3Scale(player.vel, 0.35f));
                        grenades.push_back({ spawnPos, throwVel, isFlash, isFlash ? 1.8f : 2.2f, false });
                    } else {
                        Vector3 muzzlePos = Vector3Add(eyePos, Vector3Add(Vector3Scale(listenerRight, 0.24f), { 0, -0.18f, 0 }));

                        bool anyHit = false;
                        bool anyHeadshot = false;
                        bool anyWallbang = false;
                        bool anyKill = false;
                        bool hitBarrel = false;

                        for (int p = 0; p < wStats.pelletCount; p++) {
                            Vector3 shotDir = ApplySpreadToDirection(forward, wStats.spread);
                            Ray ray = { eyePos, shotDir };

                            BallisticHitResult hitRes = TraceBallisticShot(ray, wStats.range, wStats.penetrationPower, world, enemies);
                            float shotDist = Vector3Distance(eyePos, hitRes.endPoint);

                            int barrelIdx = mission.CheckBarrelShot(ray, shotDist);
                            if (barrelIdx != -1) {
                                RayCollision bCol = GetRayCollisionBox(ray, mission.barrels[barrelIdx].GetAABB());
                                if (!wStats.isMelee) {
                                    tracers.push_back({ muzzlePos, bCol.point, ORANGE, 0.085f });
                                }
                                mission.DamageBarrel(barrelIdx, wStats.damage, explosions);
                                anyHit = true;
                                hitBarrel = true;
                                continue;
                            }

                            if (!wStats.isMelee) {
                                Color shotColor = isStealthFlank ? SKYBLUE : (hitRes.didWallbang ? ORANGE : wStats.accentColor);
                                tracers.push_back({ muzzlePos, hitRes.endPoint, shotColor, 0.085f });

                                if (hitRes.hasPiercePoint) {
                                    Vector3 sparkEnd = Vector3Add(hitRes.piercePoint, Vector3Scale(shotDir, 0.45f));
                                    tracers.push_back({ hitRes.piercePoint, sparkEnd, WHITE, 0.14f });
                                }
                            }

                            if (hitRes.hitEnemyIdx != -1) {
                                EnemyBot& targetEnemy = enemies[hitRes.hitEnemyIdx];
                                bool wasAlive = targetEnemy.IsAlive();

                                float stateMult = isStealthFlank ? 2.0f : (player.isDowned ? 0.65f : 1.0f);
                                float finalDamage = wStats.damage * hitRes.damageMultiplier * stateMult;

                                targetEnemy.hp -= finalDamage;
                                targetEnemy.suppression = std::min(100.0f, targetEnemy.suppression + wStats.suppressionOnHit);

                                if (!player.isDowned) {
                                    player.aggro = std::min(100.0f, player.aggro + wStats.aggroOnHit);
                                }

                                anyHit = true;
                                if (hitRes.isHeadshot) {
                                    anyHeadshot = true;
                                    mission.stats.headshotsLanded++;
                                }
                                if (hitRes.didWallbang) anyWallbang = true;
                                if (wasAlive && !targetEnemy.IsAlive()) {
                                    anyKill = true;
                                    mission.stats.enemiesEliminated++;
                                }
                            }
                        }

                        if (anyHit) {
                            hitmarker.timer = hitmarker.maxDuration;
                            if (anyKill) {
                                hitmarker.color = RED;
                                hitmarker.callout = anyHeadshot ? "HEADSHOT! ABATE" : (anyWallbang ? "WALLBANG! ABATE" : "ELIMINADO");
                            } else if (anyHeadshot) {
                                hitmarker.color = GOLD;
                                hitmarker.callout = anyWallbang ? "WALLBANG HEADSHOT! (2.5x)" : "HEADSHOT! (2.5x)";
                            } else if (hitBarrel) {
                                hitmarker.color = ORANGE;
                                hitmarker.callout = "BARRIL EXPLOSIVO!";
                            } else if (anyWallbang) {
                                hitmarker.color = ORANGE;
                                hitmarker.callout = "WALLBANG!";
                            } else {
                                hitmarker.color = RAYWHITE;
                                hitmarker.callout = "";
                            }
                        }
                    }
                } else if (!weapon.isReloading && !wStats.isMelee && !wStats.isGrenade && weapon.GetCurrentAmmo() <= 0) {
                    if (weapon.StartReload()) {
                        audio.PlayReload();
                    }
                }
            }

            size_t prevExplosions = explosions.size();
            UpdateGrenades(dt, grenades, explosions, world);

            for (size_t e = prevExplosions; e < explosions.size(); e++) {
                const ExplosionEffect exp = explosions[e];
                audio.PlayExplosion3D(exp.pos, exp.isFlashbang);
                mission.CheckExplosionChain(exp, explosions);

                bool nadeHit = false;
                bool nadeKill = false;
                for (auto& enemy : enemies) {
                    if (!enemy.IsAlive()) continue;
                    float dist = Vector3Distance(enemy.GetEyePos(), exp.pos);
                    if (dist <= exp.maxRadius) {
                        float falloff = 1.0f - (dist / exp.maxRadius) * 0.5f;
                        if (exp.isFlashbang) {
                            enemy.hp -= 5.0f;
                            enemy.suppression = 100.0f;
                            enemy.isSuppressed = true;
                            enemy.coverSearchTimer = 0.0f;
                            player.aggro = std::max(0.0f, player.aggro - 20.0f);
                            nadeHit = true;
                        } else {
                            enemy.hp -= 115.0f * falloff;
                            enemy.suppression = 100.0f;
                            enemy.isSuppressed = true;
                            nadeHit = true;
                            if (!enemy.IsAlive()) {
                                nadeKill = true;
                                mission.stats.enemiesEliminated++;
                            }
                        }
                    }
                }
                if (nadeHit) {
                    hitmarker.timer = 0.25f;
                    hitmarker.color = nadeKill ? RED : ORANGE;
                    hitmarker.callout = nadeKill ? "EXPLOSAO! ABATE" : (exp.isFlashbang ? "INIMIGO CEGADO!" : "DANO EM AREA");
                }
            }

            grenades.erase(
                std::remove_if(grenades.begin(), grenades.end(), [](const ThrownGrenade& g) { return g.exploded; }),
                grenades.end()
            );
            for (size_t i = 0; i < explosions.size(); ) {
                explosions[i].timer -= dt;
                if (explosions[i].timer <= 0.0f) explosions.erase(explosions.begin() + i);
                else i++;
            }

            for (size_t i = 0; i < damageIndicators.size(); ) {
                damageIndicators[i].timer -= dt;
                if (damageIndicators[i].timer <= 0.0f) damageIndicators.erase(damageIndicators.begin() + i);
                else i++;
            }

            if (!player.isDowned && !enemies.empty()) {
                bool playerExposed = false;
                for (const auto& enemy : enemies) {
                    if (enemy.IsAlive() && HasLineOfSight(enemy.GetEyePos(), eyePos, world)) {
                        playerExposed = true;
                        break;
                    }
                }
                float decayRate = playerExposed ? 6.5f : 15.0f;
                player.aggro = std::max(0.0f, player.aggro - decayRate * dt);
            }

            bool prevPlayerDowned = player.isDowned;
            std::vector<bool> prevAllyDowned(allies.size(), false);
            for (size_t i = 0; i < allies.size(); i++) {
                prevAllyDowned[i] = allies[i].isDowned;
            }

            size_t prevTracers = tracers.size();

            if (!allies.empty() || !enemies.empty()) {
                UpdateAllies(dt, allies, enemies, player, currentOrder, world, tracers, tacticalPing);
                UpdateEnemies(dt, enemies, player, allies, world, tracers, damageIndicators);
            }

            for (size_t t = prevTracers; t < tracers.size(); t++) {
                bool isEnemyShot = (tracers[t].color.r > 180 && tracers[t].color.b < 80);
                SfxId shotSfx = isEnemyShot ? SfxId::RIFLE : SfxId::LMG;
                float pitch = isEnemyShot ? 0.93f : 1.07f;
                audio.Play3D(shotSfx, tracers[t].start, 0.68f, pitch, 48.0f);
            }

            botStepTimer += dt;
            if (botStepTimer >= 0.45f) {
                botStepTimer = 0.0f;
                for (const auto& ally : allies) {
                    if (!ally.isDowned && Vector3Distance(ally.pos, ally.anchorPos) > 0.65f) {
                        audio.PlayFootstep3D(ally.pos, 0.32f);
                    }
                }
            }

            if (!prevPlayerDowned && player.isDowned) {
                audio.PlayDownedAlert();
            } else if (prevPlayerDowned && !player.isDowned) {
                audio.PlayRevive();
            }

            for (size_t i = 0; i < allies.size(); i++) {
                if (!prevAllyDowned[i] && allies[i].isDowned) {
                    audio.PlayDownedAlert();
                    break;
                }
            }

            for (size_t i = 0; i < tracers.size(); ) {
                tracers[i].timer -= dt;
                if (tracers[i].timer <= 0.0f) tracers.erase(tracers.begin() + i);
                else i++;
            }
        } else if (inCutscene) {
            // Durante a cutscene, a câmera segue o trilho cinematográfico
            GetMouseDelta(); // Consome delta do mouse para evitar salto ao voltar
            cutscenes.ApplyToCamera(camera);
            Vector3 camFwd = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
            Vector3 camRight = Vector3Normalize(Vector3CrossProduct(camFwd, camera.up));
            audio.UpdateListener(camera.position, camFwd, camRight);
        }

        // Atualiza Trilha Sonora Dinâmica
        float maxAllyAggro = 0.0f;
        for (const auto& ally : allies) {
            if (!ally.isDowned && ally.aggro > maxAllyAggro) maxAllyAggro = ally.aggro;
        }
        float rawTension = 0.0f;
        if (currentMode != GameMode::MOVEMENT_TEST && !missionEnded) {
            float peakAggro = std::max(player.aggro, maxAllyAggro) / 100.0f;
            int activeHostiles = 0;
            for (const auto& e : enemies) {
                if (e.IsAlive() && e.currentTargetId != -1) activeHostiles++;
            }
            float hostileFactor = std::min(1.0f, activeHostiles * 0.25f);
            rawTension = std::clamp(std::max(peakAggro, hostileFactor * 0.70f), 0.0f, 1.0f);
            if (inCutscene) rawTension *= 0.45f;
        }
        audio.UpdateDynamicMusic(dt, rawTension, true);

        // =====================================================================
        // RENDERIZAÇÃO 3D
        // =====================================================================
        Vector3 eyePos = player.GetEyePosition();
        Vector3 forward = player.GetForward3D();
        float horizSpeed = sqrtf(player.vel.x * player.vel.x + player.vel.z * player.vel.z);
        bool isStealthFlank = (!allies.empty() && !player.isDowned && player.aggro < 25.0f && maxAllyAggro > 50.0f);

        BeginDrawing();
            ClearBackground({ 20, 23, 30, 255 });

            BeginMode3D(camera);
                for (const auto& block : world) {
                    DrawCubeV(block.pos, block.size, block.color);
                    DrawCubeWiresV(block.pos, block.size, BLACK);
                }

                mission.Draw3D(assets);
                cutscenes.DrawTriggers3D();

                for (const auto& ally : allies) {
                    assets.DrawAlly(ally, enemies, currentOrder, animTime);
                }

                for (size_t eIdx = 0; eIdx < enemies.size(); eIdx++) {
                    const auto& enemy = enemies[eIdx];
                    if (!enemy.IsAlive()) continue;

                    bool isPingedTarget = (tacticalPing.type == PingType::FOCUS_TARGET && tacticalPing.targetEnemyIdx == (int)eIdx);
                    assets.DrawEnemy(enemy, player, allies, isPingedTarget, animTime);

                    if (enemy.currentTargetId != -1) {
                        Vector3 targetAim = (enemy.currentTargetId == 0)
                            ? Vector3Subtract(eyePos, { 0, 0.3f, 0 })
                            : allies[enemy.currentTargetId - 1].GetEyePos();

                        Color aggroLineColor = (enemy.currentTargetId == 0)
                            ? Fade(RED, 0.65f)
                            : Fade(ORANGE, 0.35f);
                        DrawLine3D(enemy.GetEyePos(), targetAim, aggroLineColor);
                    }
                }

                DrawTacticalPing3D(tacticalPing, allies, enemies);

                for (const auto& g : grenades) {
                    DrawSphere(g.pos, 0.16f, g.isFlashbang ? SKYBLUE : DARKGREEN);
                    DrawSphereWires(g.pos, 0.17f, 6, 6, g.isFlashbang ? WHITE : RED);
                }

                for (const auto& exp : explosions) {
                    float progress = 1.0f - (exp.timer / exp.maxDuration);
                    float curRadius = exp.maxRadius * (0.25f + 0.75f * progress);
                    float alpha = (1.0f - progress) * 0.55f;
                    Color expCol = exp.isFlashbang ? Fade(WHITE, alpha) : Fade(ORANGE, alpha);
                    DrawSphere(exp.pos, curRadius, expCol);
                }

                for (const auto& t : tracers) {
                    DrawLine3D(t.start, t.end, t.color);
                }

                if (!missionEnded && !inCutscene) {
                    weapon.Draw(camera, isStealthFlank, assets);
                }
            EndMode3D();

            // =================================================================
            // HUD 2D (Oculta HUD de combate durante Cutscenes)
            // =================================================================
            int cx = largura / 2;
            int cy = altura / 2;

            if (!inCutscene) {
                int nearbyDownedIdx = -1;
                for (size_t i = 0; i < allies.size(); i++) {
                    if (allies[i].isDowned && Vector3Distance(player.pos, allies[i].pos) < 2.8f) {
                        nearbyDownedIdx = (int)i;
                        break;
                    }
                }

                if (!missionEnded) {
                    Vector2 playerLook2D = Vector2Normalize({ sinf(player.yaw), cosf(player.yaw) });
                    Vector2 playerRight2D = { -playerLook2D.y, playerLook2D.x };
                    for (const auto& dmg : damageIndicators) {
                        Vector2 toAttacker = { dmg.attackerPos.x - player.pos.x, dmg.attackerPos.z - player.pos.z };
                        if (Vector2Length(toAttacker) < 0.01f) continue;
                        toAttacker = Vector2Normalize(toAttacker);

                        float fwdDot = Vector2DotProduct(playerLook2D, toAttacker);
                        float rightDot = Vector2DotProduct(playerRight2D, toAttacker);
                        float relAngle = atan2f(rightDot, fwdDot);

                        float ringRadius = 78.0f;
                        float dirX = sinf(relAngle);
                        float dirY = -cosf(relAngle);
                        Vector2 tip = { cx + dirX * (ringRadius + 16.0f), cy + dirY * (ringRadius + 16.0f) };
                        Vector2 perp = { -dirY, dirX };
                        Vector2 base1 = { cx + dirX * ringRadius + perp.x * 12.0f, cy + dirY * ringRadius + perp.y * 12.0f };
                        Vector2 base2 = { cx + dirX * ringRadius - perp.x * 12.0f, cy + dirY * ringRadius - perp.y * 12.0f };

                        float alpha = std::clamp(dmg.timer / 0.65f, 0.0f, 1.0f) * 0.85f;
                        DrawTriangle(tip, base2, base1, Fade(RED, alpha));
                    }
                }

                if (reloadAssetsBannerTimer > 0.0f) {
                    const char* reloadMsg = TextFormat("ASSETS RECARREGADOS: %d .GLB  |  %d SFX .OGG/.WAV", assets.loadedCount, audio.externalSfxCount);
                    DrawText(reloadMsg, cx - MeasureText(reloadMsg, 17) / 2, 114, 17, SKYBLUE);
                }

                if (currentMode == GameMode::MOVEMENT_TEST) {
                    float unitsPerSec = horizSpeed * 50.0f;
                    Color speedColor = (unitsPerSec > 495.0f) ? GREEN : (player.isSprinting ? SKYBLUE : RAYWHITE);
                    const char* postura = player.isCrouched ? "AGACHADO" : (player.isSprinting ? "CORRENDO" : "EM PE");
                    const char* arStatus = player.isGrounded ? "NO CHAO" : "NO AR (AIR STRAFE)";

                    DrawText("MODO: PISTA DE MOVIMENTACAO (SOURCE PHYSICS)", 20, 20, 18, SKYBLUE);
                    DrawText(TextFormat("Posicao: X=%.1f  Y=%.2f  Z=%.1f", player.pos.x, player.pos.y, player.pos.z), 20, 46, 16, LIGHTGRAY);
                    DrawText(TextFormat("Estado: %s | %s", postura, arStatus), 20, 68, 16, GOLD);

                    DrawText(TextFormat("VELOCIDADE: %.0f u/s", unitsPerSec), cx - 120, altura - 115, 26, speedColor);
                    DrawText("1-5: Armas | Shift: Correr | F5: Reiniciar | F6: Cutscene | F7: Reload | ESC: Menu",
                             20, altura - 26, 14, LIGHTGRAY);
                } else {
                    if (tacticalPing.type == PingType::MOVE_COVER) {
                        Vector3 beaconTop = { tacticalPing.worldPos.x, 2.15f, tacticalPing.worldPos.z };
                        if (Vector3DotProduct(forward, Vector3Subtract(beaconTop, eyePos)) > 0.0f) {
                            Vector2 pScreen = GetWorldToScreen(beaconTop, camera);
                            if (pScreen.x > 0 && pScreen.x < largura && pScreen.y > 0 && pScreen.y < altura) {
                                const char* pingTag = TextFormat("COBERTURA [%s]", allies[tacticalPing.assignedAllyId - 1].name.c_str());
                                DrawText(pingTag, (int)pScreen.x - MeasureText(pingTag, 13) / 2, (int)pScreen.y - 16, 13, SKYBLUE);
                            }
                        }
                    }

                    for (const auto& ally : allies) {
                        float headOffset = ally.isDowned ? 1.2f : (ally.isSuppressed ? 1.6f : 2.15f);
                        Vector3 headWorld = { ally.pos.x, ally.pos.y + headOffset, ally.pos.z };
                        if (Vector3DotProduct(forward, Vector3Subtract(headWorld, eyePos)) <= 0.0f) continue;

                        Vector2 screenPos = GetWorldToScreen(headWorld, camera);
                        if (screenPos.x <= 0 || screenPos.x >= largura || screenPos.y <= 0 || screenPos.y >= altura) continue;

                        if (ally.isDowned) {
                            const char* alert = TextFormat("%s CAIDO! (%.0fs)", ally.name.c_str(), ally.bleedoutTimer);
                            DrawText(alert, (int)screenPos.x - MeasureText(alert, 15) / 2, (int)screenPos.y - 28, 15, RED);

                            if (Vector3Distance(player.pos, ally.pos) < 3.0f) {
                                const char* prompt = "[E] ARRASTAR  |  [F] REANIMAR";
                                DrawText(prompt, (int)screenPos.x - MeasureText(prompt, 14) / 2, (int)screenPos.y - 10, 14, YELLOW);
                            }

                            if (ally.reviveProgress > 0.0f) {
                                DrawRectangle((int)screenPos.x - 40, (int)screenPos.y + 10, 80, 10, BLACK);
                                DrawRectangle((int)screenPos.x - 38, (int)screenPos.y + 12, (int)(76 * ally.reviveProgress), 6, GREEN);
                            }
                        } else {
                            DrawRectangle((int)screenPos.x - 30, (int)screenPos.y - 8, 60, 7, BLACK);
                            DrawRectangle((int)screenPos.x - 29, (int)screenPos.y - 7, (int)(58 * (ally.hp / 100.0f)), 5, ally.color);

                            DrawRectangle((int)screenPos.x - 30, (int)screenPos.y + 1, 60, 5, BLACK);
                            Color suppCol = ally.isSuppressed ? ORANGE : YELLOW;
                            DrawRectangle((int)screenPos.x - 29, (int)screenPos.y + 2, (int)(58 * (ally.suppression / 100.0f)), 3, suppCol);

                            if (ally.isSuppressed) {
                                const char* suppTxt = "SUPRIMIDO!";
                                DrawText(suppTxt, (int)screenPos.x - MeasureText(suppTxt, 12) / 2, (int)screenPos.y - 23, 12, ORANGE);
                            } else if (ally.hasPingOrder) {
                                const char* moveTxt = "INDO P/ COBERTURA";
                                DrawText(moveTxt, (int)screenPos.x - MeasureText(moveTxt, 12) / 2, (int)screenPos.y - 23, 12, SKYBLUE);
                            }
                        }
                    }

                    for (size_t eIdx = 0; eIdx < enemies.size(); eIdx++) {
                        const auto& enemy = enemies[eIdx];
                        if (!enemy.IsAlive()) continue;
                        float headOffset = enemy.isSuppressed ? 1.65f : 2.25f;
                        Vector3 headWorld = { enemy.pos.x, enemy.pos.y + headOffset, enemy.pos.z };
                        if (Vector3DotProduct(forward, Vector3Subtract(headWorld, eyePos)) <= 0.0f) continue;

                        Vector2 screenPos = GetWorldToScreen(headWorld, camera);
                        if (screenPos.x > 0 && screenPos.x < largura && screenPos.y > 0 && screenPos.y < altura) {
                            DrawRectangle((int)screenPos.x - 30, (int)screenPos.y - 6, 60, 7, BLACK);
                            DrawRectangle((int)screenPos.x - 29, (int)screenPos.y - 5, (int)(58 * (enemy.hp / enemy.maxHp)), 5, RED);

                            DrawRectangle((int)screenPos.x - 30, (int)screenPos.y + 3, 60, 5, BLACK);
                            Color enemySuppCol = enemy.isSuppressed ? ORANGE : YELLOW;
                            DrawRectangle((int)screenPos.x - 29, (int)screenPos.y + 4, (int)(58 * (enemy.suppression / 100.0f)), 3, enemySuppCol);

                            bool isPinged = (tacticalPing.type == PingType::FOCUS_TARGET && tacticalPing.targetEnemyIdx == (int)eIdx);
                            if (isPinged) {
                                const char* focusTag = "[ALVO PRIORITARIO]";
                                DrawText(focusTag, (int)screenPos.x - MeasureText(focusTag, 13) / 2, (int)screenPos.y - 36, 13, YELLOW);
                            }

                            const char* statusTag = enemy.isSuppressed
                                ? "SUPRIMIDO!"
                                : ((enemy.currentTargetId == -1) ? "SEM ALVO" : ((enemy.currentTargetId == 0) ? "ALVO: VOCE!" : TextFormat("ALVO: ALIADO %d", enemy.currentTargetId)));
                            Color tagColor = enemy.isSuppressed ? YELLOW : ((enemy.currentTargetId == 0) ? RED : ORANGE);
                            DrawText(statusTag, (int)screenPos.x - MeasureText(statusTag, 12) / 2, (int)screenPos.y - 21, 12, tagColor);
                        }
                    }

                    if (orderBannerTimer > 0.0f) {
                        const char* orderCallout = TextFormat("ORDEM AO ESQUADRAO: %s", GetOrderLabel(currentOrder));
                        Color bannerCol = (currentOrder == SquadOrder::SUPPRESS) ? ORANGE : ((currentOrder == SquadOrder::ADVANCE) ? GREEN : SKYBLUE);
                        DrawText(orderCallout, cx - MeasureText(orderCallout, 20) / 2, 85, 20, bannerCol);
                    }

                    if (player.isDowned && !missionEnded) {
                        DrawRectangleLinesEx({ 0, 0, (float)largura, (float)altura }, 12, Fade(RED, 0.65f));
                        const char* downMsg = TextFormat("VOCE CAIU! SANGRAMENTO EM %.0fs...", std::max(0.0f, 25.0f - mission.playerDownedTimer));
                        DrawText(downMsg, cx - MeasureText(downMsg, 22) / 2, cy + 50, 22, RED);

                        if (player.reviveProgress > 0.0f) {
                            DrawRectangle(cx - 120, cy + 85, 240, 18, BLACK);
                            DrawRectangle(cx - 118, cy + 87, (int)(236 * player.reviveProgress), 14, GREEN);
                            DrawText("REANIMANDO...", cx - 55, cy + 110, 16, GREEN);
                        }
                    } else if (player.draggingAllyId != -1) {
                        const char* dragMsg = TextFormat("ARRASTANDO %s PARA COBERTURA (PODE ATIRAR!)", allies[player.draggingAllyId - 1].name.c_str());
                        DrawText(dragMsg, cx - MeasureText(dragMsg, 18) / 2, altura - 135, 18, YELLOW);
                    }

                    int barW = 320;
                    int barX = cx - barW / 2;
                    DrawRectangle(barX - 2, 18, barW + 4, 24, BLACK);
                    Color aggroBarColor = player.isDowned ? GRAY : (isStealthFlank ? SKYBLUE : ((player.aggro > 65.0f) ? RED : ORANGE));
                    DrawRectangle(barX, 20, (int)(barW * (player.aggro / 100.0f)), 20, aggroBarColor);

                    const char* statusText = player.isDowned
                        ? "CAIDO (AMEACA ZERO)"
                        : (isStealthFlank ? "FURTIVO (2x DANO NOS FLANCOS)" : ((player.aggro > 65.0f) ? "AGGRO MAXIMO" : "AMEACA MODERADA"));
                    DrawText(statusText, cx - MeasureText(statusText, 16) / 2, 48, 16, aggroBarColor);

                    DrawText("ESQUADRAO:", 20, 20, 16, WHITE);
                    DrawText(TextFormat("Voce (Alpha): HP %.0f | Aggro %.0f%% %s", player.hp, player.aggro, player.isDowned ? "[CAIDO]" : ""),
                             20, 44, 16, player.isDowned ? RED : aggroBarColor);

                    for (size_t i = 0; i < allies.size(); i++) {
                        const auto& a = allies[i];
                        const char* tagExtra = a.isSuppressed ? "[SUPRIMIDO]" : (a.hasPingOrder ? "[PING: COBERTURA]" : "");
                        const char* allyLine = a.isDowned
                            ? TextFormat("%s: [CAIDO - %.0fs]", a.name.c_str(), a.bleedoutTimer)
                            : TextFormat("%s: HP %.0f | Aggro %.0f%% %s", a.name.c_str(), a.hp, a.aggro, tagExtra);
                        Color lineColor = a.isDowned ? RED : (a.isSuppressed ? ORANGE : a.color);
                        DrawText(allyLine, 20, 68 + (int)i * 22, 15, lineColor);
                    }

                    int cmdX = largura - 340;
                    DrawText("COMANDOS DO ESQUADRAO:", cmdX, 20, 16, WHITE);
                    DrawText("[F1] Avancar", cmdX, 44, 15, (currentOrder == SquadOrder::ADVANCE) ? GREEN : GRAY);
                    DrawText("[F2] Manter Posicao", cmdX, 66, 15, (currentOrder == SquadOrder::HOLD) ? SKYBLUE : GRAY);
                    DrawText("[F3] Fogo de Supressao", cmdX, 88, 15, (currentOrder == SquadOrder::SUPPRESS) ? ORANGE : GRAY);
                    DrawText("[Scroll Click] Ping Contextual", cmdX, 110, 15, (tacticalPing.type != PingType::NONE) ? YELLOW : LIGHTGRAY);
                    DrawText(TextFormat("[M] Som: %s | Trilha: %.0f%%", audio.muted ? "MUDO" : "3D ON", audio.musicTension * 100.0f),
                             cmdX, 134, 14, audio.muted ? RED : (audio.musicTension > 0.55f ? ORANGE : GREEN));

                    DrawText("1-5: Armas | F1-F3: Ordens | E/F: Interagir | F5: Reiniciar | F6: Cutscene | F8: Triggers | ESC: Menu",
                             20, altura - 26, 13, LIGHTGRAY);
                }

                const WeaponStats& wStats = weapon.GetCurrentStats();
                int hudW = 330;
                int hudH = 88;
                int hudX = largura - hudW - 20;
                int hudY = altura - hudH - 42;

                DrawRectangle(hudX, hudY, hudW, hudH, Color{ 18, 22, 30, 215 });
                DrawRectangleLinesEx({ (float)hudX, (float)hudY, (float)hudW, (float)hudH }, 1.5f, wStats.accentColor);

                const char* slotNames[5] = { "[1] PRIM", "[2] SEC", "[3] FACA", "[4] FRAG", "[5] FLASH" };
                for (int s = 1; s <= 5; s++) {
                    bool active = (weapon.currentSlot == s);
                    DrawText(slotNames[s - 1], hudX + 10 + (s - 1) * 63, hudY + 8, 12, active ? wStats.accentColor : GRAY);
                }

                DrawText(wStats.name, hudX + 12, hudY + 28, 19, WHITE);

                if (weapon.isReloading) {
                    DrawText("RECARREGANDO...", hudX + 12, hudY + 56, 15, YELLOW);
                } else if (wStats.isMelee) {
                    DrawText("CORPO A CORPO (SILENCIOSO)", hudX + 12, hudY + 56, 14, LIME);
                } else {
                    const char* ammoTxt = TextFormat("MUNICAO: %d / %d", weapon.GetCurrentAmmo(), wStats.magSize);
                    DrawText(ammoTxt, hudX + 12, hudY + 56, 15, (weapon.GetCurrentAmmo() == 0) ? RED : LIGHTGRAY);

                    if (wStats.penetrationPower > 0.0f) {
                        DrawText(TextFormat("PEN: %.2fm", wStats.penetrationPower), hudX + 225, hudY + 57, 13, ORANGE);
                    } else if (weapon.currentSlot == 1 || weapon.currentSlot == 2) {
                        DrawText(TextFormat("[Aperte %d]", weapon.currentSlot), hudX + 235, hudY + 57, 12, SKYBLUE);
                    }
                }

                if (!missionEnded) {
                    DrawCircleLines(cx, cy, (wStats.id == WeaponId::SHOTGUN) ? 14.0f : 5.0f, isStealthFlank ? SKYBLUE : GREEN);

                    if (hitmarker.timer > 0.0f) {
                        float ratio = hitmarker.timer / hitmarker.maxDuration;
                        float inner = 6.0f + (1.0f - ratio) * 4.0f;
                        float outer = 14.0f + (1.0f - ratio) * 6.0f;
                        Color hmCol = Fade(hitmarker.color, std::min(1.0f, ratio * 1.4f));

                        DrawLineEx({ cx - outer, cy - outer }, { cx - inner, cy - inner }, 2.2f, hmCol);
                        DrawLineEx({ cx + outer, cy - outer }, { cx + inner, cy - inner }, 2.2f, hmCol);
                        DrawLineEx({ cx - outer, cy + outer }, { cx - inner, cy + inner }, 2.2f, hmCol);
                        DrawLineEx({ cx + outer, cy + outer }, { cx + inner, cy + inner }, 2.2f, hmCol);

                        if (hitmarker.callout[0] != '\0') {
                            DrawText(hitmarker.callout, cx - MeasureText(hitmarker.callout, 14) / 2, cy + 20, 14, hmCol);
                        }
                    }
                }

                mission.DrawHUD(largura, altura, camera, player, (nearbyDownedIdx != -1));
            }

            // Renderiza Letterbox e Legendas (também anima o recolhimento suave após o fim)
            cutscenes.DrawHUD(largura, altura);
        EndDrawing();
    }

    assets.Shutdown();
    audio.Shutdown();
    CloseWindow();
    return 0;
}