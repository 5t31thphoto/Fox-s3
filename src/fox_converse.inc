// fox_converse.inc — the conversational engine. Included by fox_main.cpp.
//
// DESIGN (this is the point of the whole project): we can't run full offline
// speech-to-text on this chip. So we make talking to the fox feel real with a
// clever system around a SIMPLE recognizer:
//
//   * You don't have to be understood word-for-word to FEEL understood. If the
//     recognizer catches even ONE topical keyword, the fox answers that topic
//     with confidence and it lands. If it catches nothing but you clearly spoke,
//     it answers the SHAPE of what you said — length, energy, mood, and whether
//     it just asked you a question — with reflection, warmth, and its own
//     follow-up questions. Either way it makes sense and keeps the turn going.
//   * The tiny on-device LLM (fox_llm) rephrases the chosen line so responses
//     don't feel canned — a real little brain on top of the heuristics.
//   * Mood + needs + a rolling memory give it context and continuity.
//
// The SAME engine serves Part 1 (VAD only → heard == "") and Part 2 (MultiNet
// passes the caught keywords as `heard`). Part 2 is literally: recognize words,
// call fox_respond(words, voiced_ms). Nothing else changes.
#pragma once

// from other units
void   speak(const String& fact);
String llm_flavour(const String& fact, FoxMood mood, const FoxConfig& cfg);
String fox_reflect(const String& user_lower);

// ---- conversation state (continuity / context) ----------------------------
struct Convo {
    bool     fox_asked   = false;   // did the fox just ask you something?
    String   last_topic;            // last topic we talked about
    uint32_t last_turn   = 0;
    int      turns       = 0;
    int      no_topic_streak = 0;   // consecutive turns with nothing recognized
};
static Convo convo;

// ---- simple VAD: how long (ms) did the user actually speak in this buffer? --
uint32_t fox_voiced_ms(const int16_t* audio, size_t n) {
    if (!audio || !n) return 0;
    const size_t FR = 256;                 // ~16ms frames @16k
    size_t voiced_frames = 0, total = 0;
    for (size_t p = 0; p + FR <= n; p += FR) {
        long amp = 0;
        for (size_t i = 0; i < FR; ++i) { int v = audio[p + i]; amp += v < 0 ? -v : v; }
        if (amp / (long)FR > 600) voiced_frames++;
        total++;
    }
    if (!total) return 0;
    return (uint32_t)((voiced_frames * (uint64_t)FR * 1000) / SAMPLE_RATE);
}

// ---- topic banks: keyword -> warm, on-topic replies ------------------------
// Part 2's MultiNet grammar will surface these keywords. Keep the phrasings
// natural and varied so a single caught word feels like real understanding.
struct Topic { const char* key; const char* replies[4]; };
static const Topic TOPICS[] = {
    {"name",    {"i'm your fox! did you forget already? hehe", "i'm right here~",
                 "you can call me whatever feels right", "that's me!"}},
    {"hello",   {"hi hi! i'm so glad you're here", "hello you~", "oh! hey!",
                 "*ears perk up* hi!"}},
    {"how",     {"i feel pretty good! how about you?", "content and cozy~",
                 "a little fidgety, honestly. you?", "better now that you're talking to me"}},
    {"love",    {"aw, i love you too! *tail wag*", "you're my favorite person",
                 "that makes my whole tail wag~", "hehe, i feel warm now"}},
    {"sad",     {"aw... want to just sit together a while?", "i'm here. tell me?",
                 "that's okay to feel. i've got you", "*leans against you*"}},
    {"happy",   {"yay! that makes me happy too!", "your good mood is contagious~",
                 "*bounces* i love when you're happy", "hehe, me too!"}},
    {"tired",   {"mm, me too. we can rest a bit", "*yawn* cozy time?",
                 "close your eyes, i'll keep watch", "sleepy foxes unite"}},
    {"play",    {"yes yes! let's play something!", "ooh, a game? open the menu~",
                 "i love games! flick to pick one", "*wiggles* play play play!"}},
    {"weather", {"i can peek at the weather if we're online~", "hope it's nice out!",
                 "want me to check the sky?", "clouds or sun today?"}},
    {"tv",      {"want me to try turning the tv off? i can learn it~",
                 "i've got some ir tricks for that", "point me at the tv!",
                 "*beep boop* remote mode"}},
    {"joke",    {"why did the fox cross the road? to boop you! ...i'm working on it",
                 "hehe, i'm more cute than funny", "knock knock~ ...who's there? me!",
                 "i only know fox jokes, they're pretty bad"}},
    {"food",    {"snacks?! did someone say snacks?", "i could go for a berry~",
                 "*sniffs hopefully*", "feed me? hehe"}},
    {"good",    {"aw, you're good too~", "*happy fox noises*", "that means a lot",
                 "you always know what to say"}},
    {"sorry",   {"it's okay! i'm not upset~", "no worries at all", "we're good, promise",
                 "*nuzzles* all forgiven"}},
    {"bye",     {"aw, bye for now~ come back soon!", "i'll be right here",
                 "*waves paw* see you!", "don't be gone too long!"}},
};
static const int NTOPIC = sizeof(TOPICS) / sizeof(TOPICS[0]);

// generic reactions to "you spoke" when no topic was caught
static const char* ACK_SHORT[] = { "mm?", "oh?", "yeah?", "hehe", "go on~", "really?" };
static const char* ACK_LONG[]  = { "ooh, tell me more!", "wow, and then?", "i'm listening~",
                                    "that's a lot to say!", "*ears forward* really?", "mmhm, i hear you" };
static const char* CURIOUS[]   = { "what do you mean?", "tell me more?", "and how'd that feel?",
                                   "then what happened?", "why's that?" };
static const char* ASK[]       = { "how are you feeling?", "what should we do?",
                                   "did something happen today?", "wanna play a game?",
                                   "what's on your mind?", "are you happy right now?" };
static const char* WARM[]      = { "i like talking with you", "*tail wag*", "this is nice",
                                   "you're good company", "i feel cozy" };

template <size_t N> static const char* one(const char* const (&a)[N]) { return a[esp_random() % N]; }

// Speak a line, but let the tiny on-device LLM rephrase it first so it feels
// fresh rather than canned. (llm_flavour returns "" if the model isn't loaded
// or if it would drift off — then we speak the original line.)
static void converse_say(const String& line) {
    FoxMood m = fox_mood(needs);
    String flav = llm_flavour(line, m, cfg);
    speak(flav.length() ? flav : line);
}

// Scan recognized text for the strongest topic keyword; -1 if none.
static int match_topic(const String& lower) {
    for (int i = 0; i < NTOPIC; ++i)
        if (lower.indexOf(TOPICS[i].key) >= 0) return i;
    return -1;
}

// ---- the main entry --------------------------------------------------------
// heard      : recognized keywords, space-joined ("" in Part 1 / VAD-only)
// voiced_ms  : how long the user spoke
void fox_respond(const String& heard, uint32_t voiced_ms) {
    needs_interact(needs, false);
    convo.turns++;
    convo.last_turn = millis();
    FoxMood m = fox_mood(needs);
    String lower = heard; lower.toLowerCase();

    // 1) Did we actually catch a topic? Then answer it with confidence — this is
    //    what makes even one recognized word feel like real understanding.
    int t = (heard.length() ? match_topic(lower) : -1);
    if (t >= 0) {
        convo.no_topic_streak = 0;
        convo.last_topic = TOPICS[t].key;
        converse_say(TOPICS[t].replies[esp_random() % 4]);
        if (cfg.persistence) mem_append("you", heard);
        // sometimes keep the turn going
        if (esp_random() % 100 < 40) { delay(220); converse_say(one(ASK)); convo.fox_asked = true; }
        return;
    }

    // 2) No topic caught. If the fox just asked something, treat this utterance
    //    as your ANSWER and acknowledge it warmly (reflection), so it feels heard.
    convo.no_topic_streak++;
    if (convo.fox_asked) {
        convo.fox_asked = false;
        if (m == MOOD_GRUMPY) { converse_say("hmph. okay."); return; }
        converse_say(voiced_ms > 1400 ? one(ACK_LONG) : one(ACK_SHORT));
        if (esp_random() % 100 < 55) { delay(220); converse_say(one(WARM)); }
        return;
    }

    // 3) Otherwise react to the SHAPE of what you said + mood, and usually turn
    //    it back into a question so it stays a two-way conversation.
    if (m == MOOD_GRUMPY)        converse_say(one(ACK_SHORT));
    else if (voiced_ms > 1600)   converse_say(one(ACK_LONG));
    else if (voiced_ms > 500)    converse_say(one(CURIOUS));
    else                         converse_say(one(ACK_SHORT));

    // if you keep talking and we keep catching nothing, the fox leans on charm:
    // asks a simple question it CAN act on, nudging toward things it understands.
    if (convo.no_topic_streak >= 2 || esp_random() % 100 < 55) {
        delay(220);
        converse_say(one(ASK));
        convo.fox_asked = true;
        convo.no_topic_streak = 0;
    }
}
