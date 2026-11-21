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
#include "SaveSystem.hpp"
#include "Network.hpp"
#include <type_traits>

template <typename T, typename = void> struct HasShootTimer : std::false_type {};
template <typename T> struct HasShootTimer<T, std::void_t<decltype(std::declval<T>().shootTimer)>> : std::true_type {};

template <typename T, typename = void> struct HasFireCooldown : std::false_type {};
template <typename T> struct HasFireCooldown<T, std::void_t<decltype(std::declval<T>().fireCooldown)>> : std::true_type {};

template <typename T, typename = void> struct HasTimerField : std::false_type {};
template <typename T> struct HasTimerField<T, std::void_t<decltype(std::declval<T>().timer)>> : std::true_type {};

template <typename T, typename = void> struct HasExtractTimerField : std::false_type {};
template <typename T> struct HasExtractTimerField<T, std::void_t<decltype(std::declval<T>().extractTimer)>> : std::true_type {};

template <typename T, typename = void> struct HasElapsedTimeField : std::false_type {};
template <typename T> struct HasElapsedTimeField<T, std::void_t<decltype(std::declval<T>().elapsedTime)>> : std::true_type {};

template <typename T, typename = void> struct HasStatsElapsedTimeField : std::false_type {};
template <typename T> struct HasStatsElapsedTimeField<T, std::void_t<decltype(std::declval<T>().stats.elapsedTime)>> : std::true_type {};

template <typename T, typename = void> struct HasStatsTimeElapsedField : std::false_type {};
template <typename T> struct HasStatsTimeElapsedField<T, std::void_t<decltype(std::declval<T>().stats.timeElapsed)>> : std::true_type {};

template <typename T>
void SuppressBotFireTimer(T& ally) {
    if constexpr (HasShootTimer<T>::value) ally.shootTimer = 99.0f;
    if constexpr (HasFireCooldown<T>::value) ally.fireCooldown = 99.0f;
}

template <typename T>
float ReadExtractionProgress(const T& ext) {
    if constexpr (HasTimerField<T>::value) return ext.timer;
    else if constexpr (HasExtractTimerField<T>::value) return ext.extractTimer;
    else return 0.0f;
}

template <typename T>
void WriteExtractionProgress(T& ext, float val) {
    if constexpr (HasTimerField<T>::value) ext.timer = val;
    else if constexpr (HasExtractTimerField<T>::value) ext.extractTimer = val;
    else (void)val;
}

template <typename T>
float ReadMissionTimer(const T& m) {
    if constexpr (HasElapsedTimeField<T>::value) return m.elapsedTime;
    else if constexpr (HasTimerField<T>::value) return m.timer;
    else if constexpr (HasStatsElapsedTimeField<T>::value) return m.stats.elapsedTime;
    else if constexpr (HasStatsTimeElapsedField<T>::value) return m.stats.timeElapsed;
    else return 0.0f;
}

template <typename T>
void WriteMissionTimer(T& m, float val) {
    if constexpr (HasElapsedTimeField<T>::value) m.elapsedTime = val;
    else if constexpr (HasTimerField<T>::value) m.timer = val;
    else if constexpr (HasStatsElapsedTimeField<T>::value) m.stats.elapsedTime = val;
    else if constexpr (HasStatsTimeElapsedField<T>::value) m.stats.timeElapsed = val;
    else (void)val;
}

struct HitmarkerFeedback {
    float timer = 0.0f;
    float maxDuration = 0.20f;
    Color color = WHITE;
    const char* callout = "";
};

Vector3 GetSquadSpawnPosition(int slot) {
    switch (slot) {
        case 0:  return {  0.0f, 0.5f, 14.0f }; // Alpha
        case 1:  return { -5.5f, 0.0f,  4.0f }; // Bravo
        case 2:  return {  5.5f, 0.0f,  4.0f }; // Charlie
        case 3:  return {  9.0f, 0.0f, -2.0f }; // Delta
        default: return {  0.0f, 0.5f, 14.0f };
    }
}

