#pragma once

#include "raylib.h"
#include "raymath.h"
#include "World.hpp"
#include "Player.hpp"
#include "Weapon.hpp"
#include "SquadAI.hpp"
#include "Events.hpp"
#include <array>
#include <string>
#include <vector>
#include <algorithm>
#include <cmath>

// =============================================================================
// INVENTÁRIO UNIFICADO DO COMBATENTE (SAVEPOINT_22)
// =============================================================================
struct CombatantInventory {
    int activeSlot = 1;
    WeaponId primaryWeapon = WeaponId::RIFLE;
    WeaponId secondaryWeapon = WeaponId::PISTOL;
    std::array<int, static_cast<size_t>(WeaponId::COUNT)> ammo = { 30, 8, 80, 15, 6, 1, 2, 2 };

    void RefillAll() {
        for (size_t i = 0; i < static_cast<size_t>(WeaponId::COUNT); i++) {
            ammo[i] = GetWeaponStats(static_cast<WeaponId>(i)).magSize;
        }
    }

    WeaponId GetEquippedWeaponId() const {
        if (activeSlot == 1) return primaryWeapon;
        if (activeSlot == 2) return secondaryWeapon;
        if (activeSlot == 3) return WeaponId::MELEE_KNIFE;
        if (activeSlot == 4) return WeaponId::GADGET_FRAG;
        return WeaponId::GADGET_FLASH;
    }

    int GetActiveAmmo() const {
        return ammo[static_cast<size_t>(GetEquippedWeaponId())];
    }

    void SyncFromViewmodel(const ViewmodelWeapon& w) {
        activeSlot = w.currentSlot;
        primaryWeapon = (w.primarySubIndex == 1) ? WeaponId::SHOTGUN
                      : ((w.primarySubIndex == 2) ? WeaponId::LMG : WeaponId::RIFLE);
        secondaryWeapon = (w.secondarySubIndex == 1) ? WeaponId::REVOLVER : WeaponId::PISTOL;
        ammo = w.ammo;
    }

    void ApplyToViewmodel(ViewmodelWeapon& w) const {
        w.currentSlot = activeSlot;
        w.ammo = ammo;
    }
};

// =============================================================================
// ENTIDADE UNIFICADA: COMBATANT (JOGADOR OU ALIADO IA)
// =============================================================================
struct Combatant {
    int slot = 0; // 0: Alpha, 1: Bravo, 2: Charlie, 3: Delta
    std::string callsign = "Alpha";
    Color teamColor = WHITE;
    bool isPlayerControlled = false;

    Vector3 pos = { 0.0f, 0.0f, 0.0f };
    float eyeHeight = 1.65f;
    bool isCrouched = false;

    // Saúde, Estado Caído & Reanimação Unificados
    float hp = 100.0f;
    float maxHp = 100.0f;
    float lastHp = 100.0f;
    bool isDowned = false;
    bool isBeingDragged = false;
    float bleedoutTimer = 25.0f;
    float reviveProgress = 0.0f;

    // Ameaça & Supressão Unificados
    float aggro = 0.0f;
    float suppression = 0.0f;
    bool isSuppressed = false;

    // Inventário Unificado
    CombatantInventory inventory;

    Vector3 GetEyePos() const noexcept {
        float h = isDowned ? 0.55f : ((isCrouched || isSuppressed) ? 1.15f : eyeHeight);
        return { pos.x, pos.y + h, pos.z };
    }

    void ApplyDamage(float amount, int attackerSlot, Vector3 attackerPos, bool isExplosion = false) {
        if (isDowned || amount <= 0.0f) return;

        hp = std::max(0.0f, hp - amount);
        suppression = std::min(100.0f, suppression + amount * 2.0f);

        EventManager::Emit(DamageDealtEvent{
            attackerSlot,
            slot,
            false,
            amount,
            false,
            false,
            isExplosion,
            false,
            GetEyePos(),
            attackerPos
        });

        if (hp <= 0.0f) {
            isDowned = true;
            aggro = 0.0f;
            suppression = 0.0f;
            isSuppressed = false;
            bleedoutTimer = 25.0f;
            reviveProgress = 0.0f;

            EventManager::Emit(CombatantDownedEvent{
                slot,
                callsign,
                pos
            });
        }
    }

    void TickStatus(float dt, float aggroFloor, float aggroDecay, float suppDecay) {
        if (isDowned) {
            aggro = 0.0f;
            suppression = 0.0f;
            isSuppressed = false;
            bleedoutTimer = std::max(0.0f, bleedoutTimer - dt);
            return;
        }

        if (hp < lastHp) {
            suppression = std::min(100.0f, suppression + (lastHp - hp) * 2.1f);
        }
        lastHp = hp;

        suppression = std::max(0.0f, suppression - suppDecay * dt);
        aggro = std::max(aggroFloor, aggro - aggroDecay * dt);

        if (!isSuppressed && suppression >= 45.0f) {
            isSuppressed = true;
        } else if (isSuppressed && suppression < 20.0f) {
            isSuppressed = false;
        }
    }

    bool AdvanceRevive(float dt, int reviverSlot, float rate = 0.42f) {
        if (!isDowned) return false;
        reviveProgress += dt * rate;
        if (reviveProgress >= 1.0f) {
            isDowned = false;
            hp = maxHp * 0.75f;
            lastHp = hp;
            reviveProgress = 0.0f;
            bleedoutTimer = 25.0f;

            EventManager::Emit(CombatantRevivedEvent{
                reviverSlot,
                slot,
                callsign,
                pos
            });
            return true;
        }
        return false;
    }

    void DecayRevive(float dt) {
        if (isDowned && reviveProgress > 0.0f) {
            reviveProgress = std::max(0.0f, reviveProgress - dt * 0.5f);
        }
    }

