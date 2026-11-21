#include "World.hpp"
#include "Player.hpp"
#include "Weapon.hpp"
#include "SquadAI.hpp"
#include "Menu.hpp"
#include "Audio.hpp"

void InitializeTestMode(GameMode mode, std::vector<WorldBlock>& world, SourcePlayer& player,
                        std::vector<SquadAlly>& allies, std::vector<EnemyBot>& enemies,
                        std::vector<Tracer>& tracers, SquadOrder& currentOrder) {
    player = SourcePlayer();
    tracers.clear();
    allies.clear();
    enemies.clear();
    currentOrder = SquadOrder::HOLD;

    if (mode == GameMode::MOVEMENT_TEST) {
        world = LevelLoader::LoadFromFile("mapa_movimento.txt", MapPreset::MOVEMENT_COURSE);
        player.pos = { 0.0f, 0.5f, 20.0f };
        player.yaw = PI;
    } else if (mode == GameMode::SUPPRESSION_RANGE) {
        world = LevelLoader::LoadFromFile("mapa_treino.txt", MapPreset::FIRING_RANGE);
        player.pos = { 0.0f, 0.5f, 14.0f };
        allies = {
            { 1, "Bravo (Tanque)",    { -4.5f, 0.0f, 6.5f }, { -4.5f, 0.0f, 6.5f }, { -4.5f, 0.0f, 6.5f }, 100.0f, 65.0f, 0.4f, 0.0f, SKYBLUE },
            { 2, "Charlie (Suporte)", {  4.5f, 0.0f, 6.5f }, {  4.5f, 0.0f, 6.5f }, {  4.5f, 0.0f, 6.5f }, 100.0f, 55.0f, 0.9f, 2.1f, LIME }
        };
        ResetWave(enemies);
    } else {
        world = LevelLoader::LoadFromFile("mapa_squad.txt", MapPreset::SQUAD_ARENA);
        player.pos = { 0.0f, 0.5f, 14.0f };
        allies = {
            { 1, "Bravo (Tanque)",    { -5.5f, 0.0f,  4.0f }, { -5.5f, 0.0f,  4.0f }, { -5.5f, 0.0f,  4.0f }, 100.0f, 65.0f, 0.4f, 0.0f, SKYBLUE },
            { 2, "Charlie (Suporte)", {  5.5f, 0.0f,  4.0f }, {  5.5f, 0.0f,  4.0f }, {  5.5f, 0.0f,  4.0f }, 100.0f, 55.0f, 0.9f, 2.1f, LIME },
            { 3, "Delta (Flanco)",    {  9.0f, 0.0f, -2.0f }, {  9.0f, 0.0f, -2.0f }, {  9.0f, 0.0f, -2.0f }, 100.0f, 30.0f, 1.3f, 4.2f, GOLD }
        };
        ResetWave(enemies);
    }
}

