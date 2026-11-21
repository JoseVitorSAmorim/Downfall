#pragma once

#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include "World.hpp"
#include "SquadAI.hpp"
#include <vector>
#include <string>
#include <array>
#include <algorithm>
#include <cmath>

enum class WeaponId {
    RIFLE = 0,
    SHOTGUN,
    LMG,
    PISTOL,
    REVOLVER,
    MELEE_KNIFE,
    GADGET_FRAG,
    GADGET_FLASH,
    COUNT
};

struct WeaponStats {
    WeaponId id;
    int slot;               // 1 a 5
    const char* name;
    const char* category;
    bool isAutomatic;
    bool isMelee;
    bool isGrenade;
    float fireInterval;
    float damage;
    int pelletCount;
    float spread;
    float range;
    float penetrationPower; // Espessura máxima (m) de parede perfurável
    float aggroOnFire;
    float aggroOnHit;
    float suppressionOnHit;
    float recoilZ;
    float recoilPitch;
    int magSize;
    float reloadTime;
    Color accentColor;
};

inline const WeaponStats& GetWeaponStats(WeaponId id) {
    static const std::array<WeaponStats, (size_t)WeaponId::COUNT> table = {{
        // Slot 1: Primárias
        { WeaponId::RIFLE,       1, "Rifle de Assalto",   "Primaria",   true,  false, false, 0.11f, 26.0f, 1, 0.012f, 85.0f, 1.05f, 10.0f, 14.0f, 18.0f, 0.09f,  6.5f, 30, 1.45f, GOLD },
        { WeaponId::SHOTGUN,     1, "Escopeta Tatica",    "Primaria",   false, false, false, 0.72f, 17.0f, 7, 0.068f, 32.0f, 0.00f, 22.0f,  5.0f, 12.0f, 0.22f, 16.0f,  8, 1.80f, ORANGE },
        { WeaponId::LMG,         1, "Metralhadora Leve",  "Primaria",   true,  false, false, 0.085f,21.0f, 1, 0.024f, 80.0f, 1.65f, 16.0f, 12.0f, 28.0f, 0.08f,  5.5f, 80, 2.35f, RED },

        // Slot 2: Secundárias
        { WeaponId::PISTOL,      2, "Pistola Tatica",     "Secundaria", false, false, false, 0.16f, 24.0f, 1, 0.008f, 65.0f, 0.00f,  4.0f,  8.0f, 12.0f, 0.07f,  5.0f, 15, 1.10f, SKYBLUE },
        { WeaponId::REVOLVER,    2, "Revolver .44",       "Secundaria", false, false, false, 0.46f, 58.0f, 1, 0.005f, 75.0f, 1.35f, 15.0f, 20.0f, 32.0f, 0.19f, 14.5f,  6, 1.65f, YELLOW },

        // Slot 3: Terciária (Arma Branca)
        { WeaponId::MELEE_KNIFE, 3, "Faca Tatica",        "Terciaria",  false, true,  false, 0.45f, 65.0f, 1, 0.0f,    2.7f, 0.00f,  0.0f,  0.0f, 15.0f, 0.0f,   0.0f, -1, 0.0f,  LIME },

        // Slots 4 e 5: Gadgets / Granadas
        { WeaponId::GADGET_FRAG, 4, "Granada Frag",       "Gadget 1",   false, false, true,  0.80f, 115.0f,1, 0.0f,   55.0f, 0.00f, 25.0f,  0.0f, 85.0f, 0.0f,   0.0f,  2, 6.00f, RED },
        { WeaponId::GADGET_FLASH,5, "Granada Flashbang",  "Gadget 2",   false, false, true,  0.80f,   5.0f,1, 0.0f,   55.0f, 0.00f,  8.0f,  0.0f,100.0f, 0.0f,   0.0f,  2, 6.00f, WHITE }
    }};
    return table[(size_t)id];
}

struct BallisticHitResult {
    Vector3 endPoint;
    int hitEnemyIdx = -1;
    bool isHeadshot = false;
    bool didWallbang = false;
    float damageMultiplier = 1.0f;
    bool hasPiercePoint = false;
    Vector3 piercePoint = { 0.0f, 0.0f, 0.0f };
};

