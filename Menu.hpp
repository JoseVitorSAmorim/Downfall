#pragma once

#include "raylib.h"
#include "SaveSystem.hpp"
#include "Audio.hpp"
#include "Network.hpp"
#include <vector>
#include <string>
#include <algorithm>
#include <cmath>

enum class GameMode {
    MAIN_MENU = 0,
    SQUAD_MISSION,
    SUPPRESSION_RANGE,
    MOVEMENT_TEST,
    MAP_EDITOR,
    EXIT_GAME
};

enum class MenuScreen {
    MAIN = 0,
    BRIEFING_LOADOUT,
    LAN_LOBBY,
    SETTINGS
};

class MainMenu {
public:
    MenuScreen screen = MenuScreen::MAIN;
    int activeSlider = -1; // 0: Sensibilidade, 1: FOV, 2: SFX, 3: Musica
    int selectedSettingRow = 0;
    bool ipFieldFocused = true;

    bool IsInSubScreen() const {
        return screen != MenuScreen::MAIN;
    }

    void ReturnToMain() {
        screen = MenuScreen::MAIN;
        activeSlider = -1;
    }

    GameMode Update(int width, int height, SaveSystem& saveSystem, ProceduralAudio& audio, LanNetworkManager& net) {
        Vector2 mouse = GetMousePosition();
        bool click = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);

        if (screen == MenuScreen::MAIN) {
            return UpdateMainScreen(width, height, mouse, click, audio);
        } else if (screen == MenuScreen::BRIEFING_LOADOUT) {
            return UpdateBriefingScreen(width, height, mouse, click, saveSystem, audio, net);
        } else if (screen == MenuScreen::LAN_LOBBY) {
            return UpdateLanLobbyScreen(width, height, saveSystem, audio, net);
        } else {
            UpdateSettingsScreen(width, height, mouse, click, saveSystem, audio);
            return GameMode::MAIN_MENU;
        }
    }

    void Draw(int width, int height, const SaveSystem& saveSystem, const LanNetworkManager& net) const {
        ClearBackground({ 14, 17, 23, 255 });

        for (int x = 0; x < width; x += 48) {
            DrawLine(x, 0, x, height, Color{ 22, 27, 36, 255 });
        }
        for (int y = 0; y < height; y += 48) {
            DrawLine(0, y, width, y, Color{ 22, 27, 36, 255 });
        }

        if (screen == MenuScreen::MAIN) {
            DrawMainScreen(width, height, saveSystem);
        } else if (screen == MenuScreen::BRIEFING_LOADOUT) {
            DrawBriefingScreen(width, height, saveSystem);
        } else if (screen == MenuScreen::LAN_LOBBY) {
            DrawLanLobbyScreen(width, height, saveSystem, net);
        } else {
            DrawSettingsScreen(width, height, saveSystem);
        }

        if (saveSystem.saveBannerTimer > 0.0f) {
            const char* msg = "PROGRESSO E CONFIGURACOES SALVOS EM savegame.cfg";
            int tw = MeasureText(msg, 15);
            DrawRectangle(width / 2 - tw / 2 - 14, height - 42, tw + 28, 28, Color{ 16, 42, 28, 235 });
            DrawRectangleLines(width / 2 - tw / 2 - 14, height - 42, tw + 28, 28, GREEN);
            DrawText(msg, width / 2 - tw / 2, height - 35, 15, GREEN);
        }
    }

