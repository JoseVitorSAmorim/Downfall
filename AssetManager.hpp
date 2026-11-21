#pragma once

#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include "SquadAI.hpp"
#include <array>
#include <vector>
#include <string>
#include <algorithm>
#include <cctype>
#include <cmath>

enum class CharAssetId {
    ALLY = 0,
    ENEMY,
    COUNT
};

enum class PropAssetId {
    BARREL = 0,
    CRATE,
    TERMINAL,
    COUNT
};

struct AnimatedModelAsset {
    Model model = { 0 };
    ModelAnimation* anims = nullptr;
    int animCount = 0;
    bool loaded = false;

    int idleAnimIdx = -1;
    int moveAnimIdx = -1;
    int crouchAnimIdx = -1;
    int downedAnimIdx = -1;
};

struct StaticModelAsset {
    Model model = { 0 };
    bool loaded = false;
};

class AssetManager {
public:
    static constexpr int WEAPON_SLOT_COUNT = 8;

    std::array<AnimatedModelAsset, (size_t)CharAssetId::COUNT> charAssets;
    std::array<StaticModelAsset, WEAPON_SLOT_COUNT> weaponAssets;
    std::array<StaticModelAsset, (size_t)PropAssetId::COUNT> propAssets;

    int loadedCount = 0;
    int totalSlots = (int)CharAssetId::COUNT + WEAPON_SLOT_COUNT + (int)PropAssetId::COUNT;

    void Init() {
        loadedCount = 0;

        LoadCharacterAsset(CharAssetId::ALLY,  "assets/models/ally.glb");
        LoadCharacterAsset(CharAssetId::ENEMY, "assets/models/enemy.glb");

        static const std::array<const char*, WEAPON_SLOT_COUNT> weaponPaths = {{
            "assets/models/weapon_rifle.glb",
            "assets/models/weapon_shotgun.glb",
            "assets/models/weapon_lmg.glb",
            "assets/models/weapon_pistol.glb",
            "assets/models/weapon_revolver.glb",
            "assets/models/weapon_knife.glb",
            "assets/models/weapon_frag.glb",
            "assets/models/weapon_flash.glb"
        }};

        for (int i = 0; i < WEAPON_SLOT_COUNT; i++) {
            LoadStaticAsset(weaponAssets[i], weaponPaths[i]);
        }

        LoadStaticAsset(propAssets[(size_t)PropAssetId::BARREL],   "assets/models/barrel.glb");
        LoadStaticAsset(propAssets[(size_t)PropAssetId::CRATE],    "assets/models/crate.glb");
        LoadStaticAsset(propAssets[(size_t)PropAssetId::TERMINAL], "assets/models/terminal.glb");
    }

    void Shutdown() {
        for (auto& ca : charAssets) {
            if (ca.anims != nullptr && ca.animCount > 0) {
                UnloadModelAnimations(ca.anims, ca.animCount);
                ca.anims = nullptr;
                ca.animCount = 0;
            }
            if (ca.loaded) {
                UnloadModel(ca.model);
                ca.loaded = false;
            }
        }

        for (auto& wa : weaponAssets) {
            if (wa.loaded) {
                UnloadModel(wa.model);
                wa.loaded = false;
            }
        }

        for (auto& pa : propAssets) {
            if (pa.loaded) {
                UnloadModel(pa.model);
                pa.loaded = false;
            }
        }

        loadedCount = 0;
    }

    void ReloadAll() {
        Shutdown();
        Init();
    }

    bool HasWeaponModel(int weaponIndex) const {
        if (weaponIndex < 0 || weaponIndex >= WEAPON_SLOT_COUNT) return false;
        return weaponAssets[weaponIndex].loaded;
    }

    void DrawWeaponModel(int weaponIndex, Color tint = WHITE) const {
        if (!HasWeaponModel(weaponIndex)) return;
        DrawModel(weaponAssets[weaponIndex].model, { 0.0f, 0.0f, 0.0f }, 1.0f, tint);
    }