// Interseção exata entrada/saída (slab method) para calcular espessura atravessada no bloco
inline bool IntersectRayBoxInterval(Ray ray, const BoundingBox& box, float& outEntry, float& outExit) {
    float tMin = 0.0f;
    float tMax = 100000.0f;

    const float orig[3] = { ray.position.x, ray.position.y, ray.position.z };
    const float dir[3]  = { ray.direction.x, ray.direction.y, ray.direction.z };
    const float bMin[3] = { box.min.x, box.min.y, box.min.z };
    const float bMax[3] = { box.max.x, box.max.y, box.max.z };

    for (int i = 0; i < 3; i++) {
        if (fabsf(dir[i]) < 1e-6f) {
            if (orig[i] < bMin[i] || orig[i] > bMax[i]) return false;
        } else {
            float invD = 1.0f / dir[i];
            float t1 = (bMin[i] - orig[i]) * invD;
            float t2 = (bMax[i] - orig[i]) * invD;
            if (t1 > t2) std::swap(t1, t2);
            tMin = std::max(tMin, t1);
            tMax = std::min(tMax, t2);
            if (tMin > tMax) return false;
        }
    }

    outEntry = tMin;
    outExit = tMax;
    return true;
}

// Traça o projétil com suporte a Headshot (2.5x) e Wallbang em coberturas finas
inline BallisticHitResult TraceBallisticShot(Ray ray, float maxRange, float penetrationPower,
                                             const std::vector<WorldBlock>& world,
                                             const std::vector<EnemyBot>& enemies) {
    struct WallSpan {
        float entry;
        float exit;
        float thickness;
    };

    std::vector<WallSpan> walls;
    for (const auto& b : world) {
        float tIn = 0.0f, tOut = 0.0f;
        if (IntersectRayBoxInterval(ray, b.bounds, tIn, tOut)) {
            if (tIn < maxRange && tOut > 0.001f) {
                walls.push_back({ tIn, tOut, std::max(0.0f, tOut - tIn) });
            }
        }
    }

    std::sort(walls.begin(), walls.end(), [](const WallSpan& a, const WallSpan& b) {
        return a.entry < b.entry;
    });

    // Descobre até onde o projétil consegue atravessar antes de parar em uma parede grossa
    float stopDist = maxRange;
    float remainingPen = penetrationPower;
    float wallDamageMult = 1.0f;

    struct PiercedSegment {
        float exitDist;
        float multAfter;
    };
    std::vector<PiercedSegment> piercedList;

    for (const auto& w : walls) {
        if (w.entry >= stopDist) break;

        if (remainingPen >= w.thickness && w.thickness > 0.001f) {
            remainingPen -= w.thickness;
            float reduction = (w.thickness / (penetrationPower + 0.25f)) * 0.52f;
            wallDamageMult *= std::max(0.38f, 1.0f - reduction);
            piercedList.push_back({ w.exit, wallDamageMult });
        } else {
            stopDist = w.entry;
            break;
        }
    }

    // Verifica colisão com Cabeça e Torso dos inimigos antes do stopDist
    BallisticHitResult res;
    res.endPoint = Vector3Add(ray.position, Vector3Scale(ray.direction, stopDist));

    float closestEnemyDist = stopDist;
    for (size_t i = 0; i < enemies.size(); i++) {
        if (!enemies[i].IsAlive()) continue;

        RayCollision headCol = GetRayCollisionBox(ray, enemies[i].GetHeadAABB());
        RayCollision torsoCol = GetRayCollisionBox(ray, enemies[i].GetTorsoAABB());

        if (headCol.hit && headCol.distance < closestEnemyDist) {
            closestEnemyDist = headCol.distance;
            res.hitEnemyIdx = (int)i;
            res.isHeadshot = true;
            res.endPoint = headCol.point;
        } else if (torsoCol.hit && torsoCol.distance < closestEnemyDist) {
            closestEnemyDist = torsoCol.distance;
            res.hitEnemyIdx = (int)i;
            res.isHeadshot = false;
            res.endPoint = torsoCol.point;
        }
    }

    // Verifica se o projétil atravesou alguma parede antes de atingir o alvo
    float appliedWallMult = 1.0f;
    for (const auto& seg : piercedList) {
        if (closestEnemyDist > seg.exitDist) {
            res.didWallbang = true;
            appliedWallMult = seg.multAfter;
            res.hasPiercePoint = true;
            res.piercePoint = Vector3Add(ray.position, Vector3Scale(ray.direction, seg.exitDist));
        }
    }

    float hitboxMult = res.isHeadshot ? 2.5f : 1.0f;
    res.damageMultiplier = hitboxMult * appliedWallMult;
    return res;
}

