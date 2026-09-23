# GomuGomuNoDrive — JUCE Audio Plugin

This directory contains the reference C++ implementation of the **GomuGomuNoDrive** (Rubber Zener) virtual analog audio plugin built with the [JUCE framework](https://juce.com/) (version 8.0.13).

The plugin features:
- A real-time, four-dimensional implicit Newton-Raphson solver modeling the physical Ebers-Moll BJT transport equations and Shockley diode characteristics.
- 2x polyphase half-band IIR multirate oversampling (>90 dB stopband rejection).
- Dual 12-band graphic equalizers (pre-distortion drive sculpting and post-distortion tone shaping).
- Dynamic CRT-style visualizers (static I/V transfer characteristic and real-time dual-trace time-domain waveform monitor).
- Integrated zero-latency cabinet impulse response (IR) convolution engine.
- Targets: **VST3**, **CLAP**, **LV2**, and **Standalone**.

---

## Important Files

- `CMakeLists.txt` — CMake build configuration and target definitions.
- `PluginProcessor.h` / `PluginProcessor.cpp` — Core DSP pipeline: APVTS parameter layout, implicit 4D Newton-Raphson solver (`RubberZener`), oversampler, and convolution engine.
- `PluginEditor.h` / `PluginEditor.cpp` — Graphical user interface, custom hardware LookAndFeel (Davies 1510 knobs, vintage levers), graphic EQ components, and CRT scopes.
- `Resources/IR/` — Bundled speaker cabinet impulse response collections (`BestPlugins_Amps` and `BestPlugins_Bands`).

---

## Building from CLI


> **Note:** Pre-compiled binaries for Linux and Windows (VST3, CLAP, Standalone) are available under the GitHub [Releases](../../releases) section if you do not wish to build from source.



### Linux (Debian / Ubuntu)

1. **Install system dependencies:**

   ```bash
   sudo apt update
   sudo apt install -y build-essential cmake ninja-build git \
       libasound2-dev libjack-jackd2-dev libgl1-mesa-dev libglu1-mesa-dev \
       libx11-dev libxinerama-dev libxext-dev libfreetype6-dev
   ```

2. **Clone the JUCE framework (tested with version 8.0.13):**

# Option A: Tested version (8.0.13)
git clone --depth 1 --branch 8.0.13 https://github.com/juce-framework/JUCE.git JUCE

# Option B: Latest version (master branch)
git clone --depth 1 https://github.com/juce-framework/JUCE.git JUCE


3. **Configure and compile with CMake and Ninja:**

   ```bash
   cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
   cmake --build build --config Release -j$(nproc)
   ```

   Compiled artifacts (VST3, CLAP, LV2, and Standalone binary) will be generated inside `build/GomuGomuNoDrive_artefacts/Release/`.

---

### Windows (CLI)

1. **Prerequisites:**
   - Visual Studio Build Tools (or Visual Studio Community) with the **C++ Desktop Development** workload installed.
   - [CMake](https://cmake.org/download/) (>= 3.22) and [Git](https://git-scm.com/) added to your system `PATH`.
   - Optional: [Ninja](https://github.com/ninja-build/ninja/releases) for faster parallel builds.

2. **Clone the JUCE framework (version 8.0.13):**

   Open PowerShell or the Developer Command Prompt:

   ```powershell
   git clone --depth 1 --branch 8.0.13 https://github.com/juce-framework/JUCE.git JUCE
   ```

3. **Configure and compile:**

   *Using Ninja (from Developer Command Prompt):*
   ```powershell
   cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
   cmake --build build --config Release
   ```

   *Or using the default MSVC generator:*
   ```powershell
   cmake -B build -G "Visual Studio 17 2022" -A x64
   cmake --build build --config Release --parallel
   ```

   Binaries will be available in `build/GomuGomuNoDrive_artefacts/Release/`.

---

## Impulse Responses (IR) & Licensing

- **Plugin Source Code:** Released under the [GNU General Public License v3.0](https://www.gnu.org/licenses/gpl-3.0.html) (GPL-3.0-or-later).
- **Cabinet Impulse Responses (`Resources/IR/`):** Sourced from the open-source **Guitarix** project, collected and created by David Fau Casquel (BestPlugins). These IR files are licensed under the [GNU General Public License v2.0](https://www.gnu.org/licenses/old-licenses/gpl-2.0.html) (GPL-2.0-or-later). See `Resources/IR/BestPlugins_Amps/LICENSE` and `Resources/IR/BestPlugins_Bands/LICENSE` for full notices.
