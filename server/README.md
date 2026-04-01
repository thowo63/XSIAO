# Qwen3-TTS HTTP Adapter

This project includes a minimal HTTP wrapper for Qwen3-TTS:

```bash
python server/qwen_tts_server.py --host 0.0.0.0 --port 8001
```

Recommended defaults:

- `--task custom`
- `--model-id Qwen/Qwen3-TTS-12Hz-1.7B-CustomVoice`
- `--default-speaker Ryan`

The ESP32 firmware can call:

```text
http://<server-ip>:8001/tts?text=Hallo%20Welt
```

Optional query parameters:

- `language`
- `speaker`
- `instruct`

Example:

```text
/tts?text=Guten%20Tag&language=German&speaker=Ryan
```