struct ThrownGrenade {
    Vector3 pos;
    Vector3 vel;
    bool isFlashbang;
    float fuseTimer;
    bool exploded = false;
};

struct ExplosionEffect {
    Vector3 pos;
    bool isFlashbang;
    float timer;
    float maxDuration;
    float maxRadius;
};

class ViewmodelWeapon {
public:
    int currentSlot = 1;
    int primaryVariant = 0;
    int secondaryVariant = 0;

    std::array<int, (size_t)WeaponId::COUNT> ammo = { 30, 8, 80, 15, 6, -1, 2, 2 };
    std::array<float, (size_t)WeaponId::COUNT> gadgetRecharge = { 0 };

    float fireCooldown = 0.0f;
    float reloadTimer = 0.0f;
    bool isReloading = false;

    Vector2 swayOffset = { 0.0f, 0.0f };
    float bobTimer = 0.0f;
    float recoilZ = 0.0f;
    float recoilPitch = 0.0f;
    float muzzleFlashTimer = 0.0f;
    float meleeSlashTimer = 0.0f;
    float throwAnimTimer = 0.0f;

    WeaponId GetCurrentWeaponId() const {
        if (currentSlot == 1) {
            if (primaryVariant == 0) return WeaponId::RIFLE;
            if (primaryVariant == 1) return WeaponId::SHOTGUN;
            return WeaponId::LMG;
        }
        if (currentSlot == 2) {
            return (secondaryVariant == 0) ? WeaponId::PISTOL : WeaponId::REVOLVER;
        }
        if (currentSlot == 3) return WeaponId::MELEE_KNIFE;
        if (currentSlot == 4) return WeaponId::GADGET_FRAG;
        return WeaponId::GADGET_FLASH;
    }

    const WeaponStats& GetCurrentStats() const {
        return GetWeaponStats(GetCurrentWeaponId());
    }

    int GetCurrentAmmo() const {
        return ammo[(size_t)GetCurrentWeaponId()];
    }

    void SelectSlot(int slot) {
        if (slot < 1 || slot > 5) return;

        if (slot == currentSlot) {
            if (slot == 1) {
                primaryVariant = (primaryVariant + 1) % 3;
                isReloading = false;
                reloadTimer = 0.0f;
            } else if (slot == 2) {
                secondaryVariant = (secondaryVariant + 1) % 2;
                isReloading = false;
                reloadTimer = 0.0f;
            }
            return;
        }

        currentSlot = slot;
        isReloading = false;
        reloadTimer = 0.0f;
        fireCooldown = 0.12f;
    }

    void CycleNextWeapon(int direction) {
        static const WeaponId order[] = {
            WeaponId::RIFLE, WeaponId::SHOTGUN, WeaponId::LMG,
            WeaponId::PISTOL, WeaponId::REVOLVER,
            WeaponId::MELEE_KNIFE,
            WeaponId::GADGET_FRAG, WeaponId::GADGET_FLASH
        };
        const int count = 8;
        WeaponId cur = GetCurrentWeaponId();
        int idx = 0;
        for (int i = 0; i < count; i++) {
            if (order[i] == cur) { idx = i; break; }
        }
        int nextIdx = (idx + direction + count) % count;
        WeaponId nextW = order[nextIdx];

        isReloading = false;
        reloadTimer = 0.0f;
        if (nextW == WeaponId::RIFLE)        { currentSlot = 1; primaryVariant = 0; }
        else if (nextW == WeaponId::SHOTGUN) { currentSlot = 1; primaryVariant = 1; }
        else if (nextW == WeaponId::LMG)     { currentSlot = 1; primaryVariant = 2; }
        else if (nextW == WeaponId::PISTOL)  { currentSlot = 2; secondaryVariant = 0; }
        else if (nextW == WeaponId::REVOLVER){ currentSlot = 2; secondaryVariant = 1; }
        else if (nextW == WeaponId::MELEE_KNIFE)  { currentSlot = 3; }
        else if (nextW == WeaponId::GADGET_FRAG)  { currentSlot = 4; }
        else if (nextW == WeaponId::GADGET_FLASH) { currentSlot = 5; }
    }

    bool StartReload() {
        const WeaponStats& st = GetCurrentStats();
        if (st.isMelee || st.isGrenade || isReloading) return false;
        int idx = (size_t)st.id;
        if (ammo[idx] >= st.magSize) return false;

        isReloading = true;
        reloadTimer = st.reloadTime;
        return true;
    }

