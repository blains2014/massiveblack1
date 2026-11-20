# Massive Black — Negative-Space Phaser → Reverb Imprint VST3

A Windows **VST3** audio effect for FL Studio by **SnoopyChu**. **Massive Black** extracts the sweeping negative-space pattern of an all-pass phaser and imprints those exact sweeping cuts (or inverted ghost bands) directly onto a stereo reverb tail in real time.

## Controls
- **Negative Phaser:** `Rate`, `Depth`, `Center`, `Feedback`, `Stages` (4/6/8/12), and `Mode` (`Carve Cuts` vs `Negative Ghost`).
- **Imprint:** `Imprint` (how deeply the sweeping pattern cuts the reverb tail) and `Dyn Track` (modulates imprint depth using the input's negative-space envelope).
- **Reverb & Mix:** `Size`, `Damping`, `Width`, `Pre-Delay`, `Mix`, and `Output`.

## Building on GitHub Actions
1. Create a new repository on GitHub and upload everything inside this `Massive Black` folder (including `.github`).
2. Open the **Actions** tab, wait for **Build Massive Black Windows VST3** to finish, and download **Massive Black-Windows-VST3**.
3. Run `Massive Black_Windows_Installer.exe` and scan plugins in FL Studio.
