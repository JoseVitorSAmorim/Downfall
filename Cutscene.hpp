#pragma once

#include "raylib.h"
#include "raymath.h"
#include "World.hpp"
#include "Audio.hpp"
#include <vector>
#include <string>
#include <algorithm>
#include <cmath>

struct CutsceneKeyframe {
    Vector3 pos;
    Vector3 target;
    float fov = 72.0f;
    float duration = 2.5f;
    std::string speaker;
    std::string subtitle;
};

struct CutsceneSequence {
    int id = 0;
    std::string title;
    std::vector<CutsceneKeyframe> keyframes;
};

struct TriggerZone {
    int id = 0;
    std::string name;
    BoundingBox bounds;
    int cutsceneId = -1;
    bool oneShot = true;
    bool triggered = false;
    bool requiresExtractionActive = false;
    Color color = SKYBLUE;
};

class CutsceneManager {
public:
    std::vector<CutsceneSequence> sequences;
    std::vector<TriggerZone> triggers;

    bool showDebugTriggers = false;

    void BuildDefaultSequences(Vector3 playerStartEye, Vector3 playerStartTarget) {
        sequences.clear();

        // Sequência 0: Inserção Tática (Panorama da Arena e Terminais)
        CutsceneSequence introSeq;
        introSeq.id = 0;
        introSeq.title = "OPERACAO SETOR ALPHA";
        introSeq.keyframes = {
            {
                { -10.5f, 8.5f, 12.0f },
                { 0.0f, 1.5f, -5.0f },
                68.0f,
                2.8f,
                "OVERWATCH",
                "Esquadrao Alpha em posicao. Dois terminais criptografados detectados nos flancos."
            },
            {
                { 10.5f, 5.2f, -1.5f },
                { -13.0f, 1.2f, -4.8f },
                65.0f,
                2.8f,
                "BRAVO",
                "Hostis protegendo o patio central. Use fogo de supressao [F3] e flanqueie."
            },
            {
                playerStartEye,
                playerStartTarget,
                75.0f,
                1.8f,
                "COMANDO ALPHA",
                "Hackeie os terminais Alpha e Bravo e prepare-se para a extracao."
            }
        };
        sequences.push_back(introSeq);

        // Sequência 1: Reconhecimento do Pátio Central
        CutsceneSequence courtyardSeq;
        courtyardSeq.id = 1;
        courtyardSeq.title = "CONTATO NO PATIO CENTRAL";
        courtyardSeq.keyframes = {
            {
                { 0.0f, 4.4f, 3.0f },
                { 0.0f, 1.2f, -13.5f },
                64.0f,
                2.3f,
                "CHARLIE",
                "Atencao! Barris de combustivel proximos as barricadas inimigas."
            },
            {
                playerStartEye,
                playerStartTarget,
                75.0f,
                1.2f,
                "COMANDO ALPHA",
                "Atire nos barris vermelhos para romper a defesa inimiga!"
            }
        };
        sequences.push_back(courtyardSeq);

        // Sequência 2: Zona de Extração Liberada
        CutsceneSequence extractSeq;
        extractSeq.id = 2;
        extractSeq.title = "PERIMETRO DE EXTRACAO";
        extractSeq.keyframes = {
            {
                { -5.5f, 3.6f, 10.5f },
                { 0.0f, 1.0f, 16.5f },
                66.0f,
                2.2f,
                "OVERWATCH",
                "Sinalizador verde ativo! Reuna o esquadrao no circulo de extracao."
            }
        };
        sequences.push_back(extractSeq);
    }

    void SetupFromScene(const SceneData& scene, bool isCampaignMission, Vector3 playerStartEye, Vector3 playerStartTarget) {
        triggers.clear();
        playing = false;
        activeSequenceIdx = -1;
        currentKeyframeIdx = 0;
        segmentTimer = 0.0f;
        letterboxRatio = 0.0f;

        BuildDefaultSequences(playerStartEye, playerStartTarget);

        if (!isCampaignMission) return;

        int nextTrigId = 1;
        for (const auto& ent : scene.entities) {
            if (ent.type != SceneEntityType::TRIGGER) continue;

            TriggerZone tz{};
            tz.id = nextTrigId++;
            tz.name = ent.tag.empty() ? std::string(TextFormat("TRIGGER 0%d", tz.id)) : ent.tag;
            tz.bounds = ent.GetAABB();
            tz.cutsceneId = (ent.paramA >= 0) ? ent.paramA : 1;
            tz.oneShot = true;
            tz.triggered = false;
            tz.requiresExtractionActive = (ent.paramB != 0);
            tz.color = tz.requiresExtractionActive ? GREEN : GOLD;
            triggers.push_back(tz);
        }
    }