    bool HasPropModel(PropAssetId id) const {
        return propAssets[(size_t)id].loaded;
    }

    void DrawPropModel(PropAssetId id, Vector3 pos, float yawDeg = 0.0f, float scale = 1.0f, Color tint = WHITE) const {
        if (!HasPropModel(id)) return;
        DrawModelEx(propAssets[(size_t)id].model, pos, { 0.0f, 1.0f, 0.0f }, yawDeg, { scale, scale, scale }, tint);
    }

    // Renderiza Aliado (.glb animado do Blender ou fallback procedural em cubos)
    void DrawAlly(const SquadAlly& ally, const std::vector<EnemyBot>& enemies,
                  SquadOrder currentOrder, float animTime) const {
        const AnimatedModelAsset& asset = charAssets[(size_t)CharAssetId::ALLY];

        if (asset.loaded) {
            float yawDeg = ComputeAllyFacingDeg(ally, enemies);

            if (ally.isDowned) {
                if (asset.downedAnimIdx != -1) {
                    ApplyAnimationFrame(asset, asset.downedAnimIdx, animTime, ally.id);
                    DrawModelEx(asset.model, ally.pos, { 0.0f, 1.0f, 0.0f }, yawDeg, { 1.0f, 1.0f, 1.0f }, Fade(LIGHTGRAY, 0.85f));
                } else {
                    DrawModelEx(asset.model, { ally.pos.x, ally.pos.y + 0.35f, ally.pos.z },
                                { 1.0f, 0.0f, 0.0f }, -90.0f, { 1.0f, 1.0f, 1.0f }, Fade(LIGHTGRAY, 0.85f));
                }
                DrawCircle3D({ ally.pos.x, 0.04f, ally.pos.z }, 0.85f, { 1.0f, 0.0f, 0.0f }, 90.0f, RED);
                return;
            }

            int chosenAnim = asset.idleAnimIdx;
            if (ally.isSuppressed && asset.crouchAnimIdx != -1) {
                chosenAnim = asset.crouchAnimIdx;
            } else if (Vector3Distance(ally.pos, ally.anchorPos) > 0.35f && asset.moveAnimIdx != -1) {
                chosenAnim = asset.moveAnimIdx;
            }

            if (chosenAnim != -1) {
                ApplyAnimationFrame(asset, chosenAnim, animTime, ally.id);
            }

            DrawModelEx(asset.model, ally.pos, { 0.0f, 1.0f, 0.0f }, yawDeg, { 1.0f, 1.0f, 1.0f }, WHITE);
            DrawCircle3D({ ally.pos.x, 0.03f, ally.pos.z }, 0.55f, { 1.0f, 0.0f, 0.0f }, 90.0f, Fade(ally.color, 0.75f));

            if (currentOrder == SquadOrder::HOLD || currentOrder == SquadOrder::SUPPRESS || ally.hasPingOrder) {
                Color ringColor = ally.hasPingOrder ? SKYBLUE : ((currentOrder == SquadOrder::SUPPRESS) ? ORANGE : SKYBLUE);
                DrawCircle3D({ ally.anchorPos.x, 0.03f, ally.anchorPos.z }, 0.65f, { 1.0f, 0.0f, 0.0f }, 90.0f, Fade(ringColor, 0.6f));
            }
            return;
        }

        // FALLBACK PROCEDURAL (CUBOS)
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

            if (currentOrder == SquadOrder::HOLD || currentOrder == SquadOrder::SUPPRESS || ally.hasPingOrder) {
                Color ringColor = ally.hasPingOrder ? SKYBLUE : ((currentOrder == SquadOrder::SUPPRESS) ? ORANGE : SKYBLUE);
                DrawCircle3D({ ally.anchorPos.x, 0.03f, ally.anchorPos.z }, 0.65f, { 1.0f, 0.0f, 0.0f }, 90.0f, Fade(ringColor, 0.6f));
            }
        }
    }

    // Renderiza Inimigo (.glb animado do Blender ou fallback procedural torso + cabeça)
    void DrawEnemy(const EnemyBot& enemy, const SourcePlayer& player,
                   const std::vector<SquadAlly>& allies, bool isPingedTarget, float animTime) const {
        if (!enemy.IsAlive()) return;

        const AnimatedModelAsset& asset = charAssets[(size_t)CharAssetId::ENEMY];

        if (asset.loaded) {
            float yawDeg = ComputeEnemyFacingDeg(enemy, player, allies);

            int chosenAnim = asset.idleAnimIdx;
            if (enemy.isSuppressed && asset.crouchAnimIdx != -1) {
                chosenAnim = asset.crouchAnimIdx;
            } else if (asset.moveAnimIdx != -1 && enemy.currentTargetId != -1) {
                chosenAnim = asset.moveAnimIdx;
            }

            if (chosenAnim != -1) {
                ApplyAnimationFrame(asset, chosenAnim, animTime, enemy.id + 100);
            }

            Color tint = enemy.isSuppressed ? Color{ 255, 225, 190, 255 } : WHITE;
            DrawModelEx(asset.model, enemy.pos, { 0.0f, 1.0f, 0.0f }, yawDeg, { 1.0f, 1.0f, 1.0f }, tint);

            if (isPingedTarget) {
                DrawCircle3D({ enemy.pos.x, 0.05f, enemy.pos.z }, 0.80f, { 1.0f, 0.0f, 0.0f }, 90.0f, YELLOW);
            }
            return;
        }

        // FALLBACK PROCEDURAL (TORSO + CABEÇA)
        if (enemy.isSuppressed) {
            DrawCube({ enemy.pos.x, enemy.pos.y + 0.44f, enemy.pos.z }, 1.04f, 0.88f, 1.04f, { 165, 55, 45, 255 });
            DrawCubeWires({ enemy.pos.x, enemy.pos.y + 0.44f, enemy.pos.z }, 1.04f, 0.88f, 1.04f, YELLOW);
            DrawCube({ enemy.pos.x, enemy.pos.y + 1.08f, enemy.pos.z }, 0.72f, 0.40f, 0.72f, { 195, 75, 65, 255 });
            DrawCubeWires({ enemy.pos.x, enemy.pos.y + 1.08f, enemy.pos.z }, 0.72f, 0.40f, 0.72f, ORANGE);
        } else {
            DrawCube({ enemy.pos.x, enemy.pos.y + 0.69f, enemy.pos.z }, 1.0f, 1.38f, 1.0f, { 200, 42, 52, 255 });
            DrawCubeWires({ enemy.pos.x, enemy.pos.y + 0.69f, enemy.pos.z }, 1.0f, 1.38f, 1.0f, isPingedTarget ? YELLOW : BLACK);

            DrawCube({ enemy.pos.x, enemy.pos.y + 1.65f, enemy.pos.z }, 0.72f, 0.54f, 0.72f, { 225, 65, 75, 255 });
            DrawCubeWires({ enemy.pos.x, enemy.pos.y + 1.65f, enemy.pos.z }, 0.72f, 0.54f, 0.72f, isPingedTarget ? YELLOW : MAROON);
            DrawCube({ enemy.pos.x, enemy.pos.y + 1.62f, enemy.pos.z + 0.34f }, 0.52f, 0.16f, 0.12f, MAROON);
        }
    }