    bool CanFire() const {
        if (isReloading || fireCooldown > 0.0f) return false;
        const WeaponStats& st = GetCurrentStats();
        if (st.isMelee) return true;
        return ammo[(size_t)st.id] > 0;
    }

    void ConsumeAndAnimateFire() {
        const WeaponStats& st = GetCurrentStats();
        fireCooldown = st.fireInterval;

        if (!st.isMelee) {
            int idx = (size_t)st.id;
            if (ammo[idx] > 0) ammo[idx]--;
        }

        if (st.isMelee) {
            meleeSlashTimer = 0.26f;
        } else if (st.isGrenade) {
            throwAnimTimer = 0.30f;
        } else {
            recoilZ = st.recoilZ;
            recoilPitch = st.recoilPitch;
            muzzleFlashTimer = 0.05f;
        }
    }

    void Update(float dt, Vector2 mouseDelta, float horizSpeed, bool isGrounded) {
        if (fireCooldown > 0.0f) fireCooldown -= dt;
        if (meleeSlashTimer > 0.0f) meleeSlashTimer -= dt;
        if (throwAnimTimer > 0.0f) throwAnimTimer -= dt;

        for (WeaponId gid : { WeaponId::GADGET_FRAG, WeaponId::GADGET_FLASH }) {
            size_t idx = (size_t)gid;
            const WeaponStats& gst = GetWeaponStats(gid);
            if (ammo[idx] < gst.magSize) {
                gadgetRecharge[idx] += dt;
                if (gadgetRecharge[idx] >= 7.0f) {
                    ammo[idx]++;
                    gadgetRecharge[idx] = 0.0f;
                }
            } else {
                gadgetRecharge[idx] = 0.0f;
            }
        }

        if (isReloading) {
            reloadTimer -= dt;
            if (reloadTimer <= 0.0f) {
                isReloading = false;
                const WeaponStats& st = GetCurrentStats();
                ammo[(size_t)st.id] = st.magSize;
            }
        }

        float targetSwayX = std::clamp(-mouseDelta.x * 0.0022f, -0.06f, 0.06f);
        float targetSwayY = std::clamp(mouseDelta.y * 0.0022f, -0.06f, 0.06f);
        swayOffset.x += (targetSwayX - swayOffset.x) * std::min(14.0f * dt, 1.0f);
        swayOffset.y += (targetSwayY - swayOffset.y) * std::min(14.0f * dt, 1.0f);

        if (isGrounded && horizSpeed > 0.5f) {
            bobTimer += dt * (7.0f + horizSpeed * 0.9f);
        } else {
            bobTimer = 0.0f;
        }

        recoilZ += (0.0f - recoilZ) * std::min(16.0f * dt, 1.0f);
        recoilPitch += (0.0f - recoilPitch) * std::min(14.0f * dt, 1.0f);
        if (muzzleFlashTimer > 0.0f) muzzleFlashTimer -= dt;
    }