int main() {
    const int largura = 1280;
    const int altura = 720;
    InitWindow(largura, altura, "Squad Tactical Shooter - Audio Procedural (SAVEPOINT_09)");
    SetTargetFPS(144);
    SetExitKey(KEY_NULL);

    ProceduralAudio audio;
    audio.Init();

    MainMenu menu;
    GameMode currentMode = GameMode::MAIN_MENU;
    bool shouldQuit = false;

    std::vector<WorldBlock> world;
    SourcePlayer player;
    ViewmodelWeapon weapon;
    std::vector<SquadAlly> allies;
    std::vector<EnemyBot> enemies;
    std::vector<Tracer> tracers;

    SquadOrder currentOrder = SquadOrder::HOLD;
    float orderBannerTimer = 0.0f;

    Camera3D camera = {};
    camera.up = { 0.0f, 1.0f, 0.0f };
    camera.fovy = 75.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    while (!WindowShouldClose() && !shouldQuit) {
        float dt = std::min(GetFrameTime(), 0.05f);

        if (IsKeyPressed(KEY_M)) {
            audio.ToggleMute();
        }

        // =====================================================================
        // ESTADO 1: MENU DE INICIALIZAÇÃO
        // =====================================================================
        if (currentMode == GameMode::MAIN_MENU) {
            if (IsCursorHidden()) EnableCursor();

            if (IsKeyPressed(KEY_ESCAPE)) {
                shouldQuit = true;
                continue;
            }

            GameMode chosen = menu.Update(largura, altura);
            if (chosen == GameMode::EXIT_GAME) {
                shouldQuit = true;
                continue;
            } else if (chosen != GameMode::MAIN_MENU) {
                currentMode = chosen;
                InitializeTestMode(currentMode, world, player, allies, enemies, tracers, currentOrder);
                audio.PlayOrder();
                DisableCursor();
            }

            BeginDrawing();
                menu.Draw(largura, altura);
                const char* audioTag = audio.muted ? "[M] Som: MUDO" : "[M] Som: PROCEDURAL ATIVO";
                DrawText(audioTag, 24, 24, 16, audio.muted ? RED : GREEN);
            EndDrawing();
            continue;
        }

        // =====================================================================
        // ESTADO 2: MODOS DE TESTE EM 3D
        // =====================================================================
        if (IsKeyPressed(KEY_ESCAPE)) {
            currentMode = GameMode::MAIN_MENU;
            EnableCursor();
            continue;
        }

        if (orderBannerTimer > 0.0f) orderBannerTimer -= dt;

        if (IsKeyPressed(KEY_R)) {
            InitializeTestMode(currentMode, world, player, allies, enemies, tracers, currentOrder);
            audio.PlayOrder();
        }

        if (IsKeyPressed(KEY_T) && currentMode != GameMode::MOVEMENT_TEST) {
            ResetWave(enemies);
            audio.PlayOrder();
        }

        // Comandos Táticos de Esquadrão (1, 2 e 3)
        if (!player.isDowned && !allies.empty()) {
            if (IsKeyPressed(KEY_ONE)) {
                currentOrder = SquadOrder::ADVANCE;
                orderBannerTimer = 2.2f;
                audio.PlayOrder();
            } else if (IsKeyPressed(KEY_TWO)) {
                currentOrder = SquadOrder::HOLD;
                orderBannerTimer = 2.2f;
                audio.PlayOrder();
                for (auto& ally : allies) {
                    if (!ally.isDowned) ally.anchorPos = ally.pos;
                }
            } else if (IsKeyPressed(KEY_THREE)) {
                currentOrder = SquadOrder::SUPPRESS;
                orderBannerTimer = 2.2f;
                audio.PlayOrder();
                for (auto& ally : allies) {
                    if (!ally.isDowned) ally.anchorPos = ally.pos;
                }
            }
        }

        // Detecta salto do jogador para disparar o som de pulo
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

        for (auto& ally : allies) {
            ally.isBeingDragged = false;
        }

        // Mecânica de Arrastar (Segurar E) e Reanimar (Segurar F)
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

        weapon.Update(dt, mouseDelta, horizSpeed, player.isGrounded);

        Vector3 eyePos = player.GetEyePosition();
        Vector3 forward = player.GetForward3D();
        camera.position = eyePos;
        camera.target = Vector3Add(eyePos, forward);

        float maxAllyAggro = 0.0f;
        for (const auto& ally : allies) {
            if (!ally.isDowned && ally.aggro > maxAllyAggro) maxAllyAggro = ally.aggro;
        }
        bool isStealthFlank = (!allies.empty() && !player.isDowned && player.aggro < 25.0f && maxAllyAggro > 50.0f);

        // Disparo do Jogador
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            weapon.TriggerFire();
            audio.PlayShoot(isStealthFlank);

            if (!player.isDowned && !enemies.empty()) {
                player.aggro = std::min(100.0f, player.aggro + 12.0f);
            }

            Ray ray = { eyePos, forward };
            float hitDist = GetClosestWallDistance(ray, world, 80.0f);
            int hitEnemyIdx = -1;

            for (size_t i = 0; i < enemies.size(); i++) {
                if (!enemies[i].IsAlive()) continue;
                RayCollision col = GetRayCollisionBox(ray, enemies[i].GetAABB());
                if (col.hit && col.distance < hitDist) {
                    hitDist = col.distance;
                    hitEnemyIdx = (int)i;
                }
            }

            Vector3 right = Vector3Normalize(Vector3CrossProduct(forward, camera.up));
            Vector3 muzzlePos = Vector3Add(eyePos, Vector3Add(Vector3Scale(right, 0.24f), { 0, -0.18f, 0 }));
            Vector3 endPoint = Vector3Add(eyePos, Vector3Scale(forward, hitDist));

            Color shotColor = isStealthFlank ? SKYBLUE : GOLD;
            tracers.push_back({ muzzlePos, endPoint, shotColor, 0.09f });

            if (hitEnemyIdx != -1) {
                float damage = isStealthFlank ? 70.0f : (player.isDowned ? 22.0f : 35.0f);
                enemies[hitEnemyIdx].hp -= damage;
                if (!player.isDowned) {
                    player.aggro = std::min(100.0f, player.aggro + 18.0f);
                }
            }
        }

        // Decaimento natural do Aggro
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

        // Monitora mudanças de estado DOWNED para disparar alertas sonoros
        bool prevPlayerDowned = player.isDowned;
        std::vector<bool> prevAllyDowned(allies.size(), false);
        for (size_t i = 0; i < allies.size(); i++) {
            prevAllyDowned[i] = allies[i].isDowned;
        }

        if (!allies.empty() || !enemies.empty()) {
            UpdateAllies(dt, allies, enemies, player, currentOrder, world, tracers);
            UpdateEnemies(dt, enemies, player, allies, world, tracers);
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

        // Renderização 3D
        BeginDrawing();
            ClearBackground({ 20, 23, 30, 255 });

            BeginMode3D(camera);
                for (const auto& block : world) {
                    DrawCubeV(block.pos, block.size, block.color);
                    DrawCubeWiresV(block.pos, block.size, BLACK);
                }

                for (const auto& ally : allies) {
                    if (ally.isDowned) {
                        DrawCube({ ally.pos.x, ally.pos.y + 0.3f, ally.pos.z }, 0.85f, 0.55f, 1.5f, Fade(ally.color, 0.7f));
                        DrawCubeWires({ ally.pos.x, ally.pos.y + 0.3f, ally.pos.z }, 0.85f, 0.55f, 1.5f, RED);
                    } else if (ally.isSuppressed) {
                        DrawCube({ ally.pos.x, ally.pos.y + 0.625f, ally.pos.z }, 0.85f, 1.25f, 0.85f, ally.color);
                        DrawCubeWires({ ally.pos.x, ally.pos.y + 0.625f, ally.pos.z }, 0.85f, 1.25f, 0.85f, ORANGE);
                        DrawCube({ ally.pos.x, ally.pos.y + 1.0f, ally.pos.z - 0.35f }, 0.5f, 0.18f, 0.2f, ORANGE);
                    } else {
                        DrawCube({ ally.pos.x, ally.pos.y + 0.9f, ally.pos.z }, 0.8f, 1.8f, 0.8f, ally.color);
                        DrawCubeWires({ ally.pos.x, ally.pos.y + 0.9f, ally.pos.z }, 0.8f, 1.8f, 0.8f, BLACK);
                        DrawCube({ ally.pos.x, ally.pos.y + 1.5f, ally.pos.z - 0.35f }, 0.5f, 0.2f, 0.2f, DARKBLUE);

                        if (currentOrder == SquadOrder::HOLD || currentOrder == SquadOrder::SUPPRESS) {
                            Color ringColor = (currentOrder == SquadOrder::SUPPRESS) ? ORANGE : SKYBLUE;
                            DrawCircle3D({ ally.anchorPos.x, 0.03f, ally.anchorPos.z }, 0.65f, { 1.0f, 0.0f, 0.0f }, 90.0f, Fade(ringColor, 0.6f));
                        }
                    }
                }

                for (const auto& enemy : enemies) {
                    if (!enemy.IsAlive()) continue;

                    if (enemy.isSuppressed) {
                        DrawCube({ enemy.pos.x, enemy.pos.y + 0.625f, enemy.pos.z }, 1.05f, 1.25f, 1.05f, { 165, 55, 45, 255 });
                        DrawCubeWires({ enemy.pos.x, enemy.pos.y + 0.625f, enemy.pos.z }, 1.05f, 1.25f, 1.05f, YELLOW);
                        DrawCube({ enemy.pos.x, enemy.pos.y + 1.0f, enemy.pos.z + 0.42f }, 0.6f, 0.18f, 0.2f, ORANGE);
                    } else {
                        DrawCube({ enemy.pos.x, enemy.pos.y + 0.95f, enemy.pos.z }, 1.0f, 1.9f, 1.0f, { 210, 45, 55, 255 });
                        DrawCubeWires({ enemy.pos.x, enemy.pos.y + 0.95f, enemy.pos.z }, 1.0f, 1.9f, 1.0f, BLACK);
                        DrawCube({ enemy.pos.x, enemy.pos.y + 1.5f, enemy.pos.z + 0.42f }, 0.6f, 0.2f, 0.2f, MAROON);
                    }

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

                for (const auto& t : tracers) {
                    DrawLine3D(t.start, t.end, t.color);
                }

                weapon.Draw(camera, isStealthFlank);
            EndMode3D();

            // =================================================================
            // HUD ESPECÍFICO POR MODO DE TESTE
            // =================================================================
            if (currentMode == GameMode::MOVEMENT_TEST) {
                float unitsPerSec = horizSpeed * 50.0f;
                Color speedColor = (unitsPerSec > 495.0f) ? GREEN : (player.isSprinting ? SKYBLUE : RAYWHITE);
                const char* postura = player.isCrouched ? "AGACHADO" : (player.isSprinting ? "CORRENDO" : "EM PE");
                const char* arStatus = player.isGrounded ? "NO CHAO" : "NO AR (AIR STRAFE)";

                DrawText("MODO: PISTA DE MOVIMENTACAO (SOURCE PHYSICS)", 20, 20, 18, SKYBLUE);
                DrawText(TextFormat("Posicao: X=%.1f  Y=%.2f  Z=%.1f", player.pos.x, player.pos.y, player.pos.z), 20, 46, 16, LIGHTGRAY);
                DrawText(TextFormat("Estado: %s | %s", postura, arStatus), 20, 68, 16, GOLD);

                DrawText(TextFormat("VELOCIDADE: %.0f u/s", unitsPerSec), largura / 2 - 120, altura - 85, 26, speedColor);
                DrawText("Shift: Correr | Espaco+A/D: Bhop | Espaco+Ctrl: Crouch-Jump | M: Mudo | R: Reiniciar | ESC: Menu",
                         20, altura - 30, 15, LIGHTGRAY);
            } else {
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
                        }
                    }
                }

                for (const auto& enemy : enemies) {
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
                    DrawText(orderCallout, largura / 2 - MeasureText(orderCallout, 20) / 2, 85, 20, bannerCol);
                }

                if (player.isDowned) {
                    DrawRectangleLinesEx({ 0, 0, (float)largura, (float)altura }, 12, Fade(RED, 0.65f));
                    const char* downMsg = "VOCE CAIU! AGUARDANDO RESGATE DE UM ALIADO...";
                    DrawText(downMsg, largura / 2 - MeasureText(downMsg, 22) / 2, altura / 2 + 50, 22, RED);

                    if (player.reviveProgress > 0.0f) {
                        DrawRectangle(largura / 2 - 120, altura / 2 + 85, 240, 18, BLACK);
                        DrawRectangle(largura / 2 - 118, altura / 2 + 87, (int)(236 * player.reviveProgress), 14, GREEN);
                        DrawText("REANIMANDO...", largura / 2 - 55, altura / 2 + 110, 16, GREEN);
                    }
                } else if (player.draggingAllyId != -1) {
                    const char* dragMsg = TextFormat("ARRASTANDO %s PARA COBERTURA (PODE ATIRAR!)", allies[player.draggingAllyId - 1].name.c_str());
                    DrawText(dragMsg, largura / 2 - MeasureText(dragMsg, 18) / 2, altura - 110, 18, YELLOW);
                }

                int barW = 320;
                int barX = largura / 2 - barW / 2;
                DrawRectangle(barX - 2, 18, barW + 4, 24, BLACK);
                Color aggroBarColor = player.isDowned ? GRAY : (isStealthFlank ? SKYBLUE : ((player.aggro > 65.0f) ? RED : ORANGE));
                DrawRectangle(barX, 20, (int)(barW * (player.aggro / 100.0f)), 20, aggroBarColor);

                const char* statusText = player.isDowned
                    ? "CAIDO (AMEACA ZERO)"
                    : (isStealthFlank ? "FURTIVO (2x DANO NOS FLANCOS)" : ((player.aggro > 65.0f) ? "AGGRO MAXIMO" : "AMEACA MODERADA"));
                DrawText(statusText, largura / 2 - MeasureText(statusText, 16) / 2, 48, 16, aggroBarColor);

                DrawText("ESQUADRAO:", 20, 20, 16, WHITE);
                DrawText(TextFormat("Voce (Alpha): HP %.0f | Aggro %.0f%% %s", player.hp, player.aggro, player.isDowned ? "[CAIDO]" : ""),
                         20, 44, 16, player.isDowned ? RED : aggroBarColor);

                for (size_t i = 0; i < allies.size(); i++) {
                    const auto& a = allies[i];
                    const char* allyLine = a.isDowned
                        ? TextFormat("%s: [CAIDO - %.0fs]", a.name.c_str(), a.bleedoutTimer)
                        : TextFormat("%s: HP %.0f | Aggro %.0f%% %s", a.name.c_str(), a.hp, a.aggro, a.isSuppressed ? "[SUPRIMIDO]" : "");
                    Color lineColor = a.isDowned ? RED : (a.isSuppressed ? ORANGE : a.color);
                    DrawText(allyLine, 20, 68 + (int)i * 22, 15, lineColor);
                }

                int cmdX = largura - 340;
                DrawText("COMANDOS DO ESQUADRAO:", cmdX, 20, 16, WHITE);
                DrawText("[1] Avancar", cmdX, 44, 15, (currentOrder == SquadOrder::ADVANCE) ? GREEN : GRAY);
                DrawText("[2] Manter Posicao", cmdX, 66, 15, (currentOrder == SquadOrder::HOLD) ? SKYBLUE : GRAY);
                DrawText("[3] Fogo de Supressao", cmdX, 88, 15, (currentOrder == SquadOrder::SUPPRESS) ? ORANGE : GRAY);

                DrawText(audio.muted ? "[M] Som: MUDO" : "[M] Som: ATIVO", cmdX, 114, 14, audio.muted ? RED : GREEN);

                DrawText("1/2/3: Ordens | E: Arrastar | F: Reanimar | T: Resetar Inimigos | R: Reiniciar | ESC: Menu", 20, altura - 30, 15, LIGHTGRAY);
            }

            DrawCircleLines(largura / 2, altura / 2, 5, isStealthFlank ? SKYBLUE : GREEN);
        EndDrawing();
    }

    audio.Shutdown();
    CloseWindow();
    return 0;
}