void InitializeTestMode(GameMode mode, std::vector<WorldBlock>& world, SourcePlayer& player,
                        ViewmodelWeapon& weapon, std::vector<SquadAlly>& allies,
                        std::vector<EnemyBot>& enemies, std::vector<Tracer>& tracers,
                        std::vector<ThrownGrenade>& grenades, std::vector<ExplosionEffect>& explosions,
                        std::vector<DamageIndicator>& damageIndicators,
                        SquadOrder& currentOrder, TacticalPing& tacticalPing,
                        HitmarkerFeedback& hitmarker, MissionManager& mission,
                        CutsceneManager& cutscenes, Camera3D& camera, ProceduralAudio& audio,
                        const SaveSystem& saveSystem, const LanNetworkManager& net) {
    player = SourcePlayer();
    weapon = ViewmodelWeapon();
    saveSystem.ApplyLoadoutToWeapon(weapon);

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

        if (net.IsClient()) {
            player.pos = GetSquadSpawnPosition(net.localSlot);
            player.pos.y = 0.5f;

            const char* names[4] = { "Alpha", "Bravo", "Charlie", "Delta" };
            Color colors[4] = { WHITE, SKYBLUE, LIME, GOLD };
            int allyIdx = 1;
            for (int s = 0; s < 4; s++) {
                if (s == net.localSlot) continue;
                Vector3 sp = GetSquadSpawnPosition(s);
                sp.y = 0.0f;
                allies.push_back({ allyIdx++, names[s], sp, sp, sp, 100.0f, 50.0f, 0.5f, 0.0f, colors[s] });
            }
        } else {
            player.pos = { 0.0f, 0.5f, 14.0f };
            allies = {
                { 1, "Bravo",   { -5.5f, 0.0f,  4.0f }, { -5.5f, 0.0f,  4.0f }, { -5.5f, 0.0f,  4.0f }, 100.0f, 50.0f, 0.4f, 0.0f, SKYBLUE },
                { 2, "Charlie", {  5.5f, 0.0f,  4.0f }, {  5.5f, 0.0f,  4.0f }, {  5.5f, 0.0f,  4.0f }, 100.0f, 50.0f, 0.9f, 2.1f, LIME },
                { 3, "Delta",   {  9.0f, 0.0f, -2.0f }, {  9.0f, 0.0f, -2.0f }, {  9.0f, 0.0f, -2.0f }, 100.0f, 50.0f, 1.3f, 4.2f, GOLD }
            };
        }

        ResetWave(enemies);

        int phase = net.IsClient() && net.hasSnapshot ? (int)net.latestSnapshot.phase : saveSystem.data.selectedPhase;
        if (phase >= 2) {
            int nextId = (int)enemies.size() + 1;
            enemies.push_back({ nextId++, { -11.5f, 0.0f, -6.5f }, { -11.5f, 0.0f, -6.5f }, 110.0f, 110.0f, 0.7f, 0.5f, 0.0f, false, -1 });
            enemies.push_back({ nextId++, {  11.5f, 0.0f, -6.5f }, {  11.5f, 0.0f, -6.5f }, 110.0f, 110.0f, 0.9f, 1.5f, 0.0f, false, -1 });
        }
        if (phase >= 3) {
            int nextId = (int)enemies.size() + 1;
            enemies.push_back({ nextId++, { 0.0f, 0.0f, -15.5f }, { 0.0f, 0.0f, -15.5f }, 140.0f, 140.0f, 0.6f, 2.2f, 0.0f, false, -1 });
            for (auto& e : enemies) {
                e.maxHp += 25.0f;
                e.hp = e.maxHp;
            }
        }

        mission.SetupForMode(true, false);
    }

    Vector3 eye = player.GetEyePosition();
    Vector3 target = Vector3Add(eye, player.GetForward3D());
    camera.position = eye;
    camera.target = target;
    camera.fovy = saveSystem.data.baseFov;

    bool playIntroCutscene = (mode != GameMode::MOVEMENT_TEST && mode != GameMode::SUPPRESSION_RANGE && !net.IsActive());
    cutscenes.SetupForMode(playIntroCutscene, eye, target);
    if (playIntroCutscene) {
        cutscenes.Play(0, eye, target, saveSystem.data.baseFov, audio);
    }
}

Vector3 ApplySpreadToDirection(Vector3 dir, float spread) {
    if (spread <= 0.0f) return dir;
    float rx = ((float)GetRandomValue(-1000, 1000) / 1000.0f) * spread;
    float ry = ((float)GetRandomValue(-1000, 1000) / 1000.0f) * spread;
    float rz = ((float)GetRandomValue(-1000, 1000) / 1000.0f) * spread;
    return Vector3Normalize({ dir.x + rx, dir.y + ry, dir.z + rz });
}

WeaponStats GetWeaponStatsById(WeaponId id) {
    ViewmodelWeapon temp;
    for (int slot = 1; slot <= 5; slot++) {
        temp.SelectSlot(slot);
        for (int sub = 0; sub < 4; sub++) {
            if (temp.GetCurrentStats().id == id) {
                return temp.GetCurrentStats();
            }
            temp.SelectSlot(slot);
        }
    }
    temp.SelectSlot(1);
    return temp.GetCurrentStats();
}

