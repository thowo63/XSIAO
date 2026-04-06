#!/usr/bin/env python3
"""
Minimal HTTP proxy for OpenAI audio endpoints.

Endpoints:
  GET  /health
  GET  /tts?text=...
  POST /tts
  POST /stt

The ESP32 can keep using simple local URLs while the server talks to the
OpenAI API with a standard API key.
"""

from __future__ import annotations

import json
import os
import ssl
import sys
import urllib.error
import urllib.parse
import urllib.request
from http import HTTPStatus
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer


DEFAULT_BASE_URL = "https://api.openai.com/v1"
DEFAULT_TTS_MODEL = "gpt-4o-mini-tts"
DEFAULT_TTS_VOICE = "alloy"
DEFAULT_TTS_FORMAT = "wav"
DEFAULT_STT_MODEL = "gpt-4o-mini-transcribe"
DEFAULT_TIMEOUT = 120


def env(name: str, default: str) -> str:
    value = os.environ.get(name, "").strip()
    return value if value else default


def get_api_key() -> str:
    key = os.environ.get("OPENAI_API_KEY", "").strip()
    if key:
        return key
    key_file = os.environ.get("OPENAI_API_KEY_FILE", "").strip()
    if key_file and os.path.exists(key_file):
        with open(key_file, "r", encoding="utf-8") as f:
            return f.read().strip()
    return ""


def normalized_base_url() -> str:
    return env("OPENAI_BASE_URL", DEFAULT_BASE_URL).rstrip("/")


def openai_headers() -> dict[str, str]:
    api_key = get_api_key()
    if not api_key:
        raise RuntimeError("OPENAI_API_KEY fehlt")
    return {
        "Authorization": f"Bearer {api_key}",
        "User-Agent": "XiaozhiOpenAIProxy/1.0",
    }


def http_json(url: str, payload: dict[str, object], timeout: int) -> bytes:
    data = json.dumps(payload).encode("utf-8")
    headers = openai_headers()
    headers["Content-Type"] = "application/json"
    req = urllib.request.Request(url, data=data, headers=headers, method="POST")
    with urllib.request.urlopen(req, timeout=timeout, context=ssl.create_default_context()) as resp:
        return resp.read()


def http_binary(url: str, payload: dict[str, object], timeout: int) -> tuple[bytes, str]:
    data = json.dumps(payload).encode("utf-8")
    headers = openai_headers()
    headers["Content-Type"] = "application/json"
    req = urllib.request.Request(url, data=data, headers=headers, method="POST")
    with urllib.request.urlopen(req, timeout=timeout, context=ssl.create_default_context()) as resp:
        content_type = resp.headers.get_content_type() or "application/octet-stream"
        return resp.read(), content_type


def build_multipart_body(fields: dict[str, str], file_field_name: str, filename: str, file_bytes: bytes,
                         file_content_type: str = "audio/wav") -> tuple[bytes, str]:
    boundary = "----xiaozhiOpenAIProxyBoundary" + os.urandom(8).hex()
    body = bytearray()

    def add_line(line: str) -> None:
        body.extend(line.encode("utf-8"))
        body.extend(b"\r\n")

    for key, value in fields.items():
        add_line(f"--{boundary}")
        add_line(f'Content-Disposition: form-data; name="{key}"')
        add_line("")
        add_line(value)

    add_line(f"--{boundary}")
    add_line(
        f'Content-Disposition: form-data; name="{file_field_name}"; filename="{filename}"'
    )
    add_line(f"Content-Type: {file_content_type}")
    body.extend(b"\r\n")
    body.extend(file_bytes)
    body.extend(b"\r\n")
    add_line(f"--{boundary}--")

    return bytes(body), boundary


def parse_content_type_parameter(content_type: str, parameter: str) -> str:
    for part in content_type.split(";"):
        piece = part.strip()
        if "=" not in piece:
            continue
        key, value = piece.split("=", 1)
        if key.strip().lower() == parameter.lower():
            return value.strip().strip('"')
    return ""


