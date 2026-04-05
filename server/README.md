# OpenAI Audio Proxy

This project includes a minimal HTTP proxy for OpenAI audio endpoints:

- `GET /tts?text=...` returns WAV audio from OpenAI TTS
- `POST /stt` uploads WAV audio and returns JSON `{ "ok": true, "text": "..." }`
- `GET /health` returns `ok`

Example:

```bash
python server/openai_audio_server.py --host 0.0.0.0 --port 8010
```

Recommended defaults:

- `OPENAI_BASE_URL=https://api.openai.com/v1`
- `OPENAI_TTS_MODEL=gpt-4o-mini-tts`
- `OPENAI_TTS_VOICE=alloy`
- `OPENAI_TTS_RESPONSE_FORMAT=wav`
- `OPENAI_STT_MODEL=gpt-4o-mini-transcribe`
- `OPENAI_STT_RESPONSE_FORMAT=text`

The proxy expects `OPENAI_API_KEY` in the environment or in `/etc/default/openai-audio`.
Use a real OpenAI API key from platform.openai.com, not a ChatGPT web session token.

The ESP32 firmware can call:

```text
http://<server-ip>:8010/tts?text=Hallo%20Welt
http://<server-ip>:8010/stt
```

Health check:

```bash
curl http://127.0.0.1:8010/health
```

Install as a service:

```bash
bash server/install_openai_audio.sh \
  --repo-dir /opt/xiaozhi \
  --venv-python /opt/xiaozhi/.venv/bin/python
```

Healthcheck script:

```bash
bash server/healthcheck_openai_audio.sh --host 127.0.0.1 --port 8010 --text "Hallo Welt"
```

The healthcheck helper uses `curl`, `file`, and `python3`.

## Qwen3-TTS HTTP Adapter

This project also includes a minimal HTTP wrapper for Qwen3-TTS:

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

## systemd

You can run the adapter as a service with the template in
[`qwen3-tts.service`](./qwen3-tts.service).

Typical install steps:

```bash
sudo cp server/qwen3-tts.service /etc/systemd/system/qwen3-tts.service
sudo systemctl daemon-reload
sudo systemctl enable --now qwen3-tts.service
sudo systemctl status qwen3-tts.service
```

Or use the installer script:

```bash
bash server/install_qwen3_tts.sh \
  --repo-dir /opt/xiaozhi \
  --venv-python /opt/qwen3-tts/.venv/bin/python
```

If you are already root, the script will run without `sudo`. If `sudo` exists,
it will use it automatically when needed.

Health check:

```bash
curl http://127.0.0.1:8001/health
```

Logs:

```bash
journalctl -u qwen3-tts -f
```

Healthcheck script:

```bash
bash server/healthcheck_qwen3_tts.sh --host 127.0.0.1 --port 8001 --text "Hallo Welt"
```

The healthcheck helper uses `curl`, `file`, and `python3`.

Before enabling the unit, adjust these paths in the service file:

- `/opt/xiaozhi`
- `/opt/qwen3-tts/.venv/bin/python`

If your repo or venv lives elsewhere, replace them with your actual paths.

The installer writes an optional defaults file at `/etc/default/qwen3-tts`.
You can edit that file later to change model, task, speaker, or language without
touching the service unit.