private:
    static std::string ToLowerStr(const char* s) {
        std::string out = s ? s : "";
        for (char& c : out) c = (char)std::tolower((unsigned char)c);
        return out;
    }

    void LoadStaticAsset(StaticModelAsset& slot, const char* path) {
        slot.loaded = false;
        if (!FileExists(path)) return;

        Model m = LoadModel(path);
        if (m.meshCount > 0) {
            slot.model = m;
            slot.loaded = true;
            loadedCount++;
        } else {
            UnloadModel(m);
        }
    }

    void LoadCharacterAsset(CharAssetId id, const char* path) {
        AnimatedModelAsset& slot = charAssets[(size_t)id];
        slot = AnimatedModelAsset();
        if (!FileExists(path)) return;

        Model m = LoadModel(path);
        if (m.meshCount <= 0) {
            UnloadModel(m);
            return;
        }

        slot.model = m;
        slot.loaded = true;
        loadedCount++;

        int count = 0;
        ModelAnimation* anims = LoadModelAnimations(path, &count);
        if (anims != nullptr && count > 0) {
            slot.anims = anims;
            slot.animCount = count;

            for (int i = 0; i < count; i++) {
                std::string name = ToLowerStr(anims[i].name);
                if (name.find("idle") != std::string::npos || name.find("parado") != std::string::npos) {
                    slot.idleAnimIdx = i;
                } else if (name.find("run") != std::string::npos || name.find("walk") != std::string::npos || name.find("andar") != std::string::npos) {
                    slot.moveAnimIdx = i;
                } else if (name.find("crouch") != std::string::npos || name.find("duck") != std::string::npos || name.find("agach") != std::string::npos) {
                    slot.crouchAnimIdx = i;
                } else if (name.find("down") != std::string::npos || name.find("dead") != std::string::npos || name.find("caido") != std::string::npos) {
                    slot.downedAnimIdx = i;
                }
            }

            if (slot.idleAnimIdx == -1) slot.idleAnimIdx = 0;
            if (slot.moveAnimIdx == -1 && count > 1) slot.moveAnimIdx = 1;
            if (slot.crouchAnimIdx == -1 && count > 2) slot.crouchAnimIdx = 2;
            if (slot.downedAnimIdx == -1 && count > 3) slot.downedAnimIdx = 3;
        }
    }

    static void ApplyAnimationFrame(const AnimatedModelAsset& asset, int animIdx, float animTime, int seed) {
        if (animIdx < 0 || animIdx >= asset.animCount || asset.anims == nullptr) return;
        const ModelAnimation& anim = asset.anims[animIdx];
        if (anim.keyframeCount <= 0) return;

        int frame = ((int)(animTime * 30.0f) + seed * 17) % anim.keyframeCount;
        if (frame < 0) frame += anim.keyframeCount;
        UpdateModelAnimation(asset.model, anim, frame);
    }

    static float ComputeAllyFacingDeg(const SquadAlly& ally, const std::vector<EnemyBot>& enemies) {
        float bestDist = 99999.0f;
        Vector3 lookTarget = { ally.pos.x, ally.pos.y, ally.pos.z - 5.0f };
        for (const auto& e : enemies) {
            if (!e.IsAlive()) continue;
            float d = Vector3Distance(ally.pos, e.pos);
            if (d < bestDist) {
                bestDist = d;
                lookTarget = e.pos;
            }
        }
        Vector2 dir = { lookTarget.x - ally.pos.x, lookTarget.z - ally.pos.z };
        if (Vector2Length(dir) < 0.01f) return 180.0f;
        return atan2f(dir.x, dir.y) * RAD2DEG;
    }

    static float ComputeEnemyFacingDeg(const EnemyBot& enemy, const SourcePlayer& player, const std::vector<SquadAlly>& allies) {
        Vector3 lookTarget = { enemy.pos.x, enemy.pos.y, enemy.pos.z + 5.0f };
        if (enemy.currentTargetId == 0) {
            lookTarget = player.pos;
        } else if (enemy.currentTargetId > 0 && enemy.currentTargetId <= (int)allies.size()) {
            lookTarget = allies[enemy.currentTargetId - 1].pos;
        }
        Vector2 dir = { lookTarget.x - enemy.pos.x, lookTarget.z - enemy.pos.z };
        if (Vector2Length(dir) < 0.01f) return 0.0f;
        return atan2f(dir.x, dir.y) * RAD2DEG;
    }
};