private:
    static bool DrawButton(Rectangle rect, const char* label, const char* sublabel, Color accent, bool isSelected = false) {
        Vector2 mouse = GetMousePosition();
        bool hovered = CheckCollisionPointRec(mouse, rect);

        Color bg = hovered ? Color{ 34, 42, 56, 245 } : (isSelected ? Color{ 28, 36, 48, 240 } : Color{ 20, 25, 34, 230 });
        Color border = (hovered || isSelected) ? accent : Color{ 55, 65, 82, 255 };

        DrawRectangleRec(rect, bg);
        DrawRectangleLinesEx(rect, (hovered || isSelected) ? 2.0f : 1.2f, border);
        DrawRectangle((int)rect.x, (int)rect.y, 6, (int)rect.height, border);

        int titleY = (sublabel && sublabel[0] != '\0') ? (int)rect.y + 10 : (int)(rect.y + rect.height * 0.5f - 9);
        DrawText(label, (int)rect.x + 18, titleY, 18, (hovered || isSelected) ? WHITE : RAYWHITE);

        if (sublabel && sublabel[0] != '\0') {
            DrawText(sublabel, (int)rect.x + 18, (int)rect.y + 33, 13, hovered ? accent : LIGHTGRAY);
        }

        return hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    }

    // =========================================================================
    // TELA 1: MENU PRINCIPAL
    // =========================================================================
    GameMode UpdateMainScreen(int width, int height, Vector2 mouse, bool click, ProceduralAudio& audio) {
        (void)height;
        (void)mouse;
        (void)click;
        int cx = width / 2;
        float btnW = 480.0f;
        float btnX = cx - btnW * 0.5f;
        float startY = 188.0f;
        float gap = 64.0f;

        if (IsKeyPressed(KEY_ENTER)) {
            screen = MenuScreen::BRIEFING_LOADOUT;
            audio.PlayOrder();
            return GameMode::MAIN_MENU;
        }

        if (CheckCollisionPointRec(GetMousePosition(), { btnX, startY + gap * 0, btnW, 54.0f }) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            screen = MenuScreen::BRIEFING_LOADOUT;
            audio.PlayOrder();
            return GameMode::MAIN_MENU;
        }
        if (CheckCollisionPointRec(GetMousePosition(), { btnX, startY + gap * 1, btnW, 54.0f }) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            screen = MenuScreen::LAN_LOBBY;
            audio.PlayOrder();
            return GameMode::MAIN_MENU;
        }
        if (CheckCollisionPointRec(GetMousePosition(), { btnX, startY + gap * 2, btnW, 54.0f }) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            return GameMode::SUPPRESSION_RANGE;
        }
        if (CheckCollisionPointRec(GetMousePosition(), { btnX, startY + gap * 3, btnW, 54.0f }) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            return GameMode::MOVEMENT_TEST;
        }
        if (CheckCollisionPointRec(GetMousePosition(), { btnX, startY + gap * 4, btnW, 54.0f }) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            return GameMode::MAP_EDITOR;
        }
        if (CheckCollisionPointRec(GetMousePosition(), { btnX, startY + gap * 5, btnW, 54.0f }) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            screen = MenuScreen::SETTINGS;
            audio.PlayOrder();
            return GameMode::MAIN_MENU;
        }
        if (CheckCollisionPointRec(GetMousePosition(), { btnX, startY + gap * 6, btnW, 54.0f }) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            return GameMode::EXIT_GAME;
        }

        return GameMode::MAIN_MENU;
    }

    void DrawMainScreen(int width, int height, const SaveSystem& saveSystem) const {
        (void)height;
        int cx = width / 2;

        const char* title = "SQUAD TACTICAL SHOOTER";
        DrawText(title, cx - MeasureText(title, 36) / 2, 72, 36, RAYWHITE);

        const char* sub = TextFormat("FASE DESBLOQUEADA: %d/%d  |  MISSOES CONCLUIDAS: %d  |  ABATES: %d",
                                     saveSystem.data.unlockedPhase, SaveSystem::MAX_PHASES,
                                     saveSystem.data.missionsCompleted, saveSystem.data.totalKills);
        DrawText(sub, cx - MeasureText(sub, 15) / 2, 118, 15, SKYBLUE);

        const char* loadoutTag = TextFormat("LOADOUT ATUAL: %s + %s",
                                            saveSystem.GetPrimaryName(), saveSystem.GetSecondaryName());
        DrawText(loadoutTag, cx - MeasureText(loadoutTag, 14) / 2, 142, 14, GOLD);

        float btnW = 480.0f;
        float btnX = cx - btnW * 0.5f;
        float startY = 188.0f;
        float gap = 64.0f;

        DrawButton({ btnX, startY + gap * 0, btnW, 54.0f },
                   "MISSAO SOLO (BRIEFING & LOADOUT)",
                   "Escolher fase, armas primarias/secundarias e iniciar com bots",
                   GREEN, true);

        DrawButton({ btnX, startY + gap * 1, btnW, 54.0f },
                   "MULTIPLAYER LAN (HOST / CLIENTE UDP)",
                   "Co-op ate 4 jogadores na rede local (Alpha, Bravo, Charlie, Delta)",
                   SKYBLUE);

        DrawButton({ btnX, startY + gap * 2, btnW, 54.0f },
                   "ESTANDE DE SUPRESSAO & FLANCO",
                   "Treinar fogo de supressao, wallbang e dano furtivo nos flancos",
                   ORANGE);

        DrawButton({ btnX, startY + gap * 3, btnW, 54.0f },
                   "PISTA DE MOVIMENTACAO (SOURCE PHYSICS)",
                   "Testar bunnyhop, air-strafe, crouch-jump e velocidade em u/s",
                   LIME);

        DrawButton({ btnX, startY + gap * 4, btnW, 54.0f },
                   "EDITOR DE MAPAS IN-GAME",
                   "Construir coberturas destrutiveis e salvar mapas personalizados",
                   GOLD);

        DrawButton({ btnX, startY + gap * 5, btnW, 54.0f },
                   "CONFIGURACOES & SAVE (savegame.cfg)",
                   "Sensibilidade do mouse, FOV, volumes de audio e progresso salvo",
                   PURPLE);

        DrawButton({ btnX, startY + gap * 6, btnW, 54.0f },
                   "SAIR DO JOGO",
                   "Encerrar simulador tatico",
                   RED);
    }

    // =========================================================================
    // TELA 2: LOBBY MULTIPLAYER LAN (HOST / CLIENTE UDP)
    // =========================================================================
    GameMode UpdateLanLobbyScreen(int width, int height, SaveSystem& saveSystem, ProceduralAudio& audio, LanNetworkManager& net) {
        int leftX = 60;
        int colW = 550;
        int rightX = width - colW - 60;

        // Coluna Esquerda: Criar Sala (Host)
        Rectangle hostCreateBtn = { (float)leftX, 185.0f, (float)colW, 58.0f };
        if (CheckCollisionPointRec(GetMousePosition(), hostCreateBtn) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            if (net.StartHost(LanNetworkManager::DEFAULT_PORT)) {
                audio.PlayOrder();
            } else {
                audio.PlayDownedAlert();
            }
        }

        // Se for Host, permite iniciar a partida para todos os conectados
        if (net.IsHost()) {
            Rectangle launchHostBtn = { (float)leftX, 495.0f, (float)colW, 62.0f };
            if ((CheckCollisionPointRec(GetMousePosition(), launchHostBtn) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) || IsKeyPressed(KEY_ENTER)) {
                saveSystem.Save();
                net.BroadcastStartMatch((uint8_t)saveSystem.data.selectedPhase);
                audio.PlayOrder();
                return GameMode::SQUAD_MISSION;
            }
        }

        // Coluna Direita: Campo de IP e Conectar como Cliente
        Rectangle ipBox = { (float)rightX, 215.0f, (float)colW, 48.0f };
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            ipFieldFocused = CheckCollisionPointRec(GetMousePosition(), ipBox);
        }

        if (ipFieldFocused && !net.IsHost()) {
            int key = GetCharPressed();
            while (key > 0) {
                if (((key >= '0' && key <= '9') || key == '.') && net.hostIpInput.size() < 22) {
                    net.hostIpInput.push_back((char)key);
                }
                key = GetCharPressed();
            }
            if ((IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) && !net.hostIpInput.empty()) {
                net.hostIpInput.pop_back();
            }
        }

        // Atalhos rápidos de IP
        if (CheckCollisionPointRec(GetMousePosition(), { (float)rightX, 274.0f, 265.0f, 38.0f }) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            net.hostIpInput = "127.0.0.1";
            audio.PlayOrder();
        }
        if (CheckCollisionPointRec(GetMousePosition(), { (float)rightX + 285.0f, 274.0f, 265.0f, 38.0f }) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            net.hostIpInput = "192.168.1.";
            ipFieldFocused = true;
            audio.PlayOrder();
        }

        Rectangle connectBtn = { (float)rightX, 330.0f, (float)colW, 58.0f };
        if (CheckCollisionPointRec(GetMousePosition(), connectBtn) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            if (net.ConnectToHost(net.hostIpInput, LanNetworkManager::DEFAULT_PORT)) {
                audio.PlayOrder();
            } else {
                audio.PlayDownedAlert();
            }
        }

        // Se o Host iniciou a partida remotamente, o Cliente entra automaticamente na arena
        if (net.IsClient() && net.matchStarted) {
            saveSystem.Save();
            audio.PlayOrder();
            return GameMode::SQUAD_MISSION;
        }

        // Botão Voltar / Desconectar
        Rectangle backBtn = { (float)leftX, (float)height - 96.0f, 280.0f, 52.0f };
        if (CheckCollisionPointRec(GetMousePosition(), backBtn) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            net.Shutdown();
            ReturnToMain();
            audio.PlayOrder();
        }

        return GameMode::MAIN_MENU;
    }

    void DrawLanLobbyScreen(int width, int height, const SaveSystem& saveSystem, const LanNetworkManager& net) const {
        int leftX = 60;
        int colW = 550;
        int rightX = width - colW - 60;

        DrawText("MULTIPLAYER COOPERATIVO LAN (UDP HOST / CLIENTE)", leftX, 38, 28, RAYWHITE);
        DrawText(TextFormat("Status da Rede: %s", net.statusMessage.c_str()), leftX, 74, 16,
                 net.IsActive() ? GREEN : SKYBLUE);
        DrawLine(leftX, 102, width - 60, 102, Color{ 55, 68, 90, 255 });

        // =====================================================================
        // COLUNA ESQUERDA: CRIAR SALA (HOST AUTORITATIVO)
        // =====================================================================
        DrawText("1. CRIAR SALA NA REDE LOCAL (HOST):", leftX, 122, 18, GREEN);
        DrawText(TextFormat("Seu IP Local: %s  |  Porta UDP: %d  |  Fase: 0%d",
                            net.detectedLocalIp.c_str(), LanNetworkManager::DEFAULT_PORT, saveSystem.data.selectedPhase),
                 leftX, 150, 15, GOLD);

        DrawButton({ (float)leftX, 185.0f, (float)colW, 58.0f },
                   net.IsHost() ? "SERVIDOR ATIVO (AGUARDANDO ESQUADRAO)" : "CRIAR SALA (HOST)",
                   "Voce controla Alpha (Lider) e simula inimigos e objetivos",
                   GREEN, net.IsHost());

        // Roster dos 4 Slots do Esquadrão
        DrawRectangle(leftX, 260, colW, 218, Color{ 20, 25, 34, 235 });
        DrawRectangleLines(leftX, 260, colW, 218, Color{ 55, 70, 95, 255 });
        DrawText("FORMACAO DO ESQUADRAO (4 OPERADORES):", leftX + 16, 274, 16, SKYBLUE);

        Color slotColors[4] = { WHITE, SKYBLUE, LIME, GOLD };
        for (int s = 0; s < 4; s++) {
            int rowY = 308 + s * 40;
            bool human = net.IsSlotHuman(s);
            const char* roleTag = "";

            if (s == 0) {
                roleTag = net.IsHost() ? "[HOST - VOCE]" : (net.IsClient() ? "[HOST REMOTO]" : "[LIDER]");
            } else if (net.IsHost() && net.clientSlots[s].connected) {
                roleTag = TextFormat("[JOGADOR - %s]", net.clientSlots[s].ipString.c_str());
            } else if (net.IsClient() && net.localSlot == s) {
                roleTag = "[VOCE - CONECTADO]";
            } else if (human) {
                roleTag = "[JOGADOR CONECTADO]";
            } else {
                roleTag = "[BOT IA - Vaga Aberta]";
            }

            DrawText(TextFormat("Slot %d - %s", s + 1, LanNetworkManager::GetSlotCallsign(s)),
                     leftX + 18, rowY, 16, slotColors[s]);
            DrawText(roleTag, leftX + 275, rowY, 15, human ? GREEN : LIGHTGRAY);
        }

        if (net.IsHost()) {
            DrawButton({ (float)leftX, 495.0f, (float)colW, 62.0f },
                       "INICIAR MISSAO COOPERATIVA [ENTER]",
                       TextFormat("Iniciar com %d jogador(es) e %d bot(s) IA",
                                  net.GetConnectedPlayerCount(), 4 - net.GetConnectedPlayerCount()),
                       GREEN, true);
        }

        // =====================================================================
        // COLUNA DIREITA: ENTRAR EM SALA EXISTENTE (CLIENTE UDP)
        // =====================================================================
        DrawText("2. ENTRAR EM SALA EXISTENTE (CLIENTE):", rightX, 122, 18, SKYBLUE);
        DrawText("Digite o IPv4 do Host na rede local (ou 127.0.0.1 para testar no mesmo PC):",
                 rightX, 150, 14, LIGHTGRAY);

        DrawText("IP DO HOST:", rightX, 190, 15, WHITE);
        Rectangle ipBox = { (float)rightX, 215.0f, (float)colW, 48.0f };
        DrawRectangleRec(ipBox, Color{ 24, 30, 42, 255 });
        DrawRectangleLinesEx(ipBox, ipFieldFocused ? 2.0f : 1.2f, ipFieldFocused ? SKYBLUE : GRAY);
        DrawText(net.hostIpInput.c_str(), rightX + 16, 229, 20, WHITE);

        DrawButton({ (float)rightX, 274.0f, 265.0f, 38.0f }, "LOCALHOST (127.0.0.1)", "", LIGHTGRAY);
        DrawButton({ (float)rightX + 285.0f, 274.0f, 265.0f, 38.0f }, "REDE LAN (192.168.1.x)", "", LIGHTGRAY);

        DrawButton({ (float)rightX, 330.0f, (float)colW, 58.0f },
                   net.clientConnected ? "CONECTADO AO HOST!" : "CONECTAR AO HOST",
                   "Assume automaticamente Bravo, Charlie ou Delta",
                   SKYBLUE, net.clientConnected);

        // Instruções de Arquitetura LAN
        DrawRectangle(rightX, 408, colW, 149, Color{ 20, 26, 36, 230 });
        DrawRectangleLines(rightX, 408, colW, 149, Color{ 55, 70, 95, 255 });
        DrawText("COMO FUNCIONA O CO-OP LAN:", rightX + 16, 422, 16, GOLD);
        DrawText("- Drop-in instantaneo: clientes podem entrar antes ou durante a missao.", rightX + 16, 450, 13, LIGHTGRAY);
        DrawText("- Replicacao UDP em tempo real de tiros, granadas, ordens e reanimacao.", rightX + 16, 474, 13, LIGHTGRAY);
        DrawText("- Se um cliente desconectar, o bot IA reassume o operador na hora.", rightX + 16, 498, 13, LIGHTGRAY);
        DrawText("- Use o Loadout configurado no Briefing antes de entrar na sala.", rightX + 16, 522, 13, SKYBLUE);

        DrawButton({ (float)leftX, (float)height - 96.0f, 280.0f, 52.0f },
                   "DESCONECTAR E VOLTAR [ESC]", "Menu Principal", RED);
    }

    // =========================================================================
    // TELA 3: BRIEFING DA MISSÃO & SELEÇÃO DE LOADOUT
    // =========================================================================
    GameMode UpdateBriefingScreen(int width, int height, Vector2 mouse, bool click, SaveSystem& saveSystem, ProceduralAudio& audio, LanNetworkManager& net) {
        (void)mouse;
        (void)click;

        int leftX = 60;
        int colW = 550;
        int rightX = width - colW - 60;

        for (int p = 1; p <= SaveSystem::MAX_PHASES; p++) {
            Rectangle phaseRect = { (float)leftX, 155.0f + (p - 1) * 92.0f, (float)colW, 78.0f };
            if (CheckCollisionPointRec(GetMousePosition(), phaseRect) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                if (p <= saveSystem.data.unlockedPhase) {
                    saveSystem.data.selectedPhase = p;
                    saveSystem.Save();
                    audio.PlayOrder();
                } else {
                    audio.PlayDownedAlert();
                }
            }
        }

        float primW = (colW - 20.0f) / 3.0f;
        for (int i = 0; i < 3; i++) {
            Rectangle r = { rightX + i * (primW + 10.0f), 178.0f, primW, 68.0f };
            if (CheckCollisionPointRec(GetMousePosition(), r) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                saveSystem.data.primaryWeaponIndex = i;
                saveSystem.Save();
                audio.PlayReload();
            }
        }

        float secW = (colW - 10.0f) / 2.0f;
        for (int i = 0; i < 2; i++) {
            Rectangle r = { rightX + i * (secW + 10.0f), 296.0f, secW, 64.0f };
            if (CheckCollisionPointRec(GetMousePosition(), r) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                saveSystem.data.secondaryWeaponIndex = i;
                saveSystem.Save();
                audio.PlayReload();
            }
        }

        for (int i = 0; i < 2; i++) {
            Rectangle r = { rightX + i * (secW + 10.0f), 410.0f, secW, 64.0f };
            if (CheckCollisionPointRec(GetMousePosition(), r) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                saveSystem.data.gadgetIndex = i;
                saveSystem.Save();
                audio.PlayOrder();
            }
        }

        Rectangle backBtn = { (float)leftX, (float)height - 105.0f, 240.0f, 54.0f };
        Rectangle launchBtn = { (float)width - 420.0f, (float)height - 105.0f, 360.0f, 54.0f };

        if (CheckCollisionPointRec(GetMousePosition(), backBtn) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            ReturnToMain();
            audio.PlayOrder();
            return GameMode::MAIN_MENU;
        }

        if ((CheckCollisionPointRec(GetMousePosition(), launchBtn) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) || IsKeyPressed(KEY_ENTER)) {
            net.Shutdown();
            saveSystem.Save();
            audio.PlayOrder();
            return GameMode::SQUAD_MISSION;
        }

        return GameMode::MAIN_MENU;
    }

    void DrawBriefingScreen(int width, int height, const SaveSystem& saveSystem) const {
        int leftX = 60;
        int colW = 550;
        int rightX = width - colW - 60;

        DrawText("BRIEFING TATICO & PERSONALIZACAO DE LOADOUT", leftX, 38, 28, RAYWHITE);
        DrawText("Configure o equipamento do Lider Alpha e selecione a operacao desbloqueada.", leftX, 74, 16, LIGHTGRAY);
        DrawLine(leftX, 102, width - 60, 102, Color{ 55, 68, 90, 255 });

        DrawText("1. SELECIONE A FASE DA CAMPANHA:", leftX, 122, 18, SKYBLUE);

        const char* phaseTitles[3] = {
            "FASE 01: OPERACAO SETOR ALPHA",
            "FASE 02: CONTRA-ATAQUE TEMPESTADE",
            "FASE 03: PROTOCOLO OMEGA (ELITE)"
        };
        const char* phaseDescs[3] = {
            "Hackear 2 terminais e extrair. Resistencia inimiga padrao (4 hostis).",
            "Reforcos inimigos no patio central (6 hostis + flanqueadores).",
            "Guarnicao blindada de elite (7 hostis com HP reforcado)."
        };

        for (int p = 1; p <= SaveSystem::MAX_PHASES; p++) {
            bool unlocked = (p <= saveSystem.data.unlockedPhase);
            bool selected = (p == saveSystem.data.selectedPhase);
            Rectangle r = { (float)leftX, 155.0f + (p - 1) * 92.0f, (float)colW, 78.0f };

            if (unlocked) {
                DrawButton(r, phaseTitles[p - 1], phaseDescs[p - 1], selected ? GREEN : SKYBLUE, selected);
            } else {
                DrawRectangleRec(r, Color{ 18, 20, 26, 210 });
                DrawRectangleLinesEx(r, 1.2f, DARKGRAY);
                DrawText(TextFormat("%s [BLOQUEADA]", phaseTitles[p - 1]), (int)r.x + 18, (int)r.y + 14, 17, GRAY);
                DrawText(TextFormat("Venca a Fase 0%d para desbloquear esta operacao.", p - 1), (int)r.x + 18, (int)r.y + 42, 14, DARKGRAY);
            }
        }

        DrawRectangle(leftX, 442, colW, 132, Color{ 20, 26, 36, 230 });
        DrawRectangleLines(leftX, 442, colW, 132, Color{ 55, 70, 95, 255 });
        DrawText("INTELIGENCIA DO ESQUADRAO:", leftX + 16, 456, 16, GOLD);
        DrawText("- Aliados: Bravo (Fuzil), Charlie (Fuzil) e Delta (Suporte).", leftX + 16, 484, 14, LIGHTGRAY);
        DrawText("- Use [F3] Supressao para fixar inimigos e [Scroll] para Ping.", leftX + 16, 508, 14, LIGHTGRAY);
        DrawText("- Atirar com Aggro < 25% enquanto aliados lutam causa 2x Dano!", leftX + 16, 532, 14, SKYBLUE);

        DrawText("2. ARMA PRIMARIA [Tecla 1]:", rightX, 148, 18, ORANGE);
        const char* primNames[3] = { "FUZIL 5.56", "ESCOPETA", "LMG PESADA" };
        const char* primSubs[3]  = { "Versatil / Preciso", "Curta Dist. / 8 Proj.", "Alta Supressao / Pen." };
        float primW = (colW - 20.0f) / 3.0f;
        for (int i = 0; i < 3; i++) {
            bool sel = (saveSystem.data.primaryWeaponIndex == i);
            DrawButton({ rightX + i * (primW + 10.0f), 178.0f, primW, 68.0f }, primNames[i], primSubs[i], ORANGE, sel);
        }

        DrawText("3. ARMA SECUNDARIA [Tecla 2]:", rightX, 266, 18, SKYBLUE);
        const char* secNames[2] = { "PISTOLA TATICA 9mm", "REVOLVER .44 MAGNUM" };
        const char* secSubs[2]  = { "Saque rapido / 15 municoes", "Alto impacto / Penetra cobertura" };
        float secW = (colW - 10.0f) / 2.0f;
        for (int i = 0; i < 2; i++) {
            bool sel = (saveSystem.data.secondaryWeaponIndex == i);
            DrawButton({ rightX + i * (secW + 10.0f), 296.0f, secW, 64.0f }, secNames[i], secSubs[i], SKYBLUE, sel);
        }

        DrawText("4. GADGET TATICO PRIORITARIO:", rightX, 380, 18, LIME);
        const char* gadNames[2] = { "GRANADA FRAG [Tecla 4]", "FLASHBANG [Tecla 5]" };
        const char* gadSubs[2]  = { "Dano explosivo em area", "Cega e suprime 100% os alvos" };
        for (int i = 0; i < 2; i++) {
            bool sel = (saveSystem.data.gadgetIndex == i);
            DrawButton({ rightX + i * (secW + 10.0f), 410.0f, secW, 64.0f }, gadNames[i], gadSubs[i], LIME, sel);
        }

        DrawRectangle(rightX, 494, colW, 80, Color{ 18, 28, 24, 235 });
        DrawRectangleLines(rightX, 494, colW, 80, GREEN);
        DrawText("EQUIPAMENTO CONFIRMADO PARA INSERCAO:", rightX + 16, 506, 14, GREEN);
        DrawText(TextFormat("%s  |  %s", saveSystem.GetPrimaryName(), saveSystem.GetSecondaryName()),
                 rightX + 16, 530, 15, WHITE);
        DrawText(TextFormat("Gadget Destaque: %s", saveSystem.GetGadgetName()), rightX + 16, 552, 14, LIME);

        DrawButton({ (float)leftX, (float)height - 105.0f, 240.0f, 54.0f },
                   "VOLTAR [ESC]", "Menu Principal", LIGHTGRAY);

        DrawButton({ (float)width - 420.0f, (float)height - 105.0f, 360.0f, 54.0f },
                   "INICIAR OPERACAO [ENTER]",
                   TextFormat("Desdobrar na Fase 0%d", saveSystem.data.selectedPhase),
                   GREEN, true);
    }

    // =========================================================================
    // TELA 4: CONFIGURAÇÕES (SLIDERS) & SAVE PERSISTENTE
    // =========================================================================
    void UpdateSettingsScreen(int width, int height, Vector2 mouse, bool click, SaveSystem& saveSystem, ProceduralAudio& audio) {
        (void)mouse;
        (void)click;

        int leftX = 70;
        int sliderW = 540;

        if (IsKeyPressed(KEY_UP))   selectedSettingRow = (selectedSettingRow + 3) % 4;
        if (IsKeyPressed(KEY_DOWN)) selectedSettingRow = (selectedSettingRow + 1) % 4;

        float keyStep = 0.0f;
        if (IsKeyPressed(KEY_LEFT) || IsKeyPressedRepeat(KEY_LEFT))   keyStep = -0.02f;
        if (IsKeyPressed(KEY_RIGHT) || IsKeyPressedRepeat(KEY_RIGHT)) keyStep = 0.02f;

        if (keyStep != 0.0f) {
            if (selectedSettingRow == 0) {
                saveSystem.data.mouseSensitivity = std::clamp(saveSystem.data.mouseSensitivity + keyStep * 0.0055f, 0.0005f, 0.0060f);
            } else if (selectedSettingRow == 1) {
                saveSystem.data.baseFov = std::clamp(saveSystem.data.baseFov + keyStep * 40.0f, 60.0f, 100.0f);
            } else if (selectedSettingRow == 2) {
                saveSystem.data.sfxVolume = std::clamp(saveSystem.data.sfxVolume + keyStep, 0.0f, 1.0f);
                saveSystem.ApplyAudioSettings(audio);
                audio.PlayOrder();
            } else if (selectedSettingRow == 3) {
                saveSystem.data.musicVolume = std::clamp(saveSystem.data.musicVolume + keyStep, 0.0f, 1.0f);
                saveSystem.ApplyAudioSettings(audio);
            }
            saveSystem.Save();
        }

        for (int i = 0; i < 4; i++) {
            Rectangle hitRect = { (float)leftX, 175.0f + i * 96.0f, (float)sliderW, 44.0f };
            if (CheckCollisionPointRec(GetMousePosition(), hitRect) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                activeSlider = i;
                selectedSettingRow = i;
            }
        }

        if (activeSlider != -1 && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            float t = std::clamp((GetMousePosition().x - (float)leftX) / (float)sliderW, 0.0f, 1.0f);
            if (activeSlider == 0) {
                saveSystem.data.mouseSensitivity = 0.0005f + t * (0.0060f - 0.0005f);
            } else if (activeSlider == 1) {
                saveSystem.data.baseFov = roundf(60.0f + t * 40.0f);
            } else if (activeSlider == 2) {
                saveSystem.data.sfxVolume = t;
                saveSystem.ApplyAudioSettings(audio);
            } else if (activeSlider == 3) {
                saveSystem.data.musicVolume = t;
                saveSystem.ApplyAudioSettings(audio);
            }
        }

        if (activeSlider != -1 && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
            if (activeSlider == 2) {
                audio.PlayOrder();
            }
            activeSlider = -1;
            saveSystem.Save();
        }

        int rightX = width - 540;
        if (CheckCollisionPointRec(GetMousePosition(), { (float)rightX, 345.0f, 470.0f, 52.0f }) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            saveSystem.Save();
            audio.PlayOrder();
        }
        if (CheckCollisionPointRec(GetMousePosition(), { (float)rightX, 412.0f, 470.0f, 52.0f }) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            saveSystem.ResetSettingsToDefault(audio);
            audio.PlayOrder();
        }
        if (CheckCollisionPointRec(GetMousePosition(), { (float)rightX, 479.0f, 470.0f, 52.0f }) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            saveSystem.ResetAllProgress();
            audio.PlayDownedAlert();
        }

        if (CheckCollisionPointRec(GetMousePosition(), { (float)leftX, (float)height - 105.0f, 260.0f, 54.0f }) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            saveSystem.Save();
            ReturnToMain();
            audio.PlayOrder();
        }
    }

    void DrawSettingsScreen(int width, int height, const SaveSystem& saveSystem) const {
        int leftX = 70;
        int sliderW = 540;
        int rightX = width - 540;

        DrawText("CONFIGURACOES DE JOGO & SAVE PERSISTENTE", leftX, 38, 28, RAYWHITE);
        DrawText("Arraste os sliders ou use as setas do teclado. Todas as alteracoes sao salvas em savegame.cfg.", leftX, 74, 16, LIGHTGRAY);
        DrawLine(leftX, 102, width - 70, 102, Color{ 55, 68, 90, 255 });

        struct SliderSpec {
            const char* label;
            const char* valueText;
            float norm;
            Color color;
        };

        SliderSpec specs[4] = {
            {
                "SENSIBILIDADE DO MOUSE",
                TextFormat("%.4f  (%.2fx)", saveSystem.data.mouseSensitivity, saveSystem.data.mouseSensitivity / 0.0022f),
                (saveSystem.data.mouseSensitivity - 0.0005f) / (0.0060f - 0.0005f),
                SKYBLUE
            },
            {
                "CAMPO DE VISAO BASE (FOV)",
                TextFormat("%.0f graus", saveSystem.data.baseFov),
                (saveSystem.data.baseFov - 60.0f) / 40.0f,
                GOLD
            },
            {
                "VOLUME DE EFEITOS SONOROS (SFX)",
                TextFormat("%.0f%%", saveSystem.data.sfxVolume * 100.0f),
                saveSystem.data.sfxVolume,
                GREEN
            },
            {
                "VOLUME DA TRILHA DINAMICA (MUSICA)",
                TextFormat("%.0f%%", saveSystem.data.musicVolume * 100.0f),
                saveSystem.data.musicVolume,
                ORANGE
            }
        };

        for (int i = 0; i < 4; i++) {
            int baseY = 148 + i * 96;
            bool isFocused = (selectedSettingRow == i || activeSlider == i);

            DrawText(specs[i].label, leftX, baseY, 17, isFocused ? specs[i].color : RAYWHITE);
            int valW = MeasureText(specs[i].valueText, 17);
            DrawText(specs[i].valueText, leftX + sliderW - valW, baseY, 17, specs[i].color);

            Rectangle track = { (float)leftX, (float)(baseY + 30), (float)sliderW, 14.0f };
            DrawRectangleRec(track, Color{ 26, 32, 44, 255 });
            DrawRectangleLinesEx(track, 1.2f, isFocused ? specs[i].color : GRAY);

            float clampedNorm = std::clamp(specs[i].norm, 0.0f, 1.0f);
            DrawRectangle((int)track.x, (int)track.y, (int)(track.width * clampedNorm), (int)track.height, specs[i].color);

            float knobX = track.x + track.width * clampedNorm;
            DrawRectangle((int)knobX - 7, (int)track.y - 5, 14, 24, WHITE);
            DrawRectangleLines((int)knobX - 7, (int)track.y - 5, 14, 24, BLACK);
        }

        DrawRectangle(rightX, 148, 470, 178, Color{ 20, 26, 36, 235 });
        DrawRectangleLines(rightX, 148, 470, 178, SKYBLUE);
        DrawText("REGISTRO DO OPERADOR (savegame.cfg)", rightX + 18, 162, 17, SKYBLUE);

        DrawText(TextFormat("Fases Desbloqueadas: %d de %d", saveSystem.data.unlockedPhase, SaveSystem::MAX_PHASES),
                 rightX + 18, 196, 16, GREEN);
        DrawText(TextFormat("Missoes Concluidas: %d", saveSystem.data.missionsCompleted),
                 rightX + 18, 224, 16, RAYWHITE);
        DrawText(TextFormat("Total de Inimigos Eliminados: %d", saveSystem.data.totalKills),
                 rightX + 18, 252, 16, RAYWHITE);
        DrawText(TextFormat("Total de Headshots: %d", saveSystem.data.totalHeadshots),
                 rightX + 18, 280, 16, GOLD);

        DrawButton({ (float)rightX, 345.0f, 470.0f, 52.0f },
                   "SALVAR CONFIGURACOES AGORA", "Gravar manualmente em savegame.cfg", GREEN);

        DrawButton({ (float)rightX, 412.0f, 470.0f, 52.0f },
                   "RESTAURAR CONFIGURACOES PADRAO", "Sensibilidade 0.0022, FOV 75, Volumes padrao", GOLD);

        DrawButton({ (float)rightX, 479.0f, 470.0f, 52.0f },
                   "ZERAR PROGRESSO DA CAMPANHA", "Bloqueia fases 2-3 e zera estatisticas", RED);

        DrawButton({ (float)leftX, (float)height - 105.0f, 260.0f, 54.0f },
                   "VOLTAR [ESC]", "Salvar e voltar ao Menu", LIGHTGRAY);
    }
};