#pragma once

#include <random>
#include <cmath>
#include "mpe/events.h"
#include "global/thirdparty/picojson/picojson.h"
#include <fstream>

namespace humanizer {

class Humanizer {
public:
    struct Settings {
        // All values are max deviation as a fraction (0.0 - 1.0)
        double timingAmount    = 0.4;  // ±2% of note duration
        double durationAmount  = 0.05;  // ±5% of note duration
        double dynamicAmount   = 0.02;  // ±2% of dynamic level
        int    tuningAmount    = 50;     // ±2 cents (anything more sounds HORRIBLE)
        bool   enabled         = true;
    };

    static Humanizer& instance() {
        static Humanizer h;
        return h;
    }

    Settings settings;

    void loadConfig(const std::string& path) {
        std::ifstream f(path);
        if (!f.is_open()) return; // silently use defaults if no config file
        
        std::string err;
        picojson::value v;
        picojson::parse(v, f);
        
        if (!v.is<picojson::object>()) return;
        auto& root = v.get<picojson::object>();
        if (root.find("humanizer") == root.end()) return;
        
        auto& h = root["humanizer"].get<picojson::object>();
        
        if (h.count("enabled"))       settings.enabled       = h["enabled"].get<bool>();
        if (h.count("timingAmount"))  settings.timingAmount  = h["timingAmount"].get<double>();
        if (h.count("durationAmount")) settings.durationAmount = h["durationAmount"].get<double>();
        if (h.count("dynamicAmount")) settings.dynamicAmount = h["dynamicAmount"].get<double>();
        if (h.count("tuningAmount"))  settings.tuningAmount  = (int)h["tuningAmount"].get<double>();
        
        if (h.count("slurCancelInstruments")) {
            m_slurCancelInstruments.clear();
            for (auto& item : h["slurCancelInstruments"].get<picojson::array>()) {
                m_slurCancelInstruments.insert(item.get<std::string>());
            }
        }
    }

    bool shouldCancelSlur(const std::string& instrumentId) const {
        return m_slurCancelInstruments.count(instrumentId) > 0;
    }

    // For NoteRenderer (soundfont path)
    // Only nudge forward to prevent note from playing before dynamic change, etc.
    muse::mpe::timestamp_t nudgeTimestamp(muse::mpe::timestamp_t t, muse::mpe::duration_t d) {
        if (!settings.enabled) return t;
        return t + static_cast<muse::mpe::timestamp_t>(d * settings.timingAmount * positiveRand());
    }

    muse::mpe::duration_t nudgeDuration(muse::mpe::duration_t d) {
        if (!settings.enabled) return d;
        return d + static_cast<muse::mpe::duration_t>(d * settings.durationAmount * positiveRand());
    }

    muse::mpe::dynamic_level_t nudgeDynamic(muse::mpe::dynamic_level_t level) {
        if (!settings.enabled) return level;
        double offset = level * settings.dynamicAmount * unitRand();
        return static_cast<muse::mpe::dynamic_level_t>(std::clamp(
            static_cast<double>(level) + offset,
            0.0,
            static_cast<double>(muse::mpe::MAX_DYNAMIC_LEVEL)
        ));
    }

    // For MuseSamplerSequencer (MuseSounds path)
    long long nudgeTimestampUs(long long us, long long durationUs) {
        if (!settings.enabled) return us;
        return us + static_cast<long long>(durationUs * settings.timingAmount * unitRand());
    }

    long long nudgeDurationUs(long long us) {
        if (!settings.enabled) return us;
        return us + static_cast<long long>(us * settings.durationAmount * unitRand());
    }

    int nudgeCents(int cents) {
        if (!settings.enabled) return cents;
        return cents + static_cast<int>(settings.tuningAmount * unitRand());
    }

private:
    Humanizer() : m_rng(std::random_device{}()), m_dist(-1.0, 1.0) {}

    double unitRand() { return m_dist(m_rng); }

    std::mt19937 m_rng;
    std::uniform_real_distribution<double> m_dist;
    

    double positiveRand() { return m_posDist(m_rng); }

    std::uniform_real_distribution<double> m_posDist{ 0.0, 1.0 };
    std::unordered_set<std::string> m_slurCancelInstruments;
};

} // namespace mu::engraving