#include "../include/IntentEngine.h"
#include <algorithm>
#include <cctype>
#include <sstream>

namespace sara {

// ─────────────────────────────────────────────────────────────────────────────
IntentEngine& IntentEngine::instance() {
    static IntentEngine inst;
    return inst;
}

// ─────────────────────────────────────────────────────────────────────────────
std::string IntentEngine::lowercase(const std::string& s) const {
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(),
        [](unsigned char c) { return std::tolower(c); });
    return out;
}

// Helper for strict word boundary matching
static bool is_word_match(const std::string& text, const std::string& kw) {
    size_t pos = 0;
    while ((pos = text.find(kw, pos)) != std::string::npos) {
        bool left_ok = (pos == 0) || !std::isalnum((unsigned char)text[pos - 1]);
        bool right_ok = (pos + kw.length() == text.length()) || !std::isalnum((unsigned char)text[pos + kw.length()]);
        if (left_ok && right_ok) return true;
        pos += kw.length();
    }
    return false;
}

// ─────────────────────────────────────────────────────────────────────────────
std::vector<IntentEngine::KeywordGroup> IntentEngine::build_groups() const {
    return {
        { Intent::media_playback, {
            "play", "youtube", "spotify", "music", "song", "video",
            "watch", "listen", "stream", "pause", "resume", "next",
            "previous", "shuffle", "playlist", "track", "album", "artist", "stop music"
        }, 0.85f },

        { Intent::event_automation, {
            "when", "if", "trigger", "whenever", "every time",
            "on start", "on open", "on close", "rule", "automation",
            "watch", "monitor for", "detect", "react", "schedule when"
        }, 0.90f },

        { Intent::surveillance, {
            "screenshot", "photo", "camera", "picture", "capture",
            "record", "monitor screen", "take photo", "take a photo",
            "webcam", "snap", "image"
        }, 0.90f },

        { Intent::system_control, {
            "volume", "mute", "unmute", "brightness", "shutdown",
            "restart", "sleep", "hibernate", "kill", "close", "open",
            "launch", "start app", "kill process", "end task", "wifi",
            "bluetooth", "airplane mode", "power", "battery saver",
            "write", "create", "file", "folder", "directory"
        }, 0.80f },

        { Intent::browser_automation, {
            "search", "google", "open website", "browse", "url",
            "chrome", "edge", "firefox", "tab", "webpage", "website",
            "navigate", "go to", "find online"
        }, 0.80f },

        { Intent::productivity, {
            "clipboard", "copy", "paste", "remind me", "reminder",
            "note", "schedule", "todo", "task", "alarm", "type",
            "write", "paste text"
        }, 0.75f },

        { Intent::communication, {
            "send", "message", "notify", "notification", "alert",
            "telegram", "discord", "whatsapp", "email", "tell me"
        }, 0.75f },

        { Intent::monitoring, {
            "cpu", "ram", "memory", "disk", "battery", "temperature",
            "gpu", "performance", "stats", "usage", "how much",
            "system info", "health", "ip", "network", "speed", "status"
        }, 0.85f },

        { Intent::reminder, {
            "after", "in 5 minutes", "in an hour", "delay",
            "wait", "then", "later", "at ", "timer", "countdown",
            "minutes from now", "hours from now"
        }, 0.80f },

        { Intent::conversational, {
            "hello", "hi", "hey", "thanks", "thank you", "who are you",
            "what are you", "how are you", "what time", "what date",
            "what day", "your name", "help me"
        }, 0.85f },
    };
}

// ─────────────────────────────────────────────────────────────────────────────
IntentMatch IntentEngine::classify(const std::string& text) const {
    std::string lower = lowercase(text);
    auto groups = build_groups();

    Intent best_intent    = Intent::unknown;
    float  best_score     = 0.0f;
    std::string best_name = "unknown";

    for (auto& group : groups) {
        float score = 0.0f;
        int match_count = 0;
        
        for (auto& kw : group.keywords) {
            if (is_word_match(lower, kw)) {
                match_count++;
                float kw_score = group.base_confidence
                    * (1.0f + 0.05f * (float)std::count(kw.begin(), kw.end(), ' '));
                
                if (score == 0.0f) {
                    score = kw_score;
                } else {
                    // Reward multiple matches in the same intent group
                    score = std::min(1.0f, score + 0.05f);
                }
            }
        }
        
        if (score > best_score) {
            best_score  = score;
            best_intent = group.intent;
            best_name   = to_string(group.intent);
        }
    }

    return { best_intent, best_score, best_name };
}

// ─────────────────────────────────────────────────────────────────────────────
std::string IntentEngine::to_string(Intent intent) const {
    switch (intent) {
        case Intent::media_playback:    return "media_playback";
        case Intent::event_automation:  return "event_automation";
        case Intent::surveillance:      return "surveillance";
        case Intent::system_control:    return "system_control";
        case Intent::browser_automation:return "browser_automation";
        case Intent::productivity:      return "productivity";
        case Intent::communication:     return "communication";
        case Intent::monitoring:        return "monitoring";
        case Intent::reminder:          return "reminder";
        case Intent::conversational:    return "conversational";
        default:                        return "unknown";
    }
}

// ─────────────────────────────────────────────────────────────────────────────
std::string IntentEngine::build_intent_hint(Intent intent) const {
    switch (intent) {
        case Intent::media_playback:
            return "HINT: User wants to play/control media. "
                   "Use play_youtube for new music/videos. "
                   "Use media_play_pause, media_next, media_prev, media_stop for active playback control.";
        case Intent::event_automation:
            return "HINT: User wants to set up an automation rule based on an event trigger. "
                   "Use add_event_rule with appropriate trigger_type "
                   "(process_start, process_stop, cpu_high, battery_low, idle_detected). "
                   "The actions_json field MUST be a valid JSON array of tool calls.";
        case Intent::surveillance:
            return "HINT: User wants visual capture. "
                   "Use take_screenshot for screen, capture_camera for webcam. "
                   "Result will be automatically sent back as photo.";
        case Intent::system_control:
            return "HINT: User wants to control system settings, processes, files, OR delay an action. "
                   "For delays, pass 'delay_seconds' parameter to any tool. "
                   "Use volume_set, change_brightness, open_app, close_process, write_file as appropriate.";
        case Intent::browser_automation:
            return "HINT: User wants to browse or search. "
                   "Use play_youtube for YouTube, search_google for web search, "
                   "open_website for specific URLs.";
        case Intent::monitoring:
            return "HINT: User wants system information. "
                   "Use get_system_stats for CPU/RAM/battery/disk info. "
                   "Use get_ip_address for network info.";
        case Intent::reminder:
            return "HINT: User wants a delayed action. "
                   "Use delay_seconds parameter on the action step. "
                   "Use notify for reminders.";
        case Intent::conversational:
            return "HINT: This is casual conversation. "
                   "Respond naturally in response_text with NO tool calls. "
                   "Leave execution_plan empty.";
        default:
            return "";
    }
}

} // namespace sara
