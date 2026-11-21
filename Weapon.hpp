#pragma once

#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include "World.hpp"
#include "SquadAI.hpp"
#include <vector>
#include <array>
#include <cmath>
#include <algorithm>

class AssetManager;

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
    const char* name;
    float damage;
    float range;
    float penetrationPower;
    float fireInterval;
    float reloadDuration;
    float spread;
    int pelletCount;
    int magSize;
    bool isAutomatic;
    bool isMelee;
    bool isGrenade;
    float aggroOnFire;
    float aggroOnHit;
    float suppressionOnHit;
    Color accentColor;
};

inline const WeaponStats& GetWeaponStats(WeaponId id) {
    static const std::array<WeaponStats, static_cast<size_t>(WeaponId::COUNT)> table = {{
        { WeaponId::RIFLE,        "FUZIL TATICO 5.56",   35.0f, 95.0f, 1.05f, 0.110f, 1.45f, 0.012f, 1, 30, true,  false, false, 12.0f, 18.0f, 14.0f, GREEN },
        { WeaponId::SHOTGUN,      "ESCOPETA TATICA 12",  16.0f, 28.0f, 0.35f, 0.720f, 1.75f, 0.075f, 8,  8, false, false, false, 20.0f, 24.0f, 26.0f, ORANGE },
        { WeaponId::LMG,          "LMG PESADA M249",     28.0f, 90.0f, 1.65f, 0.085f, 2.30f, 0.024f, 1, 80, true,  false, false, 18.0f, 22.0f, 28.0f, GOLD },
        { WeaponId::PISTOL,       "PISTOLA 9MM",         26.0f, 65.0f, 0.55f, 0.160f, 1.05f, 0.015f, 1, 15, false, false, false,  8.0f, 12.0f, 10.0f, SKYBLUE },
        { WeaponId::REVOLVER,     "REVOLVER .44 MAGNUM", 62.0f, 85.0f, 1.35f, 0.420f, 1.50f, 0.010f, 1,  6, false, false, false, 16.0f, 24.0f, 20.0f, GOLD },
        { WeaponId::MELEE_KNIFE,  "FACA TATICA",         85.0f,  2.6f, 0.00f, 0.450f, 0.00f, 0.000f, 1,  1, false, true,  false,  2.0f,  4.0f,  5.0f, LIME },
        { WeaponId::GADGET_FRAG,  "GRANADA FRAG M67",   115.0f, 30.0f, 0.00f, 0.800f, 0.00f, 0.000f, 1,  2, false, false, true,  25.0f, 30.0f, 40.0f, RED },
        { WeaponId::GADGET_FLASH, "GRANADA FLASHBANG",    5.0f, 30.0f, 0.00f, 0.800f, 0.00f, 0.000f, 1,  2, false, false, true,  15.0f, 20.0f, 50.0f, SKYBLUE }
    }};
    size_t idx = std::min(static_cast<size_t>(id), table.size() - 1);
    return table[idx];
}

struct ThrownGrenade {
    Vector3 pos;
    Vector3 vel;
    bool isFlashbang;
    float timer;
    bool exploded = false;
};

struct ExplosionEffect {
    Vector3 pos;
    bool isFlashbang;
    float timer;
    float maxDuration;
    float maxRadius;
};

struct BallisticHitResult {
    Vector3 endPoint = { 0.0f, 0.0f, 0.0f };
    int hitEnemyIdx = -1;
    float damageMultiplier = 1.0f;
    bool isHeadshot = false;
    bool didWallbang = false;
    bool hasPiercePoint = false;
    Vector3 piercePoint = { 0.0f, 0.0f, 0.0f };
};

inline bool IntersectRayBoxInterval(Ray ray, BoundingBox box, float& tEnter, float& tExit) {
    float tMin = 0.0f;
    float tMax = 1e30f;

    const float orig[3] = { ray.position.x, ray.position.y, ray.position.z };
    const float dir[3]  = { ray.direction.x, ray.direction.y, ray.direction.z };
    const float bMin[3] = { box.min.x, box.min.y, box.min.z };
    const float bMax[3] = { box.max.x, box.max.y, box.max.z };

    for (int axis = 0; axis < 3; axis++) {
        if (std::fabs(dir[axis]) < 1e-7f) {
            if (orig[axis] < bMin[axis] || orig[axis] > bMax[axis]) return false;
        } else {
            float invD = 1.0f / dir[axis];
            float t1 = (bMin[axis] - orig[axis]) * invD;
            float t2 = (bMax[axis] - orig[axis]) * invD;
            if (t1 > t2) std::swap(t1, t2);
            tMin = std::max(tMin, t1);
            tMax = std::min(tMax, t2);
            if (tMin > tMax) return false;
        }
    }

    tEnter = tMin;
    tExit = tMax;
    return tMax >= 0.0f;
}

