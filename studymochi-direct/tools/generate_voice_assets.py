#!/usr/bin/env python3
"""Generate the six StudyMochi Bangla announcements with Gemini TTS.

The API key is read only from GEMINI_API_KEY. It is never accepted as a
command-line argument and is never written to a file or printed.
"""

import argparse
import base64
import json
import os
from pathlib import Path
import re
import time
import urllib.error
import urllib.request
import wave


ENDPOINT = "https://generativelanguage.googleapis.com/v1beta/interactions"
MODEL = "gemini-3.1-flash-tts-preview"
VOICE = "Leda"
SAMPLE_RATE = 24000

SCRIPTS = {
    "0003_pomo_start.wav": "পড়ার সময় শুরু! মন দিয়ে পড়ো, আমি তোমার সাথেই আছি।",
    "0004_break_start.wav": "এবার একটু বিরতি। পানি খাও, আর চোখ দুটোকে বিশ্রাম দাও।",
    "0005_session_done.wav": "ইয়েই! স্টাডি সেশন শেষ। তুমি দারুণ করেছ!",
    "0006_clock_mode.wav": "ঘড়ি মোড চালু।",
    "0007_timer_mode.wav": "টাইমার মোড চালু।",
    "0008_ai_mode.wav": "এআই মোড চালু। বলো, কী জানতে চাও?",
}


def audio_data_from_response(response):
    direct = response.get("output_audio") or response.get("outputAudio")
    if isinstance(direct, dict) and direct.get("data"):
        return direct["data"]

    # Keep the generator tolerant of envelope changes in preview APIs.
    stack = [response]
    while stack:
        value = stack.pop()
        if isinstance(value, dict):
            if value.get("type") == "audio" and value.get("data"):
                return value["data"]
            stack.extend(value.values())
        elif isinstance(value, list):
            stack.extend(value)
    raise RuntimeError("Gemini response did not contain an audio block")


def request_pcm(api_key, transcript):
    prompt = f"""# AUDIO PROFILE
A cute young-adult Bangladeshi woman with a warm, gentle, slightly playful voice.

# DIRECTOR'S NOTES
Speak clear Bangladeshi Bangla at a natural pace with a soft vocal smile.
Sound friendly and compact, never babyish. Use no music, reverb, sound effects,
introductory phrase, or extra words. Read the transcript exactly once.

# TRANSCRIPT
{transcript}"""
    payload = {
        "model": MODEL,
        "input": prompt,
        "response_format": {"type": "audio"},
        "generation_config": {"speech_config": [{"voice": VOICE}]},
    }
    encoded_payload = json.dumps(payload, ensure_ascii=False).encode("utf-8")
    for attempt in range(4):
        request = urllib.request.Request(
            ENDPOINT,
            data=encoded_payload,
            method="POST",
            headers={
                "x-goog-api-key": api_key,
                "Content-Type": "application/json",
                "Api-Revision": "2026-05-20",
            },
        )
        try:
            with urllib.request.urlopen(request, timeout=120) as response:
                result = json.load(response)
            break
        except urllib.error.HTTPError as error:
            detail = error.read().decode("utf-8", errors="replace")
            if error.code == 429 and attempt < 3:
                match = re.search(r"retry in (\d+)s", detail, re.IGNORECASE)
                delay = int(match.group(1)) + 2 if match else 62
                print(f"rate limited; retrying in {delay}s")
                time.sleep(delay)
                continue
            raise RuntimeError(
                f"Gemini TTS HTTP {error.code}: {detail[:500]}"
            ) from error
    return base64.b64decode(audio_data_from_response(result))


def write_wave(path, audio):
    path.parent.mkdir(parents=True, exist_ok=True)
    if audio.startswith(b"RIFF"):
        path.write_bytes(audio)
        return
    with wave.open(str(path), "wb") as output:
        output.setnchannels(1)
        output.setsampwidth(2)
        output.setframerate(SAMPLE_RATE)
        output.writeframes(audio)


def validate_wave(path):
    with wave.open(str(path), "rb") as source:
        values = (
            source.getnchannels(), source.getsampwidth(),
            source.getframerate(), source.getnframes(),
        )
    if values[0:3] != (1, 2, SAMPLE_RATE) or values[3] == 0:
        raise RuntimeError(f"Unexpected WAV format for {path.name}: {values}")
    return values[3] / SAMPLE_RATE


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--output-dir", default="sdcard/audio",
        help="Destination directory that mirrors the microSD /audio folder",
    )
    parser.add_argument("--overwrite", action="store_true")
    arguments = parser.parse_args()

    api_key = os.environ.get("GEMINI_API_KEY", "").strip()
    if not api_key:
        raise SystemExit("Set GEMINI_API_KEY in the current shell before running.")

    output_dir = Path(arguments.output_dir)
    for filename, transcript in SCRIPTS.items():
        destination = output_dir / filename
        if destination.exists() and not arguments.overwrite:
            print(f"skip {destination}")
            continue
        print(f"generate {filename}")
        write_wave(destination, request_pcm(api_key, transcript))
        seconds = validate_wave(destination)
        print(f"ok {filename}: {seconds:.2f}s, 24 kHz mono PCM")


if __name__ == "__main__":
    main()
