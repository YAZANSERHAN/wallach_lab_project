# Underwater Acoustic Stimulus System

A four-channel speaker driver for fish behavior experiments in Dr. Avner Wallach's lab (Technion, Project A).

An Adafruit Grand Central M4 generates the stimulus on its 12-bit DACs at 96 kHz. Each channel passes through an RC reconstruction filter, a 74HC4066 analog switch and an LM386 power amplifier to an 8 Ω speaker in one corner of a 2 × 2 m pool. Each speaker is switched on or off independently, so the researcher controls where the sound comes from.

Author: Yazan Sarhan. Supervisor: Dr. Avner Wallach.

## Repository layout

| Folder | Contents |
| --- | --- |
| `firmware/` | Arduino sketch for the Grand Central M4 |
| `hardware/` | Fusion Electronics schematic and board files, Gerbers, bill of materials |
| `docs/` | Final report and final presentation (PDF), schematic and PCB images, oscilloscope measurements |

## Stimulus modes

| Mode | Method | Parameters |
| --- | --- | --- |
| Sine | DDS, phase accumulator over a 1024-point sine table | Frequency, amplitude |
| White noise | xorshift pseudo-random generator | Amplitude |
| Noise pulse | White noise burst of p ms, repeated every r ms | p, r, amplitude |

The same stimulus is written to both DACs, and the 74HC4066 selects which speakers hear it.

| Channel | Control pin | Switch | DAC | Amplifier | Connector |
| --- | --- | --- | --- | --- | --- |
| 1 | D2 (CNTRL1) | A | A0 | U1 | SPKR |
| 2 | D3 (CNTRL2) | B | A1 | U3 | SPKR1 |
| 3 | D4 (CNTRL3) | C | A1 | U2 | SPKR2 |
| 4 | D5 (CNTRL4) | D | A0 | U4 | SPKR3 |

## Serial commands

115200 baud, one command per line.

| Command | Action | Default |
| --- | --- | --- |
| `s0` / `s1` / `s2` | Select sine / white noise / noise pulse | `s0` |
| `f<Hz>` | Sine frequency, 1 to 20000 Hz | 1000 |
| `a<0..1>` | Sine amplitude | 0.5 |
| `n<0..1>` | Noise amplitude | 0.3 |
| `p<ms>` | Pulse on time | 50 ms |
| `r<ms>` | Pulse period | 500 ms |
| `on1` .. `on4` | Turn one channel on | |
| `off1` .. `off4` | Turn one channel off | |
| `on` / `off` (or `stop`) | All four channels on / off | |
| `?` | Print current settings | |

Example: `s0`, `f2000`, `a0.6`, `on1` plays a 2 kHz tone from the speaker on SPKR.

Safety: all channels start off at power-up, and each channel turns itself off after 60 s on.

## Hardware summary

| Part | Value | Role |
| --- | --- | --- |
| R1, C7 (per channel) | 1 kΩ, 47 nF | RC reconstruction filter, corner about 3.4 kHz |
| 74HC4066 | 3.3 V supply | Analog switch per channel |
| C1, P1 | 1 µF, 10 kΩ pot | Input coupling and level |
| LM386N-1 | Gain 20 | Power amplifier |
| C6 | 22 µF | Bypass on LM386 pin 7 |
| R3, C5 | 10 Ω, 47 nF | Zobel network |
| C4 | 220 µF | Output coupling to the speaker |
| D1 | 1N5400, 3 A | Reverse polarity protection on Vin |
| C2 + 100 nF per amplifier | 100 µF | Supply decoupling |

## Getting started

1. Install the Arduino IDE and add the Adafruit SAMD boards package. Select **Adafruit Grand Central M4**.
2. Open `firmware/four_speaker_pcb/four_speaker_pcb.ino` and upload it to the board.
3. Plug the PCB shield onto the Grand Central headers and connect the speakers to SPKR, SPKR1, SPKR2 and SPKR3.
4. Connect the 12.7 V battery to Vin. D1 protects the board if the battery is reversed.
5. Open the Serial Monitor at 115200 baud with "Newline" line ending, and send the commands below.

## Measurements

Breadboard measurements, single channel, before the RC filter was added. See `docs/images/scope/`.

| Frequency | Peak-to-peak |
| --- | --- |
| 501 Hz | 7.04 V |
| 1 kHz | 6.80 V |
| 2 kHz | 8.40 V (uncertain scope reading) |
| 15 kHz | 5.28 V |

PCB results with the RC filter are in Section 8 of the report.
