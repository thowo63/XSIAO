#!/usr/bin/env bash
set -euo pipefail

host="127.0.0.1"
port="8010"
text="Hallo Welt"
stt_file=""

usage() {
  cat <<'EOF'
Usage:
  healthcheck_openai_audio.sh [--host 127.0.0.1] [--port 8010] [--text "Hallo Welt"] [--stt-file /path/to/audio.wav]

Checks:
  - /health returns ok
  - /tts returns a WAV file
  - /stt can transcribe an optional WAV file when provided
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --host)
      host="${2:-}"
      shift 2
      ;;
    --port)
      port="${2:-}"
      shift 2
      ;;
    --text)
      text="${2:-}"
      shift 2
      ;;
    --stt-file)
      stt_file="${2:-}"
      shift 2
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "Unknown argument: $1" >&2
      usage >&2
      exit 1
      ;;
  esac
done

base_url="http://${host}:${port}"

echo "Checking ${base_url}/health"
health="$(curl -fsS "${base_url}/health")"
if [[ "${health}" != ok* ]]; then
  echo "Health check failed: ${health}" >&2
  exit 1
fi
echo "Health OK"

tmp_dir="$(mktemp -d)"
trap 'rm -rf "${tmp_dir}"' EXIT

wav_path="${tmp_dir}/tts.wav"
encoded_text="$(python3 -c 'import sys; from urllib.parse import quote; print(quote(sys.argv[1]))' "${text}")"
tts_url="${base_url}/tts?text=${encoded_text}"

echo "Checking ${base_url}/tts"
curl -fsS "${tts_url}" -o "${wav_path}"

if ! file "${wav_path}" | grep -qi 'WAVE'; then
  echo "TTS check failed: response is not a WAV file" >&2
  exit 1
fi

size="$(stat -c %s "${wav_path}")"
echo "TTS OK (${size} bytes)"

if [[ -n "${stt_file}" ]]; then
  if [[ ! -f "${stt_file}" ]]; then
    echo "STT file not found: ${stt_file}" >&2
    exit 1
  fi

  stt_resp="${tmp_dir}/stt.json"
  echo "Checking ${base_url}/stt"
  curl -fsS \
    -F "audio=@${stt_file};type=audio/wav" \
    -F "language=de" \
    "${base_url}/stt" \
    -o "${stt_resp}"

  python3 - <<'PY' "${stt_resp}"
import json
import pathlib
import sys

path = pathlib.Path(sys.argv[1])
data = json.loads(path.read_text(encoding="utf-8"))
if not data.get("ok"):
    raise SystemExit(f"STT failed: {data}")
text = data.get("text", "")
print(f"STT OK: {text}")
PY
fi