def parse_multipart_form_data(body: bytes, content_type: str) -> dict[str, tuple[str, bytes, str]]:
    boundary = parse_content_type_parameter(content_type, "boundary")
    if not boundary:
        raise RuntimeError("multipart boundary fehlt")

    delimiter = b"--" + boundary.encode("utf-8")
    chunks = body.split(delimiter)
    fields: dict[str, tuple[str, bytes, str]] = {}

    for chunk in chunks[1:]:
        if chunk in (b"", b"--", b"--\r\n"):
            continue
        if chunk.startswith(b"--"):
            break

        part = chunk
        if part.startswith(b"\r\n"):
            part = part[2:]
        if part.endswith(b"\r\n"):
            part = part[:-2]
        if part.endswith(b"--"):
            part = part[:-2]

        header_blob, sep, content = part.partition(b"\r\n\r\n")
        if not sep:
            continue

        header_lines = header_blob.decode("utf-8", errors="replace").split("\r\n")
        headers: dict[str, str] = {}
        for line in header_lines:
            if ":" not in line:
                continue
            key, value = line.split(":", 1)
            headers[key.strip().lower()] = value.strip()

        disp = headers.get("content-disposition", "")
        name = parse_content_type_parameter(disp, "name")
        filename = parse_content_type_parameter(disp, "filename")
        part_content_type = headers.get("content-type", "text/plain")

        if content.endswith(b"\r\n"):
            content = content[:-2]
        fields[name] = (filename, content, part_content_type)

    return fields


def openai_tts(text: str, voice: str = "") -> tuple[bytes, str]:
    base_url = normalized_base_url()
    url = f"{base_url}/audio/speech"
    response_format = env("OPENAI_TTS_RESPONSE_FORMAT", DEFAULT_TTS_FORMAT)
    payload = {
        "model": env("OPENAI_TTS_MODEL", DEFAULT_TTS_MODEL),
        "response_format": response_format,
        "input": text,
    }

    selected_voice = voice.strip() or env("OPENAI_TTS_VOICE", DEFAULT_TTS_VOICE)
    payload["voice"] = selected_voice

    instructions = os.environ.get("OPENAI_TTS_INSTRUCTIONS", "").strip()
    if instructions:
        payload["instructions"] = instructions

    data, _ = http_binary(url, payload, int(os.environ.get("OPENAI_TIMEOUT", DEFAULT_TIMEOUT)))
    content_type = "audio/wav" if response_format == "wav" else "application/octet-stream"
    return data, content_type


def openai_stt(audio_bytes: bytes, filename: str, language: str = "") -> str:
    base_url = normalized_base_url()
    url = f"{base_url}/audio/transcriptions"

    fields: dict[str, str] = {
        "model": env("OPENAI_STT_MODEL", DEFAULT_STT_MODEL),
    }

    prompt = os.environ.get("OPENAI_STT_PROMPT", "").strip()
    if prompt:
        fields["prompt"] = prompt

    if language:
        fields["language"] = language
    else:
        default_language = os.environ.get("OPENAI_STT_LANGUAGE", "").strip()
        if default_language:
            fields["language"] = default_language

    response_format = os.environ.get("OPENAI_STT_RESPONSE_FORMAT", "").strip()
    if response_format:
        fields["response_format"] = response_format

    body, boundary = build_multipart_body(fields, "file", filename or "record.wav", audio_bytes)
    headers = openai_headers()
    headers["Content-Type"] = f"multipart/form-data; boundary={boundary}"
    headers["Content-Length"] = str(len(body))

    req = urllib.request.Request(url, data=body, headers=headers, method="POST")
    with urllib.request.urlopen(req, timeout=int(os.environ.get("OPENAI_TIMEOUT", DEFAULT_TIMEOUT)), context=ssl.create_default_context()) as resp:
        raw = resp.read()
        content_type = resp.headers.get_content_type() or ""

    if content_type == "application/json":
        payload = json.loads(raw.decode("utf-8"))
        if "text" in payload:
            return str(payload["text"])
        if "transcript" in payload:
            return str(payload["transcript"])
        raise RuntimeError("OpenAI STT JSON ohne text-Feld")

    text = raw.decode("utf-8").strip()
    if text:
        return text

    raise RuntimeError("OpenAI STT Antwort leer")


class OpenAIAudioHandler(BaseHTTPRequestHandler):
    server_version = "OpenAIAudioProxy/1.0"

    def log_message(self, fmt: str, *args: object) -> None:
        sys.stdout.write("%s - - [%s] %s\n" % (self.address_string(), self.log_date_time_string(), fmt % args))

    def send_json(self, status: int, payload: dict[str, object]) -> None:
        data = json.dumps(payload, ensure_ascii=False, indent=2).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def send_text(self, status: int, text: str) -> None:
        data = text.encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "text/plain; charset=utf-8")
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def send_binary(self, status: int, data: bytes, content_type: str) -> None:
        self.send_response(status)
        self.send_header("Content-Type", content_type or "application/octet-stream")
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def read_text_body(self) -> str:
        length = int(self.headers.get("Content-Length", "0") or "0")
        if length <= 0:
            return ""
        raw = self.rfile.read(length)
        content_type = self.headers.get("Content-Type", "")
        if "application/x-www-form-urlencoded" in content_type:
            params = urllib.parse.parse_qs(raw.decode("utf-8"), keep_blank_values=True)
            return params.get("text", [""])[0]
        return raw.decode("utf-8", errors="replace")

    def parse_multipart(self) -> tuple[bytes, str, str]:
        length = int(self.headers.get("Content-Length", "0") or "0")
        if length <= 0:
            raise RuntimeError("Multipart body fehlt")

        body = self.rfile.read(length)
        content_type = self.headers.get("Content-Type", "")
        if "multipart/form-data" not in content_type:
            raise RuntimeError("Ungueltiger STT Content-Type")

        form = parse_multipart_form_data(body, content_type)
        if "audio" not in form:
            raise RuntimeError("audio-Feld fehlt")

        filename, audio_bytes, _audio_content_type = form["audio"]
        filename = filename or "record.wav"
        language = ""
        if "language" in form:
            language = form["language"][1].decode("utf-8", errors="replace")
        return audio_bytes, filename, language

    def handle_health(self) -> None:
        self.send_text(HTTPStatus.OK, "ok")

    def handle_tts(self) -> None:
        try:
            text = ""
            parsed = urllib.parse.urlparse(self.path)
            params = urllib.parse.parse_qs(parsed.query, keep_blank_values=True)
            text = params.get("text", [""])[0].strip()
            voice = params.get("voice", [""])[0].strip()
            if not text and self.command == "POST":
                text = self.read_text_body().strip()
            if not text:
                raise RuntimeError("text fehlt")

            audio_bytes, content_type = openai_tts(text, voice=voice)
            self.send_binary(HTTPStatus.OK, audio_bytes, content_type if content_type != "application/json" else "audio/wav")
        except urllib.error.HTTPError as exc:
            body = exc.read().decode("utf-8", errors="replace")
            self.send_json(exc.code, {"ok": False, "error": body or exc.reason})
        except Exception as exc:
            self.send_json(HTTPStatus.INTERNAL_SERVER_ERROR, {"ok": False, "error": str(exc)})

    def handle_stt(self) -> None:
        try:
            audio_bytes, filename, language = self.parse_multipart()
            text = openai_stt(audio_bytes, filename, language=language)
            self.send_json(HTTPStatus.OK, {"ok": True, "text": text})
        except urllib.error.HTTPError as exc:
            body = exc.read().decode("utf-8", errors="replace")
            self.send_json(exc.code, {"ok": False, "error": body or exc.reason})
        except Exception as exc:
            self.send_json(HTTPStatus.INTERNAL_SERVER_ERROR, {"ok": False, "error": str(exc)})

    def do_GET(self) -> None:  # noqa: N802
        parsed = urllib.parse.urlparse(self.path)
        if parsed.path == "/health":
            self.handle_health()
            return
        if parsed.path == "/tts":
            self.handle_tts()
            return
        self.send_json(HTTPStatus.NOT_FOUND, {"ok": False, "error": "not found"})

    def do_POST(self) -> None:  # noqa: N802
        parsed = urllib.parse.urlparse(self.path)
        if parsed.path == "/tts":
            self.handle_tts()
            return
        if parsed.path == "/stt":
            self.handle_stt()
            return
        self.send_json(HTTPStatus.NOT_FOUND, {"ok": False, "error": "not found"})


def main() -> int:
    import argparse

    parser = argparse.ArgumentParser(description="OpenAI audio proxy for TTS and STT")
    parser.add_argument("--host", default=os.environ.get("OPENAI_AUDIO_HOST", "0.0.0.0"))
    parser.add_argument("--port", type=int, default=int(os.environ.get("OPENAI_AUDIO_PORT", "8010")))
    args = parser.parse_args()

    if not get_api_key():
        print("OPENAI_API_KEY fehlt. Setze es in der Umgebung oder in /etc/default/openai-audio.", file=sys.stderr)
        return 1

    server = ThreadingHTTPServer((args.host, args.port), OpenAIAudioHandler)
    print(f"Listening on http://{args.host}:{args.port}")
    print("Health check: /health")
    print("TTS endpoint: /tts?text=...")
    print("STT endpoint: /stt")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