    void SetupForMode(bool isCampaignMission, Vector3 playerStartEye, Vector3 playerStartTarget) {
        SceneData defaultScene;
        if (isCampaignMission) {
            defaultScene.entities = LevelLoader::BuildDefaultEntitiesForPreset(MapPreset::SQUAD_ARENA);
        }
        SetupFromScene(defaultScene, isCampaignMission, playerStartEye, playerStartTarget);
    }

    bool Play(int sequenceId, Vector3 currentCamPos, Vector3 currentCamTarget, float currentFov, ProceduralAudio& audio) {
        int foundIdx = -1;
        for (size_t i = 0; i < sequences.size(); i++) {
            if (sequences[i].id == sequenceId) {
                foundIdx = (int)i;
                break;
            }
        }
        if (foundIdx == -1 || sequences[foundIdx].keyframes.empty()) return false;

        activeSequenceIdx = foundIdx;
        currentKeyframeIdx = 0;
        segmentTimer = 0.0f;
        playing = true;

        anchorStartPos = currentCamPos;
        anchorStartTarget = currentCamTarget;
        anchorStartFov = currentFov;

        CutsceneKeyframe& lastKf = sequences[activeSequenceIdx].keyframes.back();
        lastKf.pos = currentCamPos;
        lastKf.target = currentCamTarget;
        lastKf.fov = currentFov;

        audio.PlayOrder();
        return true;
    }

    void Skip() {
        if (!playing) return;
        playing = false;
        activeSequenceIdx = -1;
        currentKeyframeIdx = 0;
        segmentTimer = 0.0f;
    }

    bool IsPlaying() const {
        return playing;
    }

