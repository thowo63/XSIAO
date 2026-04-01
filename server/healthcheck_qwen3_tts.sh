#!/usr/bin/env bash
set -euo pipefail

host="127.0.0.1"
port="8001"
text="Hallo Welt"

usage() {
  cat <<'EOF'
Usage:
  healthcheck_qwen3_tts.sh [--host 127.0.0.1] [--port 8001] [--text "Hallo Welt"]

Checks:
  - /health returns ok
  - /tts returns a WAV file
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
