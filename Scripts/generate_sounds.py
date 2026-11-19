"""Synthesise FormulaXR's sound effects as 16-bit mono WAV files (standard library only).

    python -I Scripts/generate_sounds.py <output folder>

Writes:
    motor_loop.wav   1 s seamless loop: slot car motor whine (pitch is shifted in game by speed)
    deslot.wav       plastic clatter of a car tumbling off the track
    light_beep.wav   one start light coming on
    go_beep.wav      lights out
    finish.wav       race finished chime
All sounds are our own (generated), so they can live in the public repository.
"""
import math
import random
import struct
import sys
import wave
from pathlib import Path

RATE = 44100


def write(path, samples, peak=0.85):
    top = max(1e-9, max(abs(s) for s in samples))
    scale = peak / top
    with wave.open(str(path), "wb") as out:
        out.setnchannels(1)
        out.setsampwidth(2)
        out.setframerate(RATE)
        out.writeframes(b"".join(struct.pack("<h", int(max(-1.0, min(1.0, s * scale)) * 32767)) for s in samples))


def motor_loop():
    """Electric whine: a 220 Hz fundamental with buzzy harmonics, commutator ripple and gear rattle.
    Every frequency is a whole number of cycles per second, so the 1 s loop is seamless."""
    rng = random.Random(7)
    count = RATE
    # A little noise, also looped seamlessly (generated once, then windowed by the period).
    noise = [rng.uniform(-1.0, 1.0) for _ in range(count)]
    samples = []
    for i in range(count):
        t = i / RATE
        whine = (math.sin(2 * math.pi * 220 * t)
                 + 0.55 * math.sin(2 * math.pi * 440 * t + 0.3)
                 + 0.35 * math.sin(2 * math.pi * 660 * t + 1.1)
                 + 0.20 * math.sin(2 * math.pi * 1320 * t + 0.7))   # commutator buzz
        rattle = 1.0 + 0.18 * math.sin(2 * math.pi * 30 * t)        # gear mesh ripple
        smooth_noise = 0.5 * (noise[i] + noise[i - 1])
        samples.append(whine * rattle * 0.8 + 0.12 * smooth_noise)
    return samples


def deslot():
    """Plastic clatter: a thump, then a few bright decaying clicks as the car tumbles and settles."""
    rng = random.Random(11)
    length = int(0.7 * RATE)
    samples = [0.0] * length
    # Thump: low, short.
    for i in range(int(0.08 * RATE)):
        t = i / RATE
        samples[i] += 0.9 * math.sin(2 * math.pi * 140 * t) * math.exp(-t * 45)
    # Clicks: band-limited noise bursts with decreasing loudness.
    start = 0.0
    for hit in range(7):
        start += rng.uniform(0.05, 0.11)
        loudness = 0.9 * (0.72 ** hit)
        pitch = rng.uniform(1800, 3200)
        offset = int(start * RATE)
        previous = 0.0
        for i in range(int(0.04 * RATE)):
            if offset + i >= length:
                break
            t = i / RATE
            raw = rng.uniform(-1.0, 1.0)
            previous = 0.6 * previous + 0.4 * raw              # soften the hiss a little
            tone = math.sin(2 * math.pi * pitch * t)
            samples[offset + i] += loudness * (0.6 * previous + 0.5 * tone) * math.exp(-t * 120)
    return samples


def beep(frequency, seconds, fade=0.01):
    count = int(seconds * RATE)
    samples = []
    for i in range(count):
        t = i / RATE
        envelope = min(1.0, t / fade, (seconds - t) / fade)
        samples.append(envelope * (math.sin(2 * math.pi * frequency * t) + 0.25 * math.sin(2 * math.pi * 2 * frequency * t)))
    return samples


def finish():
    """Three rising notes (C6, E6, G6), each ringing out."""
    notes = [(1046.5, 0.0), (1318.5, 0.14), (1568.0, 0.28)]
    length = int(1.3 * RATE)
    samples = [0.0] * length
    for frequency, start in notes:
        offset = int(start * RATE)
        for i in range(length - offset):
            t = i / RATE
            samples[offset + i] += math.sin(2 * math.pi * frequency * t) * math.exp(-t * 3.5) * min(1.0, t / 0.005)
    return samples


def main():
    out = Path(sys.argv[1] if len(sys.argv) > 1 else "Build/audio")
    out.mkdir(parents=True, exist_ok=True)
    write(out / "motor_loop.wav", motor_loop(), peak=0.7)
    write(out / "deslot.wav", deslot())
    write(out / "light_beep.wav", beep(880, 0.18))
    write(out / "go_beep.wav", beep(1320, 0.45))
    write(out / "finish.wav", finish())
    print(f"SOUNDS_OK {out}")


if __name__ == "__main__":
    main()