    void Update(float dt, Vector3 playerPos, Vector3 currentCamPos, Vector3 currentCamTarget,
                float currentFov, bool extractionActive, ProceduralAudio& audio) {
        if (playing) {
            letterboxRatio = std::min(1.0f, letterboxRatio + dt * 3.4f);
        } else {
            letterboxRatio = std::max(0.0f, letterboxRatio - dt * 3.4f);
        }

        if (!playing) {
            for (auto& tz : triggers) {
                if (tz.oneShot && tz.triggered) continue;
                if (tz.requiresExtractionActive && !extractionActive) continue;

                if (IsPointInBox(playerPos, tz.bounds)) {
                    tz.triggered = true;
                    Play(tz.cutsceneId, currentCamPos, currentCamTarget, currentFov, audio);
                    break;
                }
            }
            return;
        }

        if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER)) {
            Skip();
            return;
        }

        if (activeSequenceIdx < 0 || activeSequenceIdx >= (int)sequences.size()) {
            Skip();
            return;
        }

        const CutsceneSequence& seq = sequences[activeSequenceIdx];
        segmentTimer += dt;

        while (playing && currentKeyframeIdx < (int)seq.keyframes.size()) {
            float segDur = std::max(0.05f, seq.keyframes[currentKeyframeIdx].duration);
            if (segmentTimer >= segDur) {
                segmentTimer -= segDur;
                currentKeyframeIdx++;
                if (currentKeyframeIdx >= (int)seq.keyframes.size()) {
                    Skip();
                }
            } else {
                break;
            }
        }
    }

    void ApplyToCamera(Camera3D& camera) const {
        if (!playing || activeSequenceIdx < 0 || activeSequenceIdx >= (int)sequences.size()) return;
        const CutsceneSequence& seq = sequences[activeSequenceIdx];
        if (currentKeyframeIdx < 0 || currentKeyframeIdx >= (int)seq.keyframes.size()) return;

        Vector3 fromPos = (currentKeyframeIdx == 0) ? anchorStartPos : seq.keyframes[currentKeyframeIdx - 1].pos;
        Vector3 fromTarget = (currentKeyframeIdx == 0) ? anchorStartTarget : seq.keyframes[currentKeyframeIdx - 1].target;
        float fromFov = (currentKeyframeIdx == 0) ? anchorStartFov : seq.keyframes[currentKeyframeIdx - 1].fov;

        const CutsceneKeyframe& toKf = seq.keyframes[currentKeyframeIdx];
        float rawT = (toKf.duration > 0.001f) ? (segmentTimer / toKf.duration) : 1.0f;
        float t = SmootherStep(rawT);

        camera.position = Vector3Lerp(fromPos, toKf.pos, t);
        camera.target = Vector3Lerp(fromTarget, toKf.target, t);
        camera.fovy = fromFov + (toKf.fov - fromFov) * t;
    }

    void DrawTriggers3D() const {
        for (const auto& tz : triggers) {
            if (tz.oneShot && tz.triggered && !showDebugTriggers) continue;

            Vector3 center = {
                (tz.bounds.min.x + tz.bounds.max.x) * 0.5f,
                tz.bounds.min.y + 0.03f,
                (tz.bounds.min.z + tz.bounds.max.z) * 0.5f
            };
            Vector3 size = {
                tz.bounds.max.x - tz.bounds.min.x,
                tz.bounds.max.y - tz.bounds.min.y,
                tz.bounds.max.z - tz.bounds.min.z
            };

            DrawCubeWires({ center.x, 0.04f, center.z }, size.x, 0.04f, size.z, Fade(tz.color, 0.35f));

            if (showDebugTriggers) {
                Vector3 boxCenter = { center.x, tz.bounds.min.y + size.y * 0.5f, center.z };
                Color fill = tz.triggered ? Fade(GRAY, 0.12f) : Fade(tz.color, 0.16f);
                Color wire = tz.triggered ? GRAY : tz.color;
                DrawCubeV(boxCenter, size, fill);
                DrawCubeWiresV(boxCenter, size, wire);
            }
        }
    }

    void DrawHUD(int screenWidth, int screenHeight) const {
        if (letterboxRatio <= 0.001f && !playing) return;

        int barHeight = (int)(screenHeight * 0.13f * letterboxRatio);
        if (barHeight > 0) {
            DrawRectangle(0, 0, screenWidth, barHeight, Color{ 8, 10, 14, 245 });
            DrawRectangle(0, screenHeight - barHeight, screenWidth, barHeight, Color{ 8, 10, 14, 245 });
            DrawLine(0, barHeight, screenWidth, barHeight, Fade(SKYBLUE, 0.45f * letterboxRatio));
            DrawLine(0, screenHeight - barHeight, screenWidth, screenHeight - barHeight, Fade(SKYBLUE, 0.45f * letterboxRatio));
        }

        if (!playing || activeSequenceIdx < 0 || activeSequenceIdx >= (int)sequences.size()) return;
        const CutsceneSequence& seq = sequences[activeSequenceIdx];
        if (currentKeyframeIdx < 0 || currentKeyframeIdx >= (int)seq.keyframes.size()) return;

        const CutsceneKeyframe& kf = seq.keyframes[currentKeyframeIdx];

        DrawText(seq.title.c_str(), 28, std::max(10, barHeight / 2 - 9), 18, SKYBLUE);

        const char* skipHint = "[ESPACO] PULAR CENA";
        DrawText(skipHint, screenWidth - MeasureText(skipHint, 15) - 28, std::max(10, barHeight / 2 - 7), 15, LIGHTGRAY);

        int subAreaTop = screenHeight - barHeight + 12;
        if (!kf.speaker.empty()) {
            const char* speakerTag = TextFormat("[%s]", kf.speaker.c_str());
            DrawText(speakerTag, screenWidth / 2 - MeasureText(speakerTag, 16) / 2, subAreaTop, 16, GOLD);
        }

        if (!kf.subtitle.empty()) {
            DrawText(kf.subtitle.c_str(), screenWidth / 2 - MeasureText(kf.subtitle.c_str(), 19) / 2, subAreaTop + 24, 19, RAYWHITE);
        }

        float totalDur = 0.0f;
        float elapsedDur = 0.0f;
        for (size_t i = 0; i < seq.keyframes.size(); i++) {
            totalDur += seq.keyframes[i].duration;
            if ((int)i < currentKeyframeIdx) elapsedDur += seq.keyframes[i].duration;
            else if ((int)i == currentKeyframeIdx) elapsedDur += segmentTimer;
        }
        float progress = (totalDur > 0.001f) ? std::clamp(elapsedDur / totalDur, 0.0f, 1.0f) : 1.0f;
        DrawRectangle(0, screenHeight - 4, (int)(screenWidth * progress), 4, SKYBLUE);
    }

private:
    bool playing = false;
    int activeSequenceIdx = -1;
    int currentKeyframeIdx = 0;
    float segmentTimer = 0.0f;
    float letterboxRatio = 0.0f;

    Vector3 anchorStartPos = { 0.0f, 1.7f, 0.0f };
    Vector3 anchorStartTarget = { 0.0f, 1.7f, -1.0f };
    float anchorStartFov = 75.0f;

    static float SmootherStep(float t) {
        t = std::clamp(t, 0.0f, 1.0f);
        return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
    }

    static bool IsPointInBox(Vector3 p, const BoundingBox& b) {
        return (p.x >= b.min.x && p.x <= b.max.x &&
                p.y >= b.min.y && p.y <= b.max.y &&
                p.z >= b.min.z && p.z <= b.max.z);
    }
};