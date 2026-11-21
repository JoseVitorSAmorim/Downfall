#pragma once

#include "raylib.h"
#include <cmath>
#include <cstdlib>
#include <algorithm>

class ProceduralAudio {
public:
    bool initialized = false;
    bool muted = false;

    Sound sfxShootNormal = { 0 };
    Sound sfxShootStealth = { 0 };
    Sound sfxJump = { 0 };
    Sound sfxOrder = { 0 };
    Sound sfxDowned = { 0 };
    Sound sfxRevive = { 0 };

    void Init() {
        InitAudioDevice();
        if (!IsAudioDeviceReady()) {
            initialized = false;
            return;
        }

        sfxShootNormal  = GenerateShootSound(false);
        sfxShootStealth = GenerateShootSound(true);
        sfxJump         = GenerateJumpSound();
        sfxOrder        = GenerateOrderSound();
        sfxDowned       = GenerateDownedSound();
        sfxRevive       = GenerateReviveSound();

        initialized = true;
    }

    void Shutdown() {
        if (!initialized) return;
        UnloadSound(sfxShootNormal);
        UnloadSound(sfxShootStealth);
        UnloadSound(sfxJump);
        UnloadSound(sfxOrder);
        UnloadSound(sfxDowned);
        UnloadSound(sfxRevive);
        CloseAudioDevice();
        initialized = false;
    }

    void ToggleMute() {
        muted = !muted;
    }

    void PlayShoot(bool isStealth) {
        if (!initialized || muted) return;
        Sound& s = isStealth ? sfxShootStealth : sfxShootNormal;
        float pitchVar = (float)GetRandomValue(94, 106) / 100.0f;
        SetSoundPitch(s, pitchVar);
        PlaySound(s);
    }

    void PlayJump() {
        if (!initialized || muted) return;
        float pitchVar = (float)GetRandomValue(97, 104) / 100.0f;
        SetSoundPitch(sfxJump, pitchVar);
        PlaySound(sfxJump);
    }

    void PlayOrder() {
        if (!initialized || muted) return;
        PlaySound(sfxOrder);
    }

    void PlayDownedAlert() {
        if (!initialized || muted) return;
        PlaySound(sfxDowned);
    }

    void PlayRevive() {
        if (!initialized || muted) return;
        PlaySound(sfxRevive);
    }

private:
    static constexpr int SAMPLE_RATE = 44100;

    static Sound BuildSoundFromSamples(short* samples, int frameCount, float volume) {
        Wave wave = { 0 };
        wave.frameCount = (unsigned int)frameCount;
        wave.sampleRate = SAMPLE_RATE;
        wave.sampleSize = 16;
        wave.channels   = 1;
        wave.data       = samples;

        Sound sound = LoadSoundFromWave(wave);
        SetSoundVolume(sound, volume);
        UnloadWave(wave); // Libera wave.data alocado via MemAlloc
        return sound;
    }

    static short ClampSample(float v) {
        float clamped = std::clamp(v, -1.0f, 1.0f);
        return (short)(clamped * 32767.0f);
    }

    static float WhiteNoise() {
        return ((float)rand() / (float)RAND_MAX) * 2.0f - 1.0f;
    }

    // Disparo: varredura de frequência + ruído com envelope exponencial
    static Sound GenerateShootSound(bool stealth) {
        float duration = stealth ? 0.13f : 0.18f;
        int frames = (int)(SAMPLE_RATE * duration);
        short* data = (short*)MemAlloc(frames * sizeof(short));

        float phase = 0.0f;
        float lastNoise = 0.0f;

        for (int i = 0; i < frames; i++) {
            float t = (float)i / (float)SAMPLE_RATE;
            float env = expf(-t * (stealth ? 36.0f : 24.0f));

            float startFreq = stealth ? 660.0f : 310.0f;
            float endFreq   = stealth ? 180.0f : 52.0f;
            float freq = endFreq + (startFreq - endFreq) * expf(-t * 32.0f);

            phase += 2.0f * PI * freq / (float)SAMPLE_RATE;
            float tone = sinf(phase);

            // Filtro passa-baixa simples no ruído para dar corpo ao disparo
            float rawNoise = WhiteNoise();
            lastNoise = lastNoise * 0.65f + rawNoise * 0.35f;

            float mix = stealth
                ? (tone * 0.45f + rawNoise * 0.35f) * env
                : (tone * 0.55f + lastNoise * 0.65f) * env;

            data[i] = ClampSample(mix);
        }

        return BuildSoundFromSamples(data, frames, stealth ? 0.45f : 0.55f);
    }