inline BallisticHitResult TraceBallisticShot(Ray ray, float maxRange, float penetrationPower,
                                             const std::vector<WorldBlock>& world,
                                             const std::vector<EnemyBot>& enemies) {
    const SpatialHashGrid& grid = GetWorldSpatialGrid(world);

    struct WallSegment {
        float tEnter;
        float tExit;
    };

    std::vector<WallSegment> walls;
    walls.reserve(8);

    grid.TraverseRayXZ(ray.position, ray.direction, maxRange, [&](int idx) -> bool {
        float tIn = 0.0f, tOut = 0.0f;
        if (IntersectRayBoxInterval(ray, world[idx].GetAABB(), tIn, tOut)) {
            if (tIn <= maxRange && tOut > 0.001f) {
                walls.push_back({ std::max(0.0f, tIn), std::min(maxRange, tOut) });
            }
        }
        return true;
    });

    std::sort(walls.begin(), walls.end(), [](const WallSegment& a, const WallSegment& b) {
        return a.tEnter < b.tEnter;
    });

    float stopDist = maxRange;
    float remPen = penetrationPower;
    float wallAttenuation = 1.0f;
    bool didWallbang = false;
    bool hasPierce = false;
    Vector3 piercePt = { 0.0f, 0.0f, 0.0f };

    for (const WallSegment& w : walls) {
        if (w.tEnter >= stopDist) break;

        float thickness = std::max(0.02f, w.tExit - w.tEnter);
        if (remPen < thickness) {
            stopDist = w.tEnter;
            break;
        }

        remPen -= thickness;
        didWallbang = true;
        hasPierce = true;
        piercePt = Vector3Add(ray.position, Vector3Scale(ray.direction, w.tExit));
        wallAttenuation *= std::clamp(1.0f - (thickness / (penetrationPower * 1.45f)), 0.35f, 0.92f);
    }

    BallisticHitResult result;
    result.endPoint = Vector3Add(ray.position, Vector3Scale(ray.direction, stopDist));
    result.didWallbang = didWallbang;
    result.hasPiercePoint = hasPierce;
    result.piercePoint = piercePt;

    float bestEnemyDist = stopDist;
    for (size_t i = 0; i < enemies.size(); i++) {
        if (!enemies[i].IsAlive()) continue;

        RayCollision headCol = GetRayCollisionBox(ray, enemies[i].GetHeadAABB());
        if (headCol.hit && headCol.distance >= 0.0f && headCol.distance <= bestEnemyDist) {
            bestEnemyDist = headCol.distance;
            result.hitEnemyIdx = static_cast<int>(i);
            result.isHeadshot = true;
            result.damageMultiplier = 2.5f * wallAttenuation;
            result.endPoint = headCol.point;
            continue;
        }

        RayCollision torsoCol = GetRayCollisionBox(ray, enemies[i].GetTorsoAABB());
        if (torsoCol.hit && torsoCol.distance >= 0.0f && torsoCol.distance <= bestEnemyDist) {
            bestEnemyDist = torsoCol.distance;
            result.hitEnemyIdx = static_cast<int>(i);
            result.isHeadshot = false;
            result.damageMultiplier = 1.0f * wallAttenuation;
            result.endPoint = torsoCol.point;
        }
    }

    if (result.hitEnemyIdx != -1 && !walls.empty() && bestEnemyDist < walls[0].tEnter) {
        result.didWallbang = false;
        result.hasPiercePoint = false;
        result.damageMultiplier = result.isHeadshot ? 2.5f : 1.0f;
    }

    return result;
}