    void SyncFromPlayer(const SourcePlayer& p, const ViewmodelWeapon& w) {
        pos = p.pos;
        eyeHeight = p.currentHeight - 0.15f;
        isCrouched = p.isCrouched;
        hp = p.hp;
        aggro = p.aggro;
        isDowned = p.isDowned;
        reviveProgress = p.reviveProgress;
        inventory.SyncFromViewmodel(w);
    }

    void ApplyToPlayer(SourcePlayer& p, ViewmodelWeapon& w) const {
        p.hp = hp;
        p.aggro = aggro;
        p.isDowned = isDowned;
        p.reviveProgress = reviveProgress;
        if (isDowned) p.draggingAllyId = -1;
        inventory.ApplyToViewmodel(w);
    }

    void SyncFromAlly(const SquadAlly& a) {
        pos = a.pos;
        isCrouched = a.isSuppressed;
        hp = a.hp;
        lastHp = a.lastHp;
        aggro = a.aggro;
        suppression = a.suppression;
        isSuppressed = a.isSuppressed;
        isDowned = a.isDowned;
        isBeingDragged = a.isBeingDragged;
        bleedoutTimer = a.bleedoutTimer;
        reviveProgress = a.reviveProgress;
    }

    void ApplyToAlly(SquadAlly& a) const {
        a.hp = hp;
        a.lastHp = lastHp;
        a.aggro = aggro;
        a.suppression = suppression;
        a.isSuppressed = isSuppressed;
        a.isDowned = isDowned;
        a.isBeingDragged = isBeingDragged;
        a.bleedoutTimer = bleedoutTimer;
        a.reviveProgress = reviveProgress;
    }
};

// =============================================================================
// GERENCIADOR DO ESQUADRAO UNIFICADO (4 OPERADORES)
// =============================================================================
class SquadRoster {
public:
    std::array<Combatant, 4> members;
    int activeCount = 4;

    void InitializeFromEntities(const SourcePlayer& player,
                                const ViewmodelWeapon& weapon,
                                const std::vector<SquadAlly>& allies,
                                int localSlot = 0) {
        const char* defaultNames[4] = { "Alpha", "Bravo", "Charlie", "Delta" };
        Color defaultColors[4] = { WHITE, SKYBLUE, LIME, GOLD };

        for (int i = 0; i < 4; i++) {
            members[i] = Combatant{};
            members[i].slot = i;
            members[i].callsign = defaultNames[i];
            members[i].teamColor = defaultColors[i];
            members[i].inventory.RefillAll();
        }

        int clampedLocal = std::clamp(localSlot, 0, 3);
        members[clampedLocal].isPlayerControlled = true;
        members[clampedLocal].SyncFromPlayer(player, weapon);

        activeCount = 1 + static_cast<int>(allies.size());
        int allyCursor = 0;
        for (int i = 0; i < 4 && allyCursor < static_cast<int>(allies.size()); i++) {
            if (i == clampedLocal) continue;
            members[i].isPlayerControlled = false;
            members[i].callsign = allies[allyCursor].name;
            members[i].teamColor = allies[allyCursor].color;
            members[i].SyncFromAlly(allies[allyCursor]);
            allyCursor++;
        }
    }

    void SyncFromEntities(const SourcePlayer& player,
                          const ViewmodelWeapon& weapon,
                          const std::vector<SquadAlly>& allies,
                          int localSlot = 0) {
        int clampedLocal = std::clamp(localSlot, 0, 3);
        members[clampedLocal].SyncFromPlayer(player, weapon);

        int allyCursor = 0;
        for (int i = 0; i < 4 && allyCursor < static_cast<int>(allies.size()); i++) {
            if (i == clampedLocal) continue;
            members[i].SyncFromAlly(allies[allyCursor]);
            allyCursor++;
        }
    }

    void PushToEntities(SourcePlayer& player,
                        ViewmodelWeapon& weapon,
                        std::vector<SquadAlly>& allies,
                        int localSlot = 0) const {
        int clampedLocal = std::clamp(localSlot, 0, 3);
        members[clampedLocal].ApplyToPlayer(player, weapon);

        int allyCursor = 0;
        for (int i = 0; i < 4 && allyCursor < static_cast<int>(allies.size()); i++) {
            if (i == clampedLocal) continue;
            members[i].ApplyToAlly(allies[allyCursor]);
            allyCursor++;
        }
    }

    // Reanimação cooperativa unificada para qualquer membro caído próximo
    bool TryReviveNearby(int reviverSlot, float dt, float maxRange = 3.0f) {
        if (reviverSlot < 0 || reviverSlot >= activeCount) return false;
        Combatant& reviver = members[reviverSlot];
        if (reviver.isDowned) return false;

        int targetSlot = -1;
        float bestDist = maxRange;
        for (int i = 0; i < activeCount; i++) {
            if (i == reviverSlot || !members[i].isDowned) continue;
            float d = Vector3Distance(reviver.pos, members[i].pos);
            if (d < bestDist) {
                bestDist = d;
                targetSlot = i;
            }
        }

        if (targetSlot != -1) {
            members[targetSlot].AdvanceRevive(dt, reviverSlot, 0.42f);
            return true;
        }
        return false;
    }

    bool IsStealthFlankActive(int localSlot = 0) const {
        int clampedLocal = std::clamp(localSlot, 0, 3);
        const Combatant& self = members[clampedLocal];
        if (self.isDowned || self.aggro >= 25.0f || activeCount <= 1) return false;

        for (int i = 0; i < activeCount; i++) {
            if (i == clampedLocal) continue;
            if (!members[i].isDowned && members[i].aggro > 50.0f) {
                return true;
            }
        }
        return false;
    }
};