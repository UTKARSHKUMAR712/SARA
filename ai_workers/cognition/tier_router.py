"""
SARA Tier Router — classifies request complexity to route to the right cognitive tier.

Tier 0: Native execution (0 AI tokens) — handled by C++ NativeCommandRouter
Tier 1: Simple chat/question (~150 tokens, no tools)
Tier 2: Single direct tool action (~400 tokens, targeted tools)
Tier 3: Multi-step goal requiring planning (~650 tokens first, ~450 subsequent)
Tier 4: Autonomous long-horizon task (Tier 3 with max iterations)
"""
import re

# Tier 2: single direct media action keywords
MEDIA_WORDS = {
    'spotify', 'youtube', 'song', 'music', 'video',
    'play', 'pause', 'resume', 'skip', 'next track',
    'previous track', 'mute', 'unmute',
}

# Tier 1: conversational — no tools needed
CHAT_PATTERNS = [
    r'\b(hello|hi|hey|sup)\b',
    r'\bhow are you\b',
    r'\bwho are you\b',
    r'\bwhat (is|are|can)\b',
    r'\btell me (about|a)\b',
    r'\bthank(s| you)\b',
    r'\bgood (morning|night|evening|afternoon)\b',
    r'\bwhat time is it\b',
    r'\bweather\b',
]

# Multi-step connectors → Tier 3
MULTI_STEP_PATTERNS = [
    r'\band then\b', r'\bafter that\b', r'\bthen\b.*\bthen\b',
    r'\balso\b', r'\bfirst\b.*\bthen\b', r'\bfinally\b',
    r', (then|after|and)\b',
]

# Complex single-step task keywords → Tier 3
COMPLEX_WORDS = {
    'create', 'write', 'build', 'setup', 'install', 'configure',
    'html', 'css', 'python', 'code', 'script', 'page',
    'folder', 'directory', 'file', 'server', 'integration',
    'automation', 'schedule', 'monitor',
}

# Single-action OS control → Tier 2
SINGLE_OS_WORDS = {
    'screenshot', 'lock', 'brightness', 'volume',
    'open chrome', 'open spotify', 'open edge', 'close ',
}


def classify_tier(message: str, history: list = None) -> tuple:
    """
    Returns (tier: int, category_hint: str)
    category_hint maps to tool category for tool selection.
    """
    msg_lower = message.lower().strip()

    # Always Tier 3 for continuation loops
    if '[SYSTEM MESSAGE]' in message:
        return (3, 'system')

    # ── Tier 1: Pure chat ────────────────────────────────────────────────────
    # Only classify as chat if NO action words are present
    action_words = {
        'create', 'write', 'open', 'install', 'run', 'build',
        'make', 'start', 'stop', 'play', 'search', 'find',
        'set', 'change', 'get', 'show', 'list',
    }
    has_action = any(w in msg_lower for w in action_words)
    for pat in CHAT_PATTERNS:
        if re.search(pat, msg_lower) and not has_action:
            return (1, 'chat')

    # ── Tier 3: Multi-step / complex ─────────────────────────────────────────
    for pat in MULTI_STEP_PATTERNS:
        if re.search(pat, msg_lower):
            return (3, 'system')

    # Complex task keywords → Tier 3
    if any(w in msg_lower for w in COMPLEX_WORDS):
        return (3, 'system')

    # ── Tier 2: Media ─────────────────────────────────────────────────────────
    if any(w in msg_lower for w in MEDIA_WORDS):
        return (2, 'media')

    # ── Tier 2: Single OS action ─────────────────────────────────────────────
    single_patterns = [
        r'^\s*(open|close|start|kill|launch)\s+\w+',
        r'\b(screenshot|lock\s*pc|lock\s*screen)\b',
        r'\b(brightness|volume)\s+\d+\b',
    ]
    for pat in single_patterns:
        if re.search(pat, msg_lower):
            return (2, 'system')

    # ── Default: Tier 2 misc ──────────────────────────────────────────────────
    return (2, 'misc')