inline void UpdateGrenades(float dt, std::vector<ThrownGrenade>& grenades,
                           std::vector<ExplosionEffect>& explosions,
                           const std::vector<WorldBlock>& world) {
    const SpatialHashGrid& grid = GetWorldSpatialGrid(world);
    const Vector3 nadeSize = { 0.30f, 0.30f, 0.30f };

    for (auto& g : grenades) {
        if (g.exploded) continue;

        g.timer -= dt;
        g.vel.y -= 18.5f * dt;

        Vector3 nextX = { g.pos.x + g.vel.x * dt, g.pos.y, g.pos.z };
        if (!OverlapsAnyBlock(nextX, nadeSize, world, grid)) {
            g.pos.x = nextX.x;
        } else {
            g.vel.x = -g.vel.x * 0.52f;
        }

        Vector3 nextZ = { g.pos.x, g.pos.y, g.pos.z + g.vel.z * dt };
        if (!OverlapsAnyBlock(nextZ, nadeSize, world, grid)) {
            g.pos.z = nextZ.z;
        } else {
            g.vel.z = -g.vel.z * 0.52f;
        }

        Vector3 nextY = { g.pos.x, g.pos.y + g.vel.y * dt, g.pos.z };
        if (!OverlapsAnyBlock(nextY, nadeSize, world, grid)) {
            g.pos.y = nextY.y;
        } else {
            g.vel.y = -g.vel.y * 0.42f;
            g.vel.x *= 0.72f;
            g.vel.z *= 0.72f;
        }

        if (g.timer <= 0.0f) {
            g.exploded = true;
            explosions.push_back({
                g.pos,
                g.isFlashbang,
                0.45f,
                0.45f,
                g.isFlashbang ? 8.5f : 6.0f
            });
        }
    }
}

class ViewmodelWeapon {
public:
    int currentSlot = 1;
    int primarySubIndex = 0;
    int secondarySubIndex = 0;

    std::array<int, static_cast<size_t>(WeaponId::COUNT)> ammo = { 30, 8, 80, 15, 6, 1, 2, 2 };
    float fireCooldown = 0.0f;
    bool isReloading = false;
    float reloadTimer = 0.0f;

    float recoilZ = 0.0f;
    float recoilPitch = 0.0f;
    float swayX = 0.0f;
    float swayY = 0.0f;
    float bobTimer = 0.0f;
    float muzzleFlashTimer = 0.0f;

    static const WeaponStats& GetStatsById(WeaponId id) {
        return GetWeaponStats(id);
    }

    WeaponId GetCurrentWeaponId() const {
        if (currentSlot == 1) {
            if (primarySubIndex == 1) return WeaponId::SHOTGUN;
            if (primarySubIndex == 2) return WeaponId::LMG;
            return WeaponId::RIFLE;
        }
        if (currentSlot == 2) {
            return (secondarySubIndex == 1) ? WeaponId::REVOLVER : WeaponId::PISTOL;
        }
        if (currentSlot == 3) return WeaponId::MELEE_KNIFE;
        if (currentSlot == 4) return WeaponId::GADGET_FRAG;
        return WeaponId::GADGET_FLASH;
    }

    const WeaponStats& GetCurrentStats() const {
        return GetWeaponStats(GetCurrentWeaponId());
    }

    int GetCurrentAmmo() const {
        return ammo[static_cast<size_t>(GetCurrentWeaponId())];
    }

    void SelectSlot(int slot) {
        if (slot < 1 || slot > 5) return;
        if (currentSlot == slot) {
            if (slot == 1) primarySubIndex = (primarySubIndex + 1) % 3;
            else if (slot == 2) secondarySubIndex = (secondarySubIndex + 1) % 2;
        } else {
            currentSlot = slot;
        }
        isReloading = false;
        reloadTimer = 0.0f;
    }

    void CycleNextWeapon(int dir) {
        int next = currentSlot + (dir >= 0 ? 1 : -1);
        if (next > 5) next = 1;
        if (next < 1) next = 5;
        currentSlot = next;
        isReloading = false;
        reloadTimer = 0.0f;
    }

    bool StartReload() {
        const WeaponStats& st = GetCurrentStats();
        if (isReloading || st.isMelee || st.isGrenade) return false;
        size_t idx = static_cast<size_t>(st.id);
        if (ammo[idx] >= st.magSize) return false;
        isReloading = true;
        reloadTimer = st.reloadDuration;
        return true;
    }

    bool CanFire() const {
        if (fireCooldown > 0.0f || isReloading) return false;
        const WeaponStats& st = GetCurrentStats();
        if (st.isMelee) return true;
        return ammo[static_cast<size_t>(st.id)] > 0;
    }

    void ConsumeAndAnimateFire() {
        const WeaponStats& st = GetCurrentStats();
        fireCooldown = st.fireInterval;
        if (!st.isMelee) {
            size_t idx = static_cast<size_t>(st.id);
            if (ammo[idx] > 0) ammo[idx]--;
        }
        recoilZ = st.isMelee ? -0.14f : 0.065f;
        recoilPitch = st.isMelee ? -0.18f : 0.08f;
        if (!st.isMelee && !st.isGrenade) {
            muzzleFlashTimer = 0.045f;
        }
    }