    void Draw(const Camera3D& camera, bool isStealthFlank) const {
        WeaponId wid = GetCurrentWeaponId();
        const WeaponStats& st = GetCurrentStats();

        if (st.isGrenade && GetCurrentAmmo() <= 0) return;

        Matrix invView = MatrixInvert(GetCameraMatrix(camera));
        float bobX = cosf(bobTimer) * 0.012f;
        float bobY = fabsf(sinf(bobTimer)) * -0.014f;
        float reloadDrop = isReloading ? -0.16f : 0.0f;

        rlPushMatrix();
        rlMultMatrixf(MatrixToFloat(invView));

        Color statusLed = isStealthFlank ? SKYBLUE : st.accentColor;

        if (wid == WeaponId::MELEE_KNIFE) {
            float slashProgress = (meleeSlashTimer > 0.0f) ? sinf((meleeSlashTimer / 0.26f) * PI) : 0.0f;
            rlTranslatef(0.25f + swayOffset.x + bobX - slashProgress * 0.28f,
                         -0.22f + swayOffset.y + bobY + slashProgress * 0.08f,
                         -0.45f - slashProgress * 0.18f);
            rlRotatef(18.0f + slashProgress * 55.0f, 0.0f, 0.0f, 1.0f);
            rlRotatef(-slashProgress * 65.0f, 1.0f, 0.0f, 0.0f);

            DrawCube({ 0.0f, 0.0f, 0.06f }, 0.045f, 0.055f, 0.16f, { 35, 38, 44, 255 });
            DrawCube({ 0.0f, 0.0f, -0.03f }, 0.08f, 0.08f, 0.02f, DARKGRAY);
            DrawCube({ 0.0f, 0.005f, -0.16f }, 0.018f, 0.042f, 0.24f, LIGHTGRAY);
            DrawCubeWires({ 0.0f, 0.005f, -0.16f }, 0.018f, 0.042f, 0.24f, statusLed);
        } else if (st.isGrenade) {
            float pullBack = (throwAnimTimer > 0.0f) ? sinf((throwAnimTimer / 0.30f) * PI) * 0.22f : 0.0f;
            rlTranslatef(0.24f + swayOffset.x + bobX, -0.22f + swayOffset.y + bobY, -0.44f + pullBack);
            rlRotatef(15.0f, 0.0f, 0.0f, 1.0f);

            Color bodyCol = (wid == WeaponId::GADGET_FRAG) ? Color{ 55, 75, 55, 255 } : Color{ 195, 205, 215, 255 };
            Color bandCol = (wid == WeaponId::GADGET_FRAG) ? RED : SKYBLUE;

            DrawCube({ 0.0f, 0.0f, 0.0f }, 0.09f, 0.13f, 0.09f, bodyCol);
            DrawCubeWires({ 0.0f, 0.0f, 0.0f }, 0.09f, 0.13f, 0.09f, BLACK);
            DrawCube({ 0.0f, 0.02f, 0.0f }, 0.096f, 0.025f, 0.096f, bandCol);
            DrawCube({ 0.0f, 0.08f, 0.0f }, 0.04f, 0.03f, 0.04f, DARKGRAY);
        } else {
            rlTranslatef(0.27f + swayOffset.x + bobX, -0.23f + swayOffset.y + bobY + reloadDrop, -0.52f + recoilZ);
            rlRotatef(recoilPitch + (isReloading ? -25.0f : 0.0f), 1.0f, 0.0f, 0.0f);
            rlRotatef(swayOffset.x * 120.0f, 0.0f, 1.0f, 0.0f);

            if (wid == WeaponId::RIFLE) {
                DrawCube({ 0.0f, 0.0f, 0.0f }, 0.095f, 0.115f, 0.44f, { 42, 46, 54, 255 });
                DrawCubeWires({ 0.0f, 0.0f, 0.0f }, 0.095f, 0.115f, 0.44f, BLACK);
                DrawCube({ 0.0f, 0.015f, -0.32f }, 0.04f, 0.04f, 0.24f, { 28, 30, 35, 255 });
                DrawCube({ 0.0f, -0.10f, -0.04f }, 0.055f, 0.13f, 0.08f, { 35, 38, 44, 255 });
                DrawCube({ 0.0f, 0.07f, 0.06f }, 0.055f, 0.022f, 0.12f, statusLed);
            } else if (wid == WeaponId::SHOTGUN) {
                DrawCube({ 0.0f, 0.0f, 0.02f }, 0.11f, 0.125f, 0.46f, { 50, 48, 46, 255 });
                DrawCubeWires({ 0.0f, 0.0f, 0.02f }, 0.11f, 0.125f, 0.46f, BLACK);
                DrawCube({ 0.0f, 0.025f, -0.34f }, 0.055f, 0.055f, 0.28f, { 30, 32, 36, 255 });
                DrawCube({ 0.0f, -0.04f, -0.26f }, 0.075f, 0.06f, 0.18f, { 110, 75, 45, 255 });
                DrawCube({ 0.0f, 0.075f, 0.05f }, 0.06f, 0.022f, 0.10f, statusLed);
            } else if (wid == WeaponId::LMG) {
                DrawCube({ 0.0f, 0.01f, -0.02f }, 0.13f, 0.145f, 0.50f, { 48, 56, 48, 255 });
                DrawCubeWires({ 0.0f, 0.01f, -0.02f }, 0.13f, 0.145f, 0.50f, BLACK);
                DrawCube({ 0.0f, 0.02f, -0.38f }, 0.065f, 0.065f, 0.28f, { 30, 33, 35, 255 });
                DrawCube({ -0.06f, -0.10f, -0.06f }, 0.12f, 0.14f, 0.14f, { 75, 85, 65, 255 });
                DrawCube({ 0.0f, 0.095f, 0.04f }, 0.07f, 0.025f, 0.14f, statusLed);
            } else if (wid == WeaponId::PISTOL) {
                DrawCube({ 0.0f, 0.02f, -0.04f }, 0.065f, 0.085f, 0.26f, { 45, 50, 58, 255 });
                DrawCubeWires({ 0.0f, 0.02f, -0.04f }, 0.065f, 0.085f, 0.26f, BLACK);
                DrawCube({ 0.0f, -0.05f, 0.04f }, 0.055f, 0.11f, 0.07f, { 30, 32, 36, 255 });
                DrawCube({ 0.0f, 0.068f, 0.03f }, 0.04f, 0.016f, 0.08f, statusLed);
            } else if (wid == WeaponId::REVOLVER) {
                DrawCube({ 0.0f, 0.03f, -0.18f }, 0.055f, 0.065f, 0.30f, LIGHTGRAY);
                DrawCubeWires({ 0.0f, 0.03f, -0.18f }, 0.055f, 0.065f, 0.30f, BLACK);
                DrawCube({ 0.0f, 0.015f, -0.01f }, 0.095f, 0.095f, 0.10f, GRAY);
                DrawCube({ 0.0f, -0.06f, 0.07f }, 0.06f, 0.12f, 0.075f, { 95, 60, 38, 255 });
                DrawCube({ 0.0f, 0.07f, 0.04f }, 0.045f, 0.018f, 0.06f, statusLed);
            }

            if (muzzleFlashTimer > 0.0f) {
                float muzzleZ = (wid == WeaponId::PISTOL) ? -0.21f : ((wid == WeaponId::LMG) ? -0.55f : -0.47f);
                float flashRadius = (wid == WeaponId::SHOTGUN || wid == WeaponId::REVOLVER) ? 0.095f : 0.065f;
                DrawSphere({ 0.0f, 0.02f, muzzleZ }, flashRadius, GOLD);
            }
        }

        rlPopMatrix();
    }
};

