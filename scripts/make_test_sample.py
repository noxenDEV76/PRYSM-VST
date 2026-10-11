#!/usr/bin/env python3
"""Writes a 2 s stereo 16-bit WAV (decaying 220 Hz tone) used by the VST3 host test. Usage: make_test_sample.py out.wav"""
import math, struct, sys, wave

out = sys.argv[1] if len(sys.argv) > 1 else "test_sample.wav"
sr = 48000
with wave.open(out, "wb") as w:
    w.setnchannels(2); w.setsampwidth(2); w.setframerate(sr)
    frames = bytearray()
    for i in range(sr * 2):
        env = math.exp(-2.0 * i / sr)
        frames += struct.pack("<hh",
                              int(32767 * 0.6 * env * math.sin(2 * math.pi * 220 * i / sr)),
                              int(32767 * 0.6 * env * math.sin(2 * math.pi * 221 * i / sr)))
    w.writeframes(bytes(frames))
print("wrote", out)
