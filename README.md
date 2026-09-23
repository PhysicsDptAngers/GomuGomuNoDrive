# Rubber Zener (GomuGomuNoDrive): Analog Circuit & Anti-Aliased Virtual Analog Model

This repository provides open-source hardware designs, physical bench measurement datasets, numerical simulation routines, and real-time DSP emulations for the **Rubber Zener** clipping topology.

The Rubber Zener is a parameterized audio clipping circuit based on the bipolar junction transistor (BJT) $V_{be}$ multiplier, augmented with local passive and reactive networks. It allows decoupled analog tuning of positive and negative conduction thresholds, soft-knee transition slopes, deep-saturation current ceilings, frequency-dependent clipping profiles, and envelope-dependent DC baseline sag.

> **Academic Context:**  
> This repository contains the reference implementation, experimental datasets, and validation routines accompanying the research paper:  
> *The Rubber Zener Clipping Topology: Circuit Analysis, Dynamic Characterization, and Anti-Aliased Virtual Analog Modeling* (currently under peer review).

---

## コムコム の Drive  (Gomu Gomu No Drive) ?

The $V_{be}$ multiplier circuit is sometimes called "Rubber Zener" because, like rubber, its threshold is malleable. In pop culture, the most well-known "rubber man" is *Luffy* (ルフィ) from *One Piece* (ワンピース). His iconic attack names start with "ゴムゴム" (Gomu Gomu) since ゴム (gomu) in Japanese means rubber. Hence the name of this distortion pedal: コムコム の Drive (Gomu Gomu No Drive).

---

## Repository Structure

```text
GomuGomuNoDrive/
├── Data/
│   ├── AC/                                # AC bench measurements, steady-state datasets, and fitting scripts
│   └── DC characteristics/                # Static I-V measurement datasets
├── Hardware/
│   ├── GomuGomuNoDrive_hard/              # Shunt dipole hard-clipping implementation (KiCad, 3D models, Gerbers)
│   └── GomuGomuNoDrive_Soft/              # Feedback loop soft-clipping implementation (KiCad, BOM, production files)
├── NumericalSimulation/                   # SPICE benchmarks, 4D ODE solvers, harmonic balance, and validation routines
└── Software/
    ├── Faust/                             # Faust core DSP architecture and C++ wrappers
    ├── JesuSonic/                         # Real-time JSFX implementation for Cockos REAPER
    └── Juce Plugin/                       # Full C++ audio plugin project (VST3, CLAP, LV2, Standalone)
```

#### Pre-compiled Binaries

Pre-compiled binaries for 64-bit Windows and Linux systems (VST3 and Standalone) are provided under the [Releases](../../releases) section.

---

## License

This project adopts a multi-licensing scheme adapted to its different components:

- **Software** (JUCE audio plugin, DSP solvers, simulation scripts): Licensed under the [GNU General Public License v3.0](LICENSE-GPL.txt) (GPL-3.0-or-later). Bundled cabinet impulse responses are licensed under GPL-2.0-or-later (Copyright © 2013 David Fau Casquel / Guitarix project).
- **Hardware** (KiCad schematics, PCB layouts, manufacturing files): Licensed under the [CERN Open Hardware Licence Version 2 – Strongly Reciprocal](LICENSE-CERN.txt) (CERN-OHL-S-2.0).
- **Documentation & Data** (Manuscript, figures, measurement datasets): Licensed under the [Creative Commons Attribution 4.0 International License](https://creativecommons.org/licenses/by/4.0/) (CC-BY-4.0).

---

## Citation

If you use this circuit topology, DSP implementation, or dataset in academic work or derivative projects, please cite:


TBA !