inline void UpdateGrenades(float dt, std::vector<ThrownGrenade>& grenades,
                           std::vector<ExplosionEffect>& explosions,
                           const std::vector<WorldBlock>& world) {
    for (auto& g : grenades) {
        if (g.exploded) continue;

        g.fuseTimer -= dt;
        g.vel.y -= 18.0f * dt;
        float r = 0.16f;

        g.pos.x += g.vel.x * dt;
        BoundingBox box = { { g.pos.x - r, g.pos.y - r, g.pos.z - r }, { g.pos.x + r, g.pos.y + r, g.pos.z + r } };
        for (const auto& b : world) {
            if (CheckOverlapStrict(box, b.bounds)) {
                g.pos.x = (g.vel.x > 0.0f) ? (b.bounds.min.x - r) : (b.bounds.max.x + r);
                g.vel.x = -g.vel.x * 0.55f;
                break;
            }
        }

        g.pos.z += g.vel.z * dt;
        box = { { g.pos.x - r, g.pos.y - r, g.pos.z - r }, { g.pos.x + r, g.pos.y + r, g.pos.z + r } };
        for (const auto& b : world) {
            if (CheckOverlapStrict(box, b.bounds)) {
                g.pos.z = (g.vel.z > 0.0f) ? (b.bounds.min.z - r) : (b.bounds.max.z + r);
                g.vel.z = -g.vel.z * 0.55f;
                break;
            }
        }

        g.pos.y += g.vel.y * dt;
        box = { { g.pos.x - r, g.pos.y - r, g.pos.z - r }, { g.pos.x + r, g.pos.y + r, g.pos.z + r } };
        for (const auto& b : world) {
            if (CheckOverlapStrict(box, b.bounds)) {
                if (g.vel.y <= 0.0f) {
                    g.pos.y = b.bounds.max.y + r;
                    g.vel.y = (g.vel.y < -1.5f) ? -g.vel.y * 0.40f : 0.0f;
                    g.vel.x *= 0.80f;
                    g.vel.z *= 0.80f;
                } else {
                    g.pos.y = b.bounds.min.y - r;
                    g.vel.y = 0.0f;
                }
                break;
            }
        }

        if (g.fuseTimer <= 0.0f) {
            g.exploded = true;
            float radius = g.isFlashbang ? 7.5f : 5.8f;
            explosions.push_back({ g.pos, g.isFlashbang, 0.45f, 0.45f, radius });
        }
    }
}