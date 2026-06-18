# bent-sines

A dirty noisy sine wave oscillator for the Korg Prologue, built with the [logue SDK](https://github.com/korginc/logue-sdk).

A single sine oscillator run through FM self-feedback, a wavefolder, pitch jitter, noise, and sample rate reduction. Ranges from a subtly unstable sine to something completely broken.

## Parameters

| Control | Function |
|---|---|
| **Shape** | Drive — soft saturation, adds odd harmonics. LFO on Shape animates the harmonic content over time. |
| **Shift-Shape** | Noise mix — blends white noise into the output |
| **Param 1 — Jitter** | Random low-frequency pitch drift, like an unstable analog oscillator |
| **Param 2 — FM Fdbk** | FM self-feedback — feeds the previous output back into the phase. Low values add warmth, high values get chaotic |
| **Param 3 — WavFold** | Wavefolder — amplifies the signal then folds it back into range. Stacks harmonics fast |
| **Param 4 — SR Redu** | Sample rate reduction — holds each sample for longer, dropping the effective rate from 48kHz down to ~1kHz at maximum |

## Signal chain

```
sine → FM feedback → wavefold → drive → noise mix → SR reduction
```

## Installation

Download `vangelis.prlgunit` and transfer to the Prologue using the [logue Sound Librarian](https://www.korg.com/us/support/download/software/1/477/1900/) or [logue-cli](https://github.com/korginc/logue-cli).

## Building from source

Requires the [logue SDK Docker build environment](https://github.com/korginc/logue-sdk#setting-up-the-build-environment).

```sh
# Inside the Docker container
build prologue/vangelis-1/
```

## Starting points

- **Subtle instability**: Jitter 10–20%, everything else low
- **Dirty sine**: Shape 40–60%, FM Fdbk 20–30%
- **Buchla-style**: WavFold 40–70%, Shape low
- **Lo-fi texture**: SR Redu 30–50%, Noise mix 10–20%
- **Chaos**: FM Fdbk 70%+, WavFold 50%+, Jitter 30%+
