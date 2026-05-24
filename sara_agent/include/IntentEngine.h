#pragma once
#include <string>
#include <vector>

namespace sara {

// ── Intent Categories ─────────────────────────────────────────────────────────
enum class Intent {
    media_playback,       // YouTube, Spotify, music, video
    event_automation,     // "when X happens", "if Y then Z", triggers/rules
    surveillance,         // screenshots, camera, monitoring, recording
    system_control,       // volume, brightness, processes, power, WiFi, BT
    browser_automation,   // web search, open websites, tabs
    productivity,         // clipboard, reminders, scheduling, timers
    communication,        // Telegram, notifications, messaging
    monitoring,           // CPU, RAM, battery, network stats, health
    reminder,             // delayed actions, alarms, countdowns
    conversational,       // greetings, questions, chat (no tool needed)
    unknown               // fallback — let AI figure it out
};

struct IntentMatch {
    Intent      intent;
    float       confidence;      // 0.0 – 1.0
    std::string category_name;  // human-readable string
};

// ── Intent Engine ─────────────────────────────────────────────────────────────
// Fast keyword-based classifier. Zero AI calls. Used to pre-route before
// sending to the AI planner so the planner gets the right tool context.
class IntentEngine {
public:
    static IntentEngine& instance();

    IntentMatch classify(const std::string& text) const;
    std::string to_string(Intent intent) const;

    // Hint the AI planner with extra context based on classified intent
    std::string build_intent_hint(Intent intent) const;

private:
    IntentEngine() = default;

    struct KeywordGroup {
        Intent                   intent;
        std::vector<std::string> keywords;
        float                    base_confidence;
    };

    std::vector<KeywordGroup> build_groups() const;
    std::string               lowercase(const std::string& s) const;
};

} // namespace sara
