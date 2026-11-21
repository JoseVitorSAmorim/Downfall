#pragma once

#include "raylib.h"
#include "Weapon.hpp"
#include "Audio.hpp"
#include <fstream>
#include <sstream>
#include <string>
#include <algorithm>
#include <cmath>
#include <cstdio>

struct PlayerSaveData {
    // Equipamento escolhido no Briefing / Loadout
    int primaryWeaponIndex = 0;   // 0 = Fuzil, 1 = Escopeta, 2 = LMG
    int secondaryWeaponIndex = 0; // 0 = Pistola, 1 = Revolver
    int gadgetIndex = 0;          // 0 = Frag (Slot 4), 1 = Flashbang (Slot 5)

    // Preferências de Jogabilidade e Áudio
    float mouseSensitivity = 0.0022f;
    float baseFov = 75.0f;
    float sfxVolume = 0.90f;
    float musicVolume = 0.42f;

    // Progresso da Campanha e Estatísticas Acumuladas
    int unlockedPhase = 1;        // 1 a 3
    int selectedPhase = 1;        // 1 a 3
    int missionsCompleted = 0;
    int totalKills = 0;
    int totalHeadshots = 0;
};

class SaveSystem {
public:
    static constexpr int MAX_PHASES = 3;
    PlayerSaveData data;
    bool loadedFromDisk = false;
    float saveBannerTimer = 0.0f;

    bool Load(const char* filename = "savegame.cfg") {
        std::ifstream file(filename);
        if (!file.is_open()) {
            loadedFromDisk = false;
            Save(filename);
            return false;
        }

        std::string line;
        while (std::getline(file, line)) {
            if (line.empty() || line[0] == '#' || line[0] == ';') continue;

            size_t eqPos = line.find('=');
            if (eqPos == std::string::npos) continue;

            std::string key = Trim(line.substr(0, eqPos));
            std::string val = Trim(line.substr(eqPos + 1));

            try {
                if (key == "primaryWeaponIndex")        data.primaryWeaponIndex = std::stoi(val);
                else if (key == "secondaryWeaponIndex") data.secondaryWeaponIndex = std::stoi(val);
                else if (key == "gadgetIndex")          data.gadgetIndex = std::stoi(val);
                else if (key == "mouseSensitivity")     data.mouseSensitivity = std::stof(val);
                else if (key == "baseFov")              data.baseFov = std::stof(val);
                else if (key == "sfxVolume")            data.sfxVolume = std::stof(val);
                else if (key == "musicVolume")          data.musicVolume = std::stof(val);
                else if (key == "unlockedPhase")        data.unlockedPhase = std::stoi(val);
                else if (key == "selectedPhase")        data.selectedPhase = std::stoi(val);
                else if (key == "missionsCompleted")    data.missionsCompleted = std::stoi(val);
                else if (key == "totalKills")           data.totalKills = std::stoi(val);
                else if (key == "totalHeadshots")       data.totalHeadshots = std::stoi(val);
            } catch (...) {
                // Ignora linhas malformadas preservando valores seguros
            }
        }

        ClampValues();
        loadedFromDisk = true;
        return true;
    }

    bool Save(const char* filename = "savegame.cfg") {
        ClampValues();
        std::ofstream file(filename);
        if (!file.is_open()) return false;

        file << "# Squad Tactical Shooter - Savegame & Configuracoes (SAVEPOINT_18)\n";
        file << "primaryWeaponIndex=" << data.primaryWeaponIndex << "\n";
        file << "secondaryWeaponIndex=" << data.secondaryWeaponIndex << "\n";
        file << "gadgetIndex=" << data.gadgetIndex << "\n";
        file << "mouseSensitivity=" << data.mouseSensitivity << "\n";
        file << "baseFov=" << data.baseFov << "\n";
        file << "sfxVolume=" << data.sfxVolume << "\n";
        file << "musicVolume=" << data.musicVolume << "\n";
        file << "unlockedPhase=" << data.unlockedPhase << "\n";
        file << "selectedPhase=" << data.selectedPhase << "\n";
        file << "missionsCompleted=" << data.missionsCompleted << "\n";
        file << "totalKills=" << data.totalKills << "\n";
        file << "totalHeadshots=" << data.totalHeadshots << "\n";

        saveBannerTimer = 2.0f;
        return true;
    }

    void ApplyAudioSettings(ProceduralAudio& audio) const {
        audio.sfxVolume = std::clamp(data.sfxVolume, 0.0f, 1.0f);
        audio.musicVolume = std::clamp(data.musicVolume, 0.0f, 1.0f);
    }

    WeaponId GetPrimaryWeaponId() const {
        switch (data.primaryWeaponIndex) {
            case 1:  return WeaponId::SHOTGUN;
            case 2:  return WeaponId::LMG;
            default: return WeaponId::RIFLE;
        }
    }

    WeaponId GetSecondaryWeaponId() const {
        switch (data.secondaryWeaponIndex) {
            case 1:  return WeaponId::REVOLVER;
            default: return WeaponId::PISTOL;
        }
    }

    const char* GetPrimaryName() const {
        switch (data.primaryWeaponIndex) {
            case 1:  return "ESCOPETA TATICA (12 GA)";
            case 2:  return "METRALHADORA LEVE (LMG)";
            default: return "FUZIL DE ASSALTO (5.56mm)";
        }
    }

    const char* GetSecondaryName() const {
        switch (data.secondaryWeaponIndex) {
            case 1:  return "REVOLVER PESADO (.44)";
            default: return "PISTOLA TATICA (9mm)";
        }
    }

    const char* GetGadgetName() const {
        return (data.gadgetIndex == 1) ? "GRANADA FLASHBANG [Slot 5]" : "GRANADA FRAGMENTACAO [Slot 4]";
    }

    void ApplyLoadoutToWeapon(ViewmodelWeapon& weapon) const {
        WeaponId desiredPrimary = GetPrimaryWeaponId();
        weapon.SelectSlot(1);
        for (int i = 0; i < 4 && weapon.GetCurrentStats().id != desiredPrimary; i++) {
            weapon.SelectSlot(1);
        }

        WeaponId desiredSecondary = GetSecondaryWeaponId();
        weapon.SelectSlot(2);
        for (int i = 0; i < 3 && weapon.GetCurrentStats().id != desiredSecondary; i++) {
            weapon.SelectSlot(2);
        }

        // Inicia a missão com a arma primária selecionada
        weapon.SelectSlot(1);
    }

    void RecordMissionVictory(int phasePlayed, int kills, int headshots, const char* filename = "savegame.cfg") {
        data.missionsCompleted++;
        data.totalKills += std::max(0, kills);
        data.totalHeadshots += std::max(0, headshots);

        if (phasePlayed >= data.unlockedPhase && data.unlockedPhase < MAX_PHASES) {
            data.unlockedPhase = phasePlayed + 1;
            data.selectedPhase = data.unlockedPhase;
        }
        Save(filename);
    }

    void ResetSettingsToDefault(ProceduralAudio& audio, const char* filename = "savegame.cfg") {
        data.mouseSensitivity = 0.0022f;
        data.baseFov = 75.0f;
        data.sfxVolume = 0.90f;
        data.musicVolume = 0.42f;
        ApplyAudioSettings(audio);
        Save(filename);
    }

    void ResetAllProgress(const char* filename = "savegame.cfg") {
        data.unlockedPhase = 1;
        data.selectedPhase = 1;
        data.missionsCompleted = 0;
        data.totalKills = 0;
        data.totalHeadshots = 0;
        Save(filename);
    }

private:
    void ClampValues() {
        data.primaryWeaponIndex = std::clamp(data.primaryWeaponIndex, 0, 2);
        data.secondaryWeaponIndex = std::clamp(data.secondaryWeaponIndex, 0, 1);
        data.gadgetIndex = std::clamp(data.gadgetIndex, 0, 1);
        data.mouseSensitivity = std::clamp(data.mouseSensitivity, 0.0005f, 0.0060f);
        data.baseFov = std::clamp(data.baseFov, 60.0f, 100.0f);
        data.sfxVolume = std::clamp(data.sfxVolume, 0.0f, 1.0f);
        data.musicVolume = std::clamp(data.musicVolume, 0.0f, 1.0f);
        data.unlockedPhase = std::clamp(data.unlockedPhase, 1, MAX_PHASES);
        data.selectedPhase = std::clamp(data.selectedPhase, 1, data.unlockedPhase);
        data.missionsCompleted = std::max(0, data.missionsCompleted);
        data.totalKills = std::max(0, data.totalKills);
        data.totalHeadshots = std::max(0, data.totalHeadshots);
    }

    static std::string Trim(const std::string& s) {
        size_t start = 0;
        while (start < s.size() && (s[start] == ' ' || s[start] == '\t' || s[start] == '\r' || s[start] == '\n')) {
            start++;
        }
        size_t end = s.size();
        while (end > start && (s[end - 1] == ' ' || s[end - 1] == '\t' || s[end - 1] == '\r' || s[end - 1] == '\n')) {
            end--;
        }
        return s.substr(start, end - start);
    }
};