#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'EOF'
Usage:
  install_qwen3_tts.sh --repo-dir /path/to/xiaozhi --venv-python /path/to/qwen/.venv/bin/python

Optional flags:
  --unit-name qwen3-tts
  --port 8001
  --host 0.0.0.0
  --model-id Qwen/Qwen3-TTS-12Hz-1.7B-CustomVoice
  --task custom|design
  --default-speaker Ryan
  --default-language German
  --default-instruct ""
  --skip-defaults-file

The script installs a systemd service and enables it.
EOF
}

repo_dir=""
venv_python=""
unit_name="qwen3-tts"
host="0.0.0.0"
port="8001"
model_id="Qwen/Qwen3-TTS-12Hz-1.7B-CustomVoice"
task="custom"
default_speaker="Ryan"
default_language=""
default_instruct=""
skip_defaults_file="false"

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
    --model-id)
      model_id="${2:-}"
      shift 2
      ;;
    --task)
      task="${2:-}"
      shift 2
      ;;
    --default-speaker)
      default_speaker="${2:-}"
      shift 2
      ;;
    --default-language)
      default_language="${2:-}"
      shift 2
      ;;
    --default-instruct)
      default_instruct="${2:-}"
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

if [[ ! -f "$repo_dir/server/qwen_tts_server.py" ]]; then
  echo "Adapter not found: $repo_dir/server/qwen_tts_server.py" >&2
  exit 1
fi

service_path="/etc/systemd/system/${unit_name}.service"
defaults_path="/etc/default/${unit_name}"

sudo tee "$service_path" >/dev/null <<EOF
[Unit]
Description=Qwen3-TTS HTTP adapter
After=network-online.target
Wants=network-online.target

[Service]
Type=simple
WorkingDirectory=${repo_dir}
Environment=PYTHONUNBUFFERED=1
EnvironmentFile=-${defaults_path}
ExecStart=${venv_python} ${repo_dir}/server/qwen_tts_server.py --host ${host} --port ${port}
Restart=on-failure
RestartSec=5

[Install]
WantedBy=multi-user.target
EOF

if [[ "$skip_defaults_file" != "true" ]]; then
  sudo tee "$defaults_path" >/dev/null <<EOF
QWEN_TTS_MODEL_ID=${model_id}
QWEN_TTS_TASK=${task}
QWEN_TTS_SPEAKER=${default_speaker}
QWEN_TTS_LANGUAGE=${default_language}
QWEN_TTS_INSTRUCT=${default_instruct}
EOF
fi

sudo systemctl daemon-reload
sudo systemctl enable --now "${unit_name}.service"

echo "Installed and started ${unit_name}.service"
echo "Check status: sudo systemctl status ${unit_name}.service"
echo "Check health: curl http://127.0.0.1:${port}/health"