    void RefillAllAmmo() {
        for (size_t i = 0; i < static_cast<size_t>(WeaponId::COUNT); i++) {
            ammo[i] = GetWeaponStats(static_cast<WeaponId>(i)).magSize;
        }
        isReloading = false;
        reloadTimer = 0.0f;
    }

    void Update(float dt, Vector2 mouseDelta, float horizSpeed, bool isGrounded) {
        if (fireCooldown > 0.0f) fireCooldown = std::max(0.0f, fireCooldown - dt);
        if (muzzleFlashTimer > 0.0f) muzzleFlashTimer = std::max(0.0f, muzzleFlashTimer - dt);

        if (isReloading) {
            reloadTimer -= dt;
            if (reloadTimer <= 0.0f) {
                isReloading = false;
                const WeaponStats& st = GetCurrentStats();
                ammo[static_cast<size_t>(st.id)] = st.magSize;
            }
        }

        recoilZ += (0.0f - recoilZ) * std::min(16.0f * dt, 1.0f);
        recoilPitch += (0.0f - recoilPitch) * std::min(14.0f * dt, 1.0f);

        float targetSwayX = std::clamp(-mouseDelta.x * 0.0018f, -0.045f, 0.045f);
        float targetSwayY = std::clamp(mouseDelta.y * 0.0018f, -0.045f, 0.045f);
        swayX += (targetSwayX - swayX) * std::min(12.0f * dt, 1.0f);
        swayY += (targetSwayY - swayY) * std::min(12.0f * dt, 1.0f);

        if (isGrounded && horizSpeed > 0.5f) {
            bobTimer += dt * (6.0f + horizSpeed * 0.7f);
        } else {
            bobTimer += dt * 1.5f;
        }
    }

    template <typename AssetMgr>
    void Draw(const Camera3D& camera, bool isStealthFlank, const AssetMgr& assets) const {
        (void)assets;
        const WeaponStats& st = GetCurrentStats();
        Matrix camToWorld = MatrixInvert(GetCameraMatrix(camera));

        float bobX = std::cos(bobTimer * 0.5f) * 0.012f;
        float bobY = std::fabs(std::sin(bobTimer)) * 0.014f;
        float reloadDrop = isReloading ? -0.14f : 0.0f;

        rlPushMatrix();
        rlMultMatrixf(MatrixToFloat(camToWorld));
        rlTranslatef(0.24f + swayX + bobX, -0.22f + swayY - bobY + reloadDrop, -0.45f + recoilZ);
        rlRotatef(recoilPitch * RAD2DEG, 1.0f, 0.0f, 0.0f);

        Color bodyCol = Color{ 44, 50, 62, 255 };
        Color ledCol = isStealthFlank ? SKYBLUE : st.accentColor;

        if (st.isMelee) {
            DrawCube({ 0.0f, 0.0f, -0.08f }, 0.03f, 0.04f, 0.28f, LIGHTGRAY);
            DrawCube({ 0.0f, 0.0f,  0.08f }, 0.04f, 0.05f, 0.12f, DARKGRAY);
        } else if (st.isGrenade) {
            DrawSphere({ 0.0f, 0.0f, 0.0f }, 0.075f, st.id == WeaponId::GADGET_FLASH ? SKYBLUE : DARKGREEN);
            DrawSphereWires({ 0.0f, 0.0f, 0.0f }, 0.078f, 6, 6, ledCol);
        } else {
            float barrelLen = (st.id == WeaponId::LMG || st.id == WeaponId::RIFLE) ? 0.44f : 0.24f;
            DrawCube({ 0.0f, 0.0f, 0.0f }, 0.065f, 0.085f, barrelLen, bodyCol);
            DrawCubeWires({ 0.0f, 0.0f, 0.0f }, 0.066f, 0.086f, barrelLen, BLACK);
            DrawCube({ 0.0f, 0.048f, -0.02f }, 0.028f, 0.016f, barrelLen * 0.65f, ledCol);

            if (muzzleFlashTimer > 0.0f) {
                DrawSphere({ 0.0f, 0.01f, -barrelLen * 0.5f - 0.06f }, 0.055f, Fade(YELLOW, 0.85f));
            }
        }

        rlPopMatrix();
    }
};