# Part 2 — plugging in offline MultiNet speech (drop-in)

Part 1 was built so this is *additive only*. Nothing in Part 1 has to move.

## Already done for you in Part 1
- **Partition table is final.** `model` (2.75MB) is reserved and empty; `factory`,
  `foxbrain`, `foxdata`, `foxfs` never shift. Part 2 only writes `model`.
- **Grammar + dispatch are wired.** `COMMANDS[]` (28 intents) and `do_action()` /
  `menu_dispatch()` / `launch()` already run every game/tool/report. A recognized
  command id just calls `do_action(COMMANDS[i].action)`.
- **Conversation engine is live.** `fox_respond(heard, voiced_ms)` in
  `fox_converse.inc` already turns recognized *keywords* into on-topic, mood-aware,
  turn-taking replies (and copes with `heard == ""`). It's the thing that makes a
  single caught word feel like understanding.
- **The hook is marked.** See the `PART 2 PLUGS IN HERE` block in
  `process_utterance()` — it's the only place you edit.

## The three steps
1. **Add esp-sr.** In `platformio.ini`, esp-sr ships inside arduino-esp32; add its
   include dir + link, or use the ESP-IDF component if you switch the env to the
   `arduino,espidf` framework. Enable MultiNet7 English + AFE (no wakenet).
2. **Build + flash the model.** Generate `srmodels.bin` and flash it to the
   `model` partition at `0x400000` (already reserved). The web-flasher manifest
   just gains one part: `{ "path": "srmodels.bin", "offset": 4194304 }`.
3. **Write `fox_speech.inc`** with `init_speech()` (AFE + MultiNet create, load the
   `COMMANDS[]` grammar) and `int recognize_offline(int16_t*, size_t, String& heard)`
   (feed→fetch→detect; return the command id and/or fill `heard` with the caught
   keyword strings). Call `init_speech()` where setup() says "PART 2 initialises …",
   and uncomment the marked block in `process_utterance()`.

## Making it feel conversational (the important part)
`recognize_offline` should return BOTH: the exact command id when one matches
strongly, and — always — the individual keywords it caught (even low-confidence)
in `heard`. That lets `fox_respond` answer the topic when it recognizes a word,
and fall back to shape/mood/reflection when it doesn't — so "understood most of
it" still makes sense. Broaden `COMMANDS[]` with many natural phrasings per intent
(MultiNet7 handles ~200 phrases) and add topic keywords to `TOPICS[]` in
`fox_converse.inc`; the two together are what give it the Furby-with-a-brain feel.
