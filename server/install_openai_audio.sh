#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'EOF'
Usage:
  install_openai_audio.sh --repo-dir /path/to/xiaozhi --venv-python /path/to/python

Required:
  --repo-dir         Path to the checked-out xiaozhi repository
  --venv-python      Python binary used to run the proxy

Optional flags:
  --unit-name openai-audio
  --host 0.0.0.0
  --port 8010
  --api-key <key>
  --base-url https://api.openai.com/v1
  --tts-model gpt-4o-mini-tts
  --tts-voice alloy
  --tts-response-format wav
  --stt-model gpt-4o-mini-transcribe
  --stt-language de
  --stt-response-format text
  --stt-prompt ""
  --timeout 120
  --skip-defaults-file

The script installs a systemd service and enables it.
EOF
}

repo_dir=""
venv_python=""
unit_name="openai-audio"
host="0.0.0.0"
port="8010"
api_key=""
base_url="https://api.openai.com/v1"
tts_model="gpt-4o-mini-tts"
tts_voice="alloy"
tts_response_format="wav"
stt_model="gpt-4o-mini-transcribe"
stt_language="de"
stt_response_format="text"
stt_prompt=""
timeout="120"
skip_defaults_file="false"

run_as_root() {
  if [[ "${EUID}" -eq 0 ]]; then
    "$@"
  elif command -v sudo >/dev/null 2>&1; then
    sudo "$@"
  else
    echo "This script needs root privileges, but sudo is not installed." >&2
    exit 1
  fi
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --repo-dir)
      repo_dir="${2:-}"
      shift 2
      ;;
    --venv-python)
      venv_python="${2:-}"
      shift 2
      ;;
    --unit-name)
      unit_name="${2:-}"
      shift 2
      ;;
    --host)
      host="${2:-}"
      shift 2
      ;;
    --port)
      port="${2:-}"
      shift 2
      ;;
    --api-key)
      api_key="${2:-}"
      shift 2
      ;;
    --base-url)
      base_url="${2:-}"
      shift 2
      ;;
    --tts-model)
      tts_model="${2:-}"
      shift 2
      ;;
    --tts-voice)
      tts_voice="${2:-}"
      shift 2
      ;;
    --tts-response-format)
      tts_response_format="${2:-}"
      shift 2
      ;;
    --stt-model)
      stt_model="${2:-}"
      shift 2
      ;;
    --stt-language)
      stt_language="${2:-}"
      shift 2
      ;;
    --stt-response-format)
      stt_response_format="${2:-}"
      shift 2
      ;;
    --stt-prompt)
      stt_prompt="${2:-}"
      shift 2
      ;;
    --timeout)
      timeout="${2:-}"
      shift 2
      ;;
    --skip-defaults-file)
      skip_defaults_file="true"
      shift
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

if [[ -z "$repo_dir" || -z "$venv_python" ]]; then
  usage >&2
  exit 1
fi

if [[ ! -x "$venv_python" ]]; then
  echo "Venv python not found or not executable: $venv_python" >&2
  exit 1
fi

if [[ ! -f "$repo_dir/server/openai_audio_server.py" ]]; then
  echo "Proxy not found: $repo_dir/server/openai_audio_server.py" >&2
  exit 1
fi

if [[ -z "$api_key" ]]; then
  if [[ -n "${OPENAI_API_KEY:-}" ]]; then
    api_key="${OPENAI_API_KEY}"
  else
    read -r -s -p "OpenAI API key: " api_key
    echo
  fi
fi

if [[ -z "$api_key" ]]; then
  echo "OpenAI API key is required." >&2
  exit 1
fi

service_path="/etc/systemd/system/${unit_name}.service"
defaults_path="/etc/default/${unit_name}"

run_as_root tee "$service_path" >/dev/null <<EOF
[Unit]
Description=OpenAI Audio HTTP proxy
After=network-online.target
Wants=network-online.target

[Service]
Type=simple
WorkingDirectory=${repo_dir}
Environment=PYTHONUNBUFFERED=1
EnvironmentFile=-${defaults_path}
ExecStart=${venv_python} ${repo_dir}/server/openai_audio_server.py --host ${host} --port ${port}
Restart=on-failure
RestartSec=5

[Install]
WantedBy=multi-user.target
EOF

if [[ "$skip_defaults_file" != "true" ]]; then
  run_as_root tee "$defaults_path" >/dev/null <<EOF
OPENAI_API_KEY=${api_key}
OPENAI_BASE_URL=${base_url}
OPENAI_TTS_MODEL=${tts_model}
OPENAI_TTS_VOICE=${tts_voice}
OPENAI_TTS_RESPONSE_FORMAT=${tts_response_format}
OPENAI_STT_MODEL=${stt_model}
OPENAI_STT_LANGUAGE=${stt_language}
OPENAI_STT_RESPONSE_FORMAT=${stt_response_format}
OPENAI_STT_PROMPT=${stt_prompt}
OPENAI_TIMEOUT=${timeout}
EOF
fi

run_as_root systemctl daemon-reload
run_as_root systemctl enable --now "${unit_name}.service"

echo "Installed and started ${unit_name}.service"
echo "Check status: sudo systemctl status ${unit_name}.service"
echo "Check health: curl http://127.0.0.1:${port}/health"
