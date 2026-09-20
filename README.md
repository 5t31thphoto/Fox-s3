# Fox Voice Companion (PlatformIO + Arduino)

Builds on *GitHub Actions*, flashes from the **web flasher** — same pipeline you
already use. Replaces the ESP-IDF + arduino-as-component hybrid that never ran
setup() (the week-long black screen).

## Pipeline (all on GitHub)
Push to `main` → `.github/workflows/build-and-flash.yml`:
1. `pio run -e atoms3r` builds the firmware.
2. `tools/build_ir.py` + `tools/train_brain.py` generate the IR + brain data.
3. Assembles the esp-web-tools **manifests** (offsets read from `partitions.csv`
   so they can't drift).
4. Deploys the **web flasher** + all bins to **GitHub Pages**.

Then flash from your browser at the Pages URL (pick Chatterbox/Critter → Flash →
send config over USB), exactly as before.

Settings → Pages → Source must be **GitHub Actions**.

## Local flash (optional, for quick hardware checks)
```
pio run -e atoms3r -t upload
pio device monitor       # 115200; boot prints FOX: board=… displays=… LCD=WxH
```

## PART 1 (this build) — the full product surface minus speech recognition
Display (M5Unified autodetects the AtomS3R incl. the newer **ST7735** panel),
EchoBase audio, animated face + own-voice lip-sync (mic drives the mouth only in
puppet mode), mood/personality/Markov chatter, **conversation engine**, games
(wormhole, catch, Bayesian 20-questions, reaction, paw), Flipper tools (BLE/WiFi
radar, sniffer, pwnagotchi hunt, probes), demoscene toys, fox encounters +
roguelike maze, IR, on-device tiny-LLM, memory, flick-menu.

Octal PSRAM enabled (`memory_type = qio_opi`).

## PART 2 (drop-in) — offline MultiNet speech-to-intent
The `model` partition (2.75MB) is already reserved and empty; app/brain/IR/
journal offsets never move, so Part 2 only flashes `srmodels.bin`. The command
grammar, dispatch, and the conversation engine (`fox_converse.inc`) are already
wired for it — see `PART2_INTEGRATION.md` and the `PART 2 PLUGS IN HERE` marker
in `process_utterance()`.