int main() {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(1280, 720, "Squad Tactical Shooter - Multiplayer LAN UDP (SAVEPOINT_19)");

    // Auto-ajusta para a resolução nativa do monitor e ativa Tela Cheia
    int monitor = GetCurrentMonitor();
    int nativeW = GetMonitorWidth(monitor);
    int nativeH = GetMonitorHeight(monitor);
    if (nativeW > 0 && nativeH > 0) {
        SetWindowSize(nativeW, nativeH);
    }
    if (!IsWindowFullscreen()) {
        ToggleFullscreen();
    }

    SetTargetFPS(144);
    SetExitKey(KEY_NULL);

    SaveSystem saveSystem;
    saveSystem.Load();

    ProceduralAudio audio;
    audio.Init();
    saveSystem.ApplyAudioSettings(audio);

    AssetManager assets;
    assets.Init();

    LanNetworkManager net;
    MainMenu menu;
    InGameMapEditor mapEditor;
    CutsceneManager cutscenes;
    GameMode currentMode = GameMode::MAIN_MENU;
    bool shouldQuit = false;
    bool victoryRecordedForRun = false;
    uint32_t hostSnapshotTick = 0;

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
    camera.fovy = saveSystem.data.baseFov;
    camera.projection = CAMERA_PERSPECTIVE;

    while (!WindowShouldClose() && !shouldQuit) {
        if (IsKeyPressed(KEY_F11)) {
            if (IsWindowFullscreen()) {
                ToggleFullscreen();
                SetWindowSize(1280, 720);
            } else {
                int mon = GetCurrentMonitor();
                int mw = GetMonitorWidth(mon);
                int mh = GetMonitorHeight(mon);
                if (mw > 0 && mh > 0) SetWindowSize(mw, mh);
                ToggleFullscreen();
            }
        }

        int largura = GetScreenWidth();
        int altura = GetScreenHeight();
        float dt = std::min(GetFrameTime(), 0.05f);
        animTime += dt;

        if (reloadAssetsBannerTimer > 0.0f) reloadAssetsBannerTimer -= dt;
        if (saveSystem.saveBannerTimer > 0.0f) saveSystem.saveBannerTimer -= dt;

        net.PollPackets(dt, (uint8_t)saveSystem.data.selectedPhase);

        if (IsKeyPressed(KEY_M) && !(currentMode == GameMode::MAIN_MENU && menu.screen == MenuScreen::LAN_LOBBY && menu.ipFieldFocused)) {
            audio.ToggleMute();
        }

        if (IsKeyPressed(KEY_F7)) {
            assets.ReloadAll();
            audio.ReloadExternalAssets();
            saveSystem.ApplyAudioSettings(audio);
            reloadAssetsBannerTimer = 2.6f;
            audio.PlayOrder();
        }

        // =====================================================================
        // ESTADO 1: MENU DE INICIALIZAÇÃO, BRIEFING, LOBBY LAN E CONFIGURAÇÕES
        // =====================================================================
        if (currentMode == GameMode::MAIN_MENU) {
            if (IsCursorHidden()) EnableCursor();

            audio.UpdateDynamicMusic(dt, 0.0f, false);

            if (IsKeyPressed(KEY_ESCAPE)) {
                if (menu.IsInSubScreen()) {
                    if (menu.screen == MenuScreen::LAN_LOBBY) net.Shutdown();
                    saveSystem.Save();
                    menu.ReturnToMain();
                    audio.PlayOrder();
                } else {
                    shouldQuit = true;
                    continue;
                }
            }

            GameMode chosen = menu.Update(largura, altura, saveSystem, audio, net);
            if (chosen == GameMode::EXIT_GAME) {
                shouldQuit = true;
                continue;
            } else if (chosen == GameMode::MAP_EDITOR) {
                net.Shutdown();
                currentMode = GameMode::MAP_EDITOR;
                mapEditor.Enter(world);
                audio.PlayOrder();
                DisableCursor();
            } else if (chosen != GameMode::MAIN_MENU) {
                currentMode = chosen;
                victoryRecordedForRun = false;
                hostSnapshotTick = 0;
                InitializeTestMode(currentMode, world, player, weapon, allies, enemies, tracers, grenades, explosions, damageIndicators, currentOrder, tacticalPing, hitmarker, mission, cutscenes, camera, audio, saveSystem, net);
                menu.ReturnToMain();
                DisableCursor();
            }

            BeginDrawing();
                menu.Draw(largura, altura, saveSystem, net);
                const char* audioTag = audio.muted
                    ? "[M] Som: MUDO"
                    : TextFormat("[M] Audio 3D: ON (%d SFX / %d Musicas)", audio.externalSfxCount, audio.externalMusicCount);
                DrawText(audioTag, 24, 16, 14, audio.muted ? RED : GREEN);

                const char* assetTag = TextFormat("[F11] Tela Cheia  |  [F7] .glb: %d/%d (%dx%d)", assets.loadedCount, assets.totalSlots, largura, altura);
                DrawText(assetTag, largura - MeasureText(assetTag, 14) - 24, 16, 14, (assets.loadedCount > 0) ? SKYBLUE : LIGHTGRAY);
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
        // ESTADO 3: MODOS DE TESTE E MISSÃO EM 3D (SINGLEPLAYER OU CO-OP LAN)
        // =====================================================================
        if (IsKeyPressed(KEY_ESCAPE)) {
            cutscenes.Skip();
            net.Shutdown();
            currentMode = GameMode::MAIN_MENU;
            EnableCursor();
            continue;
        }

        if (orderBannerTimer > 0.0f) orderBannerTimer -= dt;
        if (hitmarker.timer > 0.0f) hitmarker.timer -= dt;

        if (IsKeyPressed(KEY_F5) && !net.IsClient()) {
            victoryRecordedForRun = false;
            InitializeTestMode(currentMode, world, player, weapon, allies, enemies, tracers, grenades, explosions, damageIndicators, currentOrder, tacticalPing, hitmarker, mission, cutscenes, camera, audio, saveSystem, net);
        }

        if (IsKeyPressed(KEY_F6) && !cutscenes.IsPlaying() && !net.IsActive()) {
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

        if (mission.state == MissionState::VICTORY && !victoryRecordedForRun) {
            victoryRecordedForRun = true;
            saveSystem.RecordMissionVictory(saveSystem.data.selectedPhase, mission.stats.enemiesEliminated, mission.stats.headshotsLanded);
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

            if (!player.isDowned && !allies.empty() && !net.IsClient()) {
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
            player.UpdateLook(mouseDelta, saveSystem.data.mouseSensitivity);
            player.Update(dt, world);

            if (wasGrounded && !player.isGrounded && player.vel.y > 1.0f) {
                audio.PlayJump();
            }

            if (player.pos.y < -20.0f) {
                player.pos = (currentMode == GameMode::MOVEMENT_TEST)
                    ? Vector3{ 0.0f, 0.5f, 20.0f }
                    : GetSquadSpawnPosition(net.IsClient() ? net.localSlot : 0);
                player.vel = { 0.0f, 0.0f, 0.0f };
            }

            float horizSpeed = sqrtf(player.vel.x * player.vel.x + player.vel.z * player.vel.z);
            float baseFov = saveSystem.data.baseFov;
            float targetFov = (horizSpeed > 7.5f) ? (baseFov + 8.0f) : baseFov;
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

            // =================================================================
            // RAMO A: HOST AUTORITATIVO OU SINGLEPLAYER
            // =================================================================
            if (!net.IsClient()) {
                std::array<bool, 3> savedHumanSlot = { false, false, false };
                std::array<Vector3, 3> savedHumanPos = {};
                std::array<bool, 3> savedHumanCrouched = { false, false, false };

                if (net.IsHost()) {
                    for (int s = 1; s <= 3; s++) {
                        if (!net.clientSlots[s].connected || (s - 1) >= (int)allies.size()) continue;
                        SquadAlly& ally = allies[s - 1];
                        const NetClientInputPacket& inPkt = net.clientSlots[s].latestInput;

                        ally.pos = { inPkt.posX, inPkt.posY, inPkt.posZ };
                        ally.anchorPos = ally.pos;
                        bool crouched = (inPkt.flags & (1 << 0)) != 0;
                        ally.isSuppressed = crouched;
                        ally.hasPingOrder = false;
                        SuppressBotFireTimer(ally);

                        savedHumanSlot[s - 1] = true;
                        savedHumanPos[s - 1] = ally.pos;
                        savedHumanCrouched[s - 1] = crouched;

                        if ((inPkt.flags & (1 << 2)) && !ally.isDowned) {
                            if (player.isDowned && Vector3Distance(ally.pos, player.pos) < 3.0f) {
                                player.reviveProgress += dt * 0.42f;
                                if (player.reviveProgress >= 1.0f) {
                                    player.isDowned = false;
                                    player.hp = 75.0f;
                                    player.reviveProgress = 0.0f;
                                    audio.PlayRevive();
                                }
                            }
                            for (size_t other = 0; other < allies.size(); other++) {
                                if ((int)other == (s - 1) || !allies[other].isDowned) continue;
                                if (Vector3Distance(ally.pos, allies[other].pos) < 3.0f) {
                                    allies[other].reviveProgress += dt * 0.42f;
                                    if (allies[other].reviveProgress >= 1.0f) {
                                        allies[other].isDowned = false;
                                        allies[other].hp = 75.0f;
                                        allies[other].lastHp = 75.0f;
                                        allies[other].reviveProgress = 0.0f;
                                        allies[other].bleedoutTimer = 25.0f;
                                        allies[other].anchorPos = allies[other].pos;
                                        audio.PlayRevive();
                                    }
                                }
                            }
                        }

                        if (net.clientSlots[s].hasPendingShot) {
                            net.clientSlots[s].hasPendingShot = false;
                            if (!ally.isDowned) {
                                WeaponStats cStats = GetWeaponStatsById((WeaponId)inPkt.weaponId);
                                Vector3 cOrigin = { inPkt.shotOriginX, inPkt.shotOriginY, inPkt.shotOriginZ };
                                Vector3 cDir = Vector3Normalize({ inPkt.shotDirX, inPkt.shotDirY, inPkt.shotDirZ });
                                Vector3 cMuzzle = Vector3Add(ally.GetEyePos(), Vector3Scale(cDir, 0.45f));

                                ally.aggro = std::min(100.0f, ally.aggro + cStats.aggroOnFire);

                                for (int p = 0; p < cStats.pelletCount; p++) {
                                    Vector3 pDir = ApplySpreadToDirection(cDir, cStats.spread);
                                    Ray ray = { cOrigin, pDir };
                                    BallisticHitResult hitRes = TraceBallisticShot(ray, cStats.range, cStats.penetrationPower, world, enemies);
                                    float shotDist = Vector3Distance(cOrigin, hitRes.endPoint);

                                    int barrelIdx = mission.CheckBarrelShot(ray, shotDist);
                                    if (barrelIdx != -1) {
                                        RayCollision bCol = GetRayCollisionBox(ray, mission.barrels[barrelIdx].GetAABB());
                                        if (!cStats.isMelee) {
                                            tracers.push_back({ cMuzzle, bCol.point, ally.color, 0.09f });
                                            net.QueueEvent(NetEventType::SHOT_TRACER, (uint8_t)s, cMuzzle, bCol.point);
                                        }
                                        mission.DamageBarrel(barrelIdx, cStats.damage, explosions);
                                        continue;
                                    }

                                    if (!cStats.isMelee) {
                                        tracers.push_back({ cMuzzle, hitRes.endPoint, ally.color, 0.09f });
                                        net.QueueEvent(NetEventType::SHOT_TRACER, (uint8_t)s, cMuzzle, hitRes.endPoint);
                                    }

                                    if (hitRes.hitEnemyIdx != -1) {
                                        EnemyBot& targetEnemy = enemies[hitRes.hitEnemyIdx];
                                        bool wasAlive = targetEnemy.IsAlive();
                                        targetEnemy.hp -= cStats.damage * hitRes.damageMultiplier;
                                        targetEnemy.suppression = std::min(100.0f, targetEnemy.suppression + cStats.suppressionOnHit);
                                        ally.aggro = std::min(100.0f, ally.aggro + cStats.aggroOnHit);
                                        if (wasAlive && !targetEnemy.IsAlive()) {
                                            mission.stats.enemiesEliminated++;
                                        }
                                    }
                                }
                            }
                        }

                        if (net.clientSlots[s].hasPendingGrenade) {
                            net.clientSlots[s].hasPendingGrenade = false;
                            if (!ally.isDowned) {
                                bool isFlash = (inPkt.isFlashbang != 0);
                                Vector3 gPos = { inPkt.nadePosX, inPkt.nadePosY, inPkt.nadePosZ };
                                Vector3 gVel = { inPkt.nadeVelX, inPkt.nadeVelY, inPkt.nadeVelZ };
                                grenades.push_back({ gPos, gVel, isFlash, isFlash ? 1.8f : 2.2f, false });
                            }
                        }

                        if (net.clientSlots[s].hasPendingPing) {
                            net.clientSlots[s].hasPendingPing = false;
                            if (!ally.isDowned) {
                                Vector3 pOrig = { inPkt.shotOriginX, inPkt.shotOriginY, inPkt.shotOriginZ };
                                Vector3 pDir = Vector3Normalize({ inPkt.shotDirX, inPkt.shotDirY, inPkt.shotDirZ });
                                if (ProcessTacticalPing(pOrig, pDir, world, allies, enemies, tacticalPing)) {
                                    audio.PlayOrder();
                                }
                            }
                        }
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
                                        net.QueueEvent(NetEventType::SHOT_TRACER, 0, muzzlePos, bCol.point, isStealthFlank ? 1 : 0);
                                    }
                                    mission.DamageBarrel(barrelIdx, wStats.damage, explosions);
                                    anyHit = true;
                                    hitBarrel = true;
                                    continue;
                                }

                                if (!wStats.isMelee) {
                                    Color shotColor = isStealthFlank ? SKYBLUE : (hitRes.didWallbang ? ORANGE : wStats.accentColor);
                                    tracers.push_back({ muzzlePos, hitRes.endPoint, shotColor, 0.085f });
                                    net.QueueEvent(NetEventType::SHOT_TRACER, 0, muzzlePos, hitRes.endPoint, isStealthFlank ? 1 : 0);

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
                    net.QueueEvent(NetEventType::EXPLOSION, 0, exp.pos, exp.pos, exp.isFlashbang ? 1 : 0);
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

                    if (net.IsHost()) {
                        for (size_t t = prevTracers; t < tracers.size(); ) {
                            bool fromHuman = false;
                            for (int idx = 0; idx < 3 && idx < (int)allies.size(); idx++) {
                                if (savedHumanSlot[idx] && Vector3Distance(tracers[t].start, allies[idx].GetEyePos()) < 1.1f) {
                                    fromHuman = true;
                                    break;
                                }
                            }
                            if (fromHuman) tracers.erase(tracers.begin() + t);
                            else t++;
                        }

                        for (int idx = 0; idx < 3 && idx < (int)allies.size(); idx++) {
                            if (savedHumanSlot[idx]) {
                                allies[idx].pos = savedHumanPos[idx];
                                allies[idx].anchorPos = savedHumanPos[idx];
                                allies[idx].isSuppressed = savedHumanCrouched[idx];
                                allies[idx].hasPingOrder = false;
                            }
                        }
                    }

                    UpdateEnemies(dt, enemies, player, allies, world, tracers, damageIndicators);
                }

                for (size_t t = prevTracers; t < tracers.size(); t++) {
                    bool isEnemyShot = (tracers[t].color.r > 180 && tracers[t].color.b < 80);
                    SfxId shotSfx = isEnemyShot ? SfxId::RIFLE : SfxId::LMG;
                    float pitch = isEnemyShot ? 0.93f : 1.07f;
                    audio.Play3D(shotSfx, tracers[t].start, 0.68f, pitch, 48.0f);
                    net.QueueEvent(NetEventType::SHOT_TRACER, isEnemyShot ? 99 : 1, tracers[t].start, tracers[t].end);
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
            }
            // =================================================================
            // RAMO B: CLIENTE LAN (ENVIA INPUTS E APLICA SNAPSHOT DO HOST)
            // =================================================================
            else {
                weapon.Update(dt, mouseDelta, horizSpeed, player.isGrounded);
                camera.position = eyePos;
                camera.target = Vector3Add(eyePos, forward);

                if (IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE) && !player.isDowned) {
                    net.QueueClientPing();
                    audio.PlayOrder();
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

                        if (wStats.isGrenade) {
                            bool isFlash = (wStats.id == WeaponId::GADGET_FLASH);
                            Vector3 spawnPos = Vector3Add(eyePos, Vector3Scale(forward, 0.65f));
                            Vector3 throwVel = Vector3Add(Vector3Scale(forward, 17.5f), { 0.0f, 4.2f, 0.0f });
                            throwVel = Vector3Add(throwVel, Vector3Scale(player.vel, 0.35f));
                            grenades.push_back({ spawnPos, throwVel, isFlash, isFlash ? 1.8f : 2.2f, false });
                            net.QueueClientGrenade(isFlash, spawnPos, throwVel);
                        } else {
                            Vector3 muzzlePos = Vector3Add(eyePos, Vector3Add(Vector3Scale(listenerRight, 0.24f), { 0, -0.18f, 0 }));
                            BallisticHitResult previewHit = TraceBallisticShot({ eyePos, forward }, wStats.range, wStats.penetrationPower, world, enemies);
                            if (!wStats.isMelee) {
                                Color shotColor = isStealthFlank ? SKYBLUE : wStats.accentColor;
                                tracers.push_back({ muzzlePos, previewHit.endPoint, shotColor, 0.085f });
                            }
                            if (previewHit.hitEnemyIdx != -1) {
                                hitmarker.timer = hitmarker.maxDuration;
                                hitmarker.color = previewHit.isHeadshot ? GOLD : RAYWHITE;
                                hitmarker.callout = previewHit.isHeadshot ? "HEADSHOT!" : "";
                            }
                            net.QueueClientFire(wStats.id, wStats.isMelee, eyePos, forward);
                        }
                    } else if (!weapon.isReloading && !wStats.isMelee && !wStats.isGrenade && weapon.GetCurrentAmmo() <= 0) {
                        if (weapon.StartReload()) {
                            audio.PlayReload();
                        }
                    }
                }

                UpdateGrenades(dt, grenades, explosions, world);
                grenades.erase(
                    std::remove_if(grenades.begin(), grenades.end(), [](const ThrownGrenade& g) { return g.exploded; }),
                    grenades.end()
                );
                for (size_t i = 0; i < explosions.size(); ) {
                    explosions[i].timer -= dt;
                    if (explosions[i].timer <= 0.0f) explosions.erase(explosions.begin() + i);
                    else i++;
                }

                net.SendClientInput(dt, player, weapon.currentSlot, IsKeyDown(KEY_F));
            }

            for (size_t i = 0; i < tracers.size(); ) {
                tracers[i].timer -= dt;
                if (tracers[i].timer <= 0.0f) tracers.erase(tracers.begin() + i);
                else i++;
            }
        } else if (inCutscene) {
            GetMouseDelta();
            cutscenes.ApplyToCamera(camera);
            Vector3 camFwd = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
            Vector3 camRight = Vector3Normalize(Vector3CrossProduct(camFwd, camera.up));
            audio.UpdateListener(camera.position, camFwd, camRight);
        }

        // =====================================================================
        // SINCRONIZAÇÃO DE REDE (HOST EMPACOTA SNAPSHOT / CLIENTE APLICA)
        // =====================================================================
        if (net.IsHost()) {
            NetWorldSnapshotPacket snap = {};
            snap.phase = (uint8_t)saveSystem.data.selectedPhase;
            snap.missionState = (uint8_t)mission.state;
            snap.currentOrder = (uint8_t)currentOrder;
            snap.extractionActive = mission.extraction.isActive ? 1 : 0;
            snap.tick = ++hostSnapshotTick;
            snap.extractionProgress = ReadExtractionProgress(mission.extraction);
            snap.missionTimer = ReadMissionTimer(mission);

            uint8_t mask = 1;
            for (int s = 1; s <= 3; s++) {
                if (net.clientSlots[s].connected) mask |= (1 << s);
            }
            snap.connectedMask = mask;

            snap.squad[0].posX = player.pos.x;
            snap.squad[0].posY = player.pos.y;
            snap.squad[0].posZ = player.pos.z;
            snap.squad[0].yaw = player.yaw;
            snap.squad[0].pitch = player.pitch;
            snap.squad[0].hp = player.hp;
            snap.squad[0].aggro = player.aggro;
            snap.squad[0].suppression = 0.0f;
            snap.squad[0].bleedoutTimer = std::max(0.0f, 25.0f - mission.playerDownedTimer);
            snap.squad[0].reviveProgress = player.reviveProgress;
            snap.squad[0].weaponSlot = (uint8_t)weapon.currentSlot;
            snap.squad[0].flags = (1 << 0)
                                | (player.isCrouched ? (1 << 1) : 0)
                                | (player.isDowned ? (1 << 2) : 0);

            for (int s = 1; s <= 3 && (s - 1) < (int)allies.size(); s++) {
                const SquadAlly& a = allies[s - 1];
                snap.squad[s].posX = a.pos.x;
                snap.squad[s].posY = a.pos.y;
                snap.squad[s].posZ = a.pos.z;
                snap.squad[s].hp = a.hp;
                snap.squad[s].aggro = a.aggro;
                snap.squad[s].suppression = a.suppression;
                snap.squad[s].bleedoutTimer = a.bleedoutTimer;
                snap.squad[s].reviveProgress = a.reviveProgress;
                snap.squad[s].flags = (net.clientSlots[s].connected ? (1 << 0) : 0)
                                    | (a.isSuppressed ? (1 << 1) : 0)
                                    | (a.isDowned ? (1 << 2) : 0)
                                    | (a.isSuppressed ? (1 << 3) : 0)
                                    | (a.hasPingOrder ? (1 << 4) : 0);
            }

            int eCount = std::min((int)enemies.size(), NetWorldSnapshotPacket::MAX_ENEMIES);
            snap.enemyCount = (uint8_t)eCount;
            for (int i = 0; i < eCount; i++) {
                snap.enemies[i].id = (int16_t)enemies[i].id;
                snap.enemies[i].currentTargetId = (int16_t)enemies[i].currentTargetId;
                snap.enemies[i].posX = enemies[i].pos.x;
                snap.enemies[i].posY = enemies[i].pos.y;
                snap.enemies[i].posZ = enemies[i].pos.z;
                snap.enemies[i].hp = enemies[i].hp;
                snap.enemies[i].maxHp = enemies[i].maxHp;
                snap.enemies[i].suppression = enemies[i].suppression;
                snap.enemies[i].flags = (enemies[i].IsAlive() ? (1 << 0) : 0)
                                      | (enemies[i].isSuppressed ? (1 << 1) : 0);
            }

            int tCount = std::min((int)mission.terminals.size(), NetWorldSnapshotPacket::MAX_TERMINALS);
            snap.terminalCount = (uint8_t)tCount;
            for (int i = 0; i < tCount; i++) {
                snap.terminals[i].progress = mission.terminals[i].progress;
                snap.terminals[i].hacked = mission.terminals[i].isHacked ? 1 : 0;
            }

            int bCount = std::min((int)mission.barrels.size(), NetWorldSnapshotPacket::MAX_BARRELS);
            snap.barrelCount = (uint8_t)bCount;
            for (int i = 0; i < bCount; i++) {
                snap.barrels[i].hp = mission.barrels[i].hp;
                snap.barrels[i].exploded = mission.barrels[i].exploded ? 1 : 0;
            }

            int evCount = std::min((int)net.outgoingEvents.size(), NetWorldSnapshotPacket::MAX_EVENTS);
            snap.eventCount = (uint8_t)evCount;
            for (int i = 0; i < evCount; i++) {
                snap.events[i] = net.outgoingEvents[net.outgoingEvents.size() - evCount + i];
            }

            net.SendHostSnapshot(dt, snap);
        } else if (net.IsClient() && net.hasSnapshot) {
            const NetWorldSnapshotPacket& snap = net.latestSnapshot;

            if ((SquadOrder)snap.currentOrder != currentOrder) {
                currentOrder = (SquadOrder)snap.currentOrder;
                orderBannerTimer = 2.0f;
                audio.PlayOrder();
            }

            mission.state = (MissionState)snap.missionState;
            mission.extraction.isActive = (snap.extractionActive != 0);
            WriteExtractionProgress(mission.extraction, snap.extractionProgress);
            WriteMissionTimer(mission, snap.missionTimer);

            if (net.localSlot >= 1 && net.localSlot <= 3) {
                const NetSquadMemberState& myNet = snap.squad[net.localSlot];
                bool wasDown = player.isDowned;
                player.hp = myNet.hp;
                player.aggro = myNet.aggro;
                player.isDowned = (myNet.flags & (1 << 2)) != 0;
                player.reviveProgress = myNet.reviveProgress;
                mission.playerDownedTimer = 25.0f - myNet.bleedoutTimer;

                if (!wasDown && player.isDowned) audio.PlayDownedAlert();
                else if (wasDown && !player.isDowned) audio.PlayRevive();
            }

            int allyIdx = 0;
            for (int s = 0; s < 4 && allyIdx < (int)allies.size(); s++) {
                if (s == net.localSlot) continue;
                const NetSquadMemberState& st = snap.squad[s];
                SquadAlly& a = allies[allyIdx++];
                a.pos = { st.posX, st.posY, st.posZ };
                a.anchorPos = a.pos;
                a.hp = st.hp;
                a.aggro = st.aggro;
                a.suppression = st.suppression;
                a.bleedoutTimer = st.bleedoutTimer;
                a.reviveProgress = st.reviveProgress;
                a.isDowned = (st.flags & (1 << 2)) != 0;
                a.isSuppressed = ((st.flags & (1 << 1)) != 0) || ((st.flags & (1 << 3)) != 0);
                a.hasPingOrder = (st.flags & (1 << 4)) != 0;
            }

            if ((int)enemies.size() != (int)snap.enemyCount) {
                enemies.resize(snap.enemyCount);
            }
            for (int i = 0; i < (int)snap.enemyCount; i++) {
                const NetEnemyState& ne = snap.enemies[i];
                enemies[i].id = ne.id;
                enemies[i].currentTargetId = ne.currentTargetId;
                enemies[i].pos = { ne.posX, ne.posY, ne.posZ };
                enemies[i].hp = ne.hp;
                enemies[i].maxHp = ne.maxHp;
                enemies[i].suppression = ne.suppression;
                enemies[i].isSuppressed = (ne.flags & (1 << 1)) != 0;
            }

            for (int i = 0; i < (int)snap.terminalCount && i < (int)mission.terminals.size(); i++) {
                mission.terminals[i].progress = snap.terminals[i].progress;
                mission.terminals[i].isHacked = (snap.terminals[i].hacked != 0);
            }
            for (int i = 0; i < (int)snap.barrelCount && i < (int)mission.barrels.size(); i++) {
                mission.barrels[i].hp = snap.barrels[i].hp;
                mission.barrels[i].exploded = (snap.barrels[i].exploded != 0);
            }

            for (int i = 0; i < (int)snap.eventCount; i++) {
                const NetWorldEvent& ev = snap.events[i];
                if (ev.eventId <= net.lastProcessedEventId) continue;
                net.lastProcessedEventId = ev.eventId;

                if (ev.type == (uint8_t)NetEventType::SHOT_TRACER && ev.sourceSlot != (uint8_t)net.localSlot) {
                    Vector3 sPos = { ev.startX, ev.startY, ev.startZ };
                    Vector3 ePos = { ev.endX, ev.endY, ev.endZ };
                    Color col = (ev.sourceSlot == 99) ? RED : ((ev.extra != 0) ? SKYBLUE : GOLD);
                    tracers.push_back({ sPos, ePos, col, 0.09f });
                    audio.Play3D((ev.sourceSlot == 99) ? SfxId::RIFLE : SfxId::LMG, sPos, 0.65f, 1.0f, 48.0f);
                } else if (ev.type == (uint8_t)NetEventType::EXPLOSION) {
                    Vector3 expPos = { ev.startX, ev.startY, ev.startZ };
                    bool isFlash = (ev.extra != 0);
                    explosions.push_back({ expPos, isFlash, 0.45f, 0.45f, isFlash ? 8.5f : 6.0f });
                    audio.PlayExplosion3D(expPos, isFlash);
                }
            }
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
                            : allies[std::min((int)allies.size() - 1, std::max(0, enemy.currentTargetId - 1))].GetEyePos();

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
            // HUD 2D
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
                    DrawText("1-5: Armas | Shift: Correr | F5: Reiniciar | F6: Cutscene | F7: Reload | F11: Tela Cheia | ESC: Menu",
                             20, altura - 26, 14, LIGHTGRAY);
                } else {
                    if (tacticalPing.type == PingType::MOVE_COVER && !allies.empty()) {
                        Vector3 beaconTop = { tacticalPing.worldPos.x, 2.15f, tacticalPing.worldPos.z };
                        if (Vector3DotProduct(forward, Vector3Subtract(beaconTop, eyePos)) > 0.0f) {
                            Vector2 pScreen = GetWorldToScreen(beaconTop, camera);
                            if (pScreen.x > 0 && pScreen.x < largura && pScreen.y > 0 && pScreen.y < altura) {
                                int aIdx = std::clamp(tacticalPing.assignedAllyId - 1, 0, (int)allies.size() - 1);
                                const char* pingTag = TextFormat("COBERTURA [%s]", allies[aIdx].name.c_str());
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

                    const char* netBadge = net.IsHost()
                        ? TextFormat("ESQUADRAO [LAN HOST %s - %d/4]:", net.detectedLocalIp.c_str(), net.GetConnectedPlayerCount())
                        : (net.IsClient()
                            ? TextFormat("ESQUADRAO [LAN CLIENTE %s - %d/4]:", LanNetworkManager::GetSlotCallsign(net.localSlot), net.GetConnectedPlayerCount())
                            : TextFormat("ESQUADRAO [FASE 0%d]:", saveSystem.data.selectedPhase));
                    DrawText(netBadge, 20, 20, 16, net.IsActive() ? SKYBLUE : WHITE);

                    const char* myTag = net.IsClient() ? LanNetworkManager::GetSlotCallsign(net.localSlot) : "Alpha";
                    DrawText(TextFormat("Voce (%s): HP %.0f | Aggro %.0f%% %s", myTag, player.hp, player.aggro, player.isDowned ? "[CAIDO]" : ""),
                             20, 44, 16, player.isDowned ? RED : aggroBarColor);

                    for (size_t i = 0; i < allies.size(); i++) {
                        const auto& a = allies[i];
                        int globalSlot = net.IsClient() ? ((int)i >= net.localSlot ? (int)i + 1 : (int)i) : ((int)i + 1);
                        const char* ctrlTag = net.IsActive() ? (net.IsSlotHuman(globalSlot) ? "[JOGADOR]" : "[BOT]") : "";
                        const char* tagExtra = a.isSuppressed ? "[SUPRIMIDO]" : (a.hasPingOrder ? "[PING: COBERTURA]" : ctrlTag);
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

                    DrawText("1-5: Armas | F1-F3: Ordens | E/F: Interagir | F5: Reiniciar | F11: Tela Cheia | ESC: Menu",
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
                    Color slotCol = active ? wStats.accentColor : ((s == (saveSystem.data.gadgetIndex == 1 ? 5 : 4)) ? LIME : GRAY);
                    DrawText(slotNames[s - 1], hudX + 10 + (s - 1) * 63, hudY + 8, 12, slotCol);
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

            cutscenes.DrawHUD(largura, altura);
        EndDrawing();
    }

    net.Shutdown();
    saveSystem.Save();
    assets.Shutdown();
    audio.Shutdown();
    CloseWindow();
    return 0;
}