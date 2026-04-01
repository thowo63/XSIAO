#!/usr/bin/env python3
"""
Minimal HTTP adapter for Qwen3-TTS.

Run this inside the Python venv where `qwen-tts` is installed:

  python server/qwen_tts_server.py --host 0.0.0.0 --port 8001

The firmware can then call:

  /tts?text=Hallo%20Welt

and receive a WAV file.
"""

from __future__ import annotations

import argparse
import io
import os
import threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from typing import Any
from urllib.parse import parse_qs, urlparse

import soundfile as sf
import torch
from qwen_tts import Qwen3TTSModel


MODEL_LOCK = threading.Lock()


def _dtype_from_name(name: str) -> torch.dtype:
    mapping = {
        "float16": torch.float16,
        "float32": torch.float32,
        "bfloat16": torch.bfloat16,
    }
    try:
        return mapping[name.lower()]
    except KeyError as exc:
        raise ValueError(f"Unsupported dtype: {name}") from exc


def _is_missing(value: str | None) -> bool:
    return value is None or value.strip() == ""


def _generate_audio(model: Any, task: str, text: str, language: str | None, speaker: str | None, instruct: str | None):
    if task == "design":
        kwargs: dict[str, Any] = {"text": text}
        if not _is_missing(language):
            kwargs["language"] = language
        if not _is_missing(instruct):
            kwargs["instruct"] = instruct
        return model.generate_voice_design(**kwargs)

    kwargs = {"text": text}
    if not _is_missing(language):
        kwargs["language"] = language
    if not _is_missing(speaker):
        kwargs["speaker"] = speaker
    if not _is_missing(instruct):
        kwargs["instruct"] = instruct
    return model.generate_custom_voice(**kwargs)


def build_model(args: argparse.Namespace):
    dtype = _dtype_from_name(args.dtype)
    device_map = args.device_map
    attn_implementation = args.attn_implementation

    if device_map == "cpu":
        if dtype in (torch.float16, torch.bfloat16):
            dtype = torch.float32
        attn_implementation = "eager"

    print(f"Loading Qwen3-TTS model: {args.model_id}")
    print(f"Task: {args.task}")
    print(f"Device map: {device_map}")
    print(f"Dtype: {dtype}")

    return Qwen3TTSModel.from_pretrained(
        args.model_id,
        device_map=device_map,
        dtype=dtype,
        attn_implementation=attn_implementation,
    )


class QwenTTSHandler(BaseHTTPRequestHandler):
    server_version = "Qwen3TTSAdapter/1.0"

    def log_message(self, format: str, *args: Any) -> None:
        print("%s - - [%s] %s" % (self.client_address[0], self.log_date_time_string(), format % args))

    def _send_json_error(self, code: int, message: str) -> None:
        payload = (message + "\n").encode("utf-8")
        self.send_response(code)
        self.send_header("Content-Type", "text/plain; charset=utf-8")
        self.send_header("Content-Length", str(len(payload)))
        self.end_headers()
        self.wfile.write(payload)

    def do_GET(self) -> None:
        parsed = urlparse(self.path)
        if parsed.path == "/health":
            payload = b"ok\n"
            self.send_response(200)
            self.send_header("Content-Type", "text/plain; charset=utf-8")
            self.send_header("Content-Length", str(len(payload)))
            self.end_headers()
            self.wfile.write(payload)
            return

        if parsed.path != "/tts":
            self._send_json_error(404, "Not found")
            return

        params = parse_qs(parsed.query)
        text = params.get("text", [""])[0].strip()
        if not text:
            self._send_json_error(400, "Missing text parameter")
            return

        language = params.get("language", [self.server.default_language])[0]  # type: ignore[attr-defined]
        speaker = params.get("speaker", [self.server.default_speaker])[0]  # type: ignore[attr-defined]
        instruct = params.get("instruct", [self.server.default_instruct])[0]  # type: ignore[attr-defined]

        try:
            with MODEL_LOCK:
                wavs, sr = _generate_audio(
                    self.server.model,  # type: ignore[attr-defined]
                    self.server.task,  # type: ignore[attr-defined]
                    text,
                    language,
                    speaker,
                    instruct,
                )

            buffer = io.BytesIO()
            sf.write(buffer, wavs[0], sr, format="WAV", subtype="PCM_16")
            body = buffer.getvalue()

            self.send_response(200)
            self.send_header("Content-Type", "audio/wav")
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Cache-Control", "no-store")
            self.end_headers()
            self.wfile.write(body)
        except Exception as exc:  # pragma: no cover - surfaced in logs
            self._send_json_error(500, f"TTS generation failed: {exc}")

    def do_POST(self) -> None:
        self.do_GET()


class QwenTTSHTTPServer(ThreadingHTTPServer):
    model: Any
    task: str
    default_language: str | None
    default_speaker: str | None
    default_instruct: str | None


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Minimal HTTP adapter for Qwen3-TTS")
    parser.add_argument("--host", default="0.0.0.0")
    parser.add_argument("--port", type=int, default=8001)
    parser.add_argument(
        "--model-id",
        default=os.environ.get("QWEN_TTS_MODEL_ID", "Qwen/Qwen3-TTS-12Hz-1.7B-CustomVoice"),
    )
    parser.add_argument(
        "--task",
        choices=("custom", "design"),
        default=os.environ.get("QWEN_TTS_TASK", "custom"),
    )
    parser.add_argument(
        "--device-map",
        default=os.environ.get("QWEN_TTS_DEVICE_MAP", "cuda:0" if torch.cuda.is_available() else "cpu"),
    )
    parser.add_argument(
        "--dtype",
        default=os.environ.get("QWEN_TTS_DTYPE", "bfloat16"),
    )
    parser.add_argument(
        "--attn-implementation",
        default=os.environ.get("QWEN_TTS_ATTN_IMPL", "flash_attention_2"),
    )
    parser.add_argument(
        "--default-language",
        default=os.environ.get("QWEN_TTS_LANGUAGE", ""),
    )
    parser.add_argument(
        "--default-speaker",
        default=os.environ.get("QWEN_TTS_SPEAKER", "Ryan"),
    )
    parser.add_argument(
        "--default-instruct",
        default=os.environ.get("QWEN_TTS_INSTRUCT", ""),
    )
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    model = build_model(args)

    server = QwenTTSHTTPServer((args.host, args.port), QwenTTSHandler)
    server.model = model
    server.task = args.task
    server.default_language = args.default_language or None
    server.default_speaker = args.default_speaker or None
    server.default_instruct = args.default_instruct or None

    print(f"Listening on http://{args.host}:{args.port}")
    print("Health check: /health")
    print("TTS endpoint: /tts?text=...")
    server.serve_forever()


if __name__ == "__main__":
    main()
