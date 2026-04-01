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

Before enabling the unit, adjust these paths in the service file:

- `/opt/xiaozhi`
- `/opt/qwen3-tts/.venv/bin/python`

If your repo or venv lives elsewhere, replace them with your actual paths.

The installer writes an optional defaults file at `/etc/default/qwen3-tts`.
You can edit that file later to change model, task, speaker, or language without
touching the service unit.