    // Pulo: varredura ascendente limpa
    static Sound GenerateJumpSound() {
        float duration = 0.14f;
        int frames = (int)(SAMPLE_RATE * duration);
        short* data = (short*)MemAlloc(frames * sizeof(short));

        float phase = 0.0f;
        for (int i = 0; i < frames; i++) {
            float t = (float)i / (float)SAMPLE_RATE;
            float progress = t / duration;
            float attack = std::min(progress * 8.0f, 1.0f);
            float env = attack * (1.0f - progress);

            float freq = 140.0f + 230.0f * progress;
            phase += 2.0f * PI * freq / (float)SAMPLE_RATE;

            float sample = sinf(phase) * 0.6f * env;
            data[i] = ClampSample(sample);
        }

        return BuildSoundFromSamples(data, frames, 0.35f);
    }

    // Comando Tático: bip duplo agudo de rádio
    static Sound GenerateOrderSound() {
        float duration = 0.16f;
        int frames = (int)(SAMPLE_RATE * duration);
        short* data = (short*)MemAlloc(frames * sizeof(short));

        float phase = 0.0f;
        for (int i = 0; i < frames; i++) {
            float t = (float)i / (float)SAMPLE_RATE;
            bool secondPulse = (t > 0.075f);
            float localT = secondPulse ? (t - 0.075f) : t;

            float freq = secondPulse ? 1320.0f : 880.0f;
            float env = expf(-localT * 28.0f);
            if (t > 0.055f && t < 0.075f) env = 0.0f; // Pequena pausa entre os dois bips

            phase += 2.0f * PI * freq / (float)SAMPLE_RATE;
            float wave = (sinf(phase) + 0.25f * sinf(phase * 2.0f)) * 0.45f * env;
            data[i] = ClampSample(wave);
        }

        return BuildSoundFromSamples(data, frames, 0.40f);
    }

    // Alerta de Membro Caído: alarme descendente duplo
    static Sound GenerateDownedSound() {
        float duration = 0.42f;
        int frames = (int)(SAMPLE_RATE * duration);
        short* data = (short*)MemAlloc(frames * sizeof(short));

        float phase = 0.0f;
        for (int i = 0; i < frames; i++) {
            float t = (float)i / (float)SAMPLE_RATE;
            bool secondPulse = (t > 0.21f);
            float localT = secondPulse ? (t - 0.21f) : t;

            float freq = 620.0f - localT * 1100.0f;
            float env = (localT < 0.17f) ? (1.0f - localT / 0.17f) : 0.0f;

            phase += 2.0f * PI * freq / (float)SAMPLE_RATE;
            // Onda dente-de-serra suavizada para soar como alerta tático
            float saw = 2.0f * (phase / (2.0f * PI) - floorf(0.5f + phase / (2.0f * PI)));
            float sample = (sinf(phase) * 0.6f + saw * 0.3f) * 0.5f * env;
            data[i] = ClampSample(sample);
        }

        return BuildSoundFromSamples(data, frames, 0.45f);
    }

    // Reanimação Concluída: arpejo ascendente de confirmação
    static Sound GenerateReviveSound() {
        float duration = 0.36f;
        int frames = (int)(SAMPLE_RATE * duration);
        short* data = (short*)MemAlloc(frames * sizeof(short));

        const float notes[3] = { 523.25f, 659.25f, 783.99f }; // C5, E5, G5
        float phase = 0.0f;

        for (int i = 0; i < frames; i++) {
            float t = (float)i / (float)SAMPLE_RATE;
            int step = std::min((int)(t / 0.12f), 2);
            float localT = t - step * 0.12f;

            float freq = notes[step];
            float env = expf(-localT * 12.0f);

            phase += 2.0f * PI * freq / (float)SAMPLE_RATE;
            float sample = sinf(phase) * 0.5f * env;
            data[i] = ClampSample(sample);
        }

        return BuildSoundFromSamples(data, frames, 0.42f);
    }
};