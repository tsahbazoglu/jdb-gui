# 🦅 Hawk Debugger

## [2.0.0-wayland] - 2026-04-25

## 🚀 Overview
Version 2.0-wayland introduces a high-performance, native Wayland implementation designed for modern Linux environments (GNOME/KDE). This release marks a significant milestone in bypassing strict compositor focus-stealing prevention to ensure a seamless debugging experience.

---

## 💎 Key Features & Breakthroughs

### 1. "Wayland-Force" Activation Logic
* **Compositor Bypass:** Successfully implemented a surface re-mapping strategy (`Hide` -> `StaysOnTop` -> `Show`) to force the debugger to the front when a breakpoint hits.
* **Browser-to-App Handover:** The app now reliably pops over web browsers or other full-screen applications without requiring user-side setting changes or Chrome flags.
* **Console Cleanup:** Removed unsupported `requestActivate()` calls, eliminating the `Wayland does not support QWindow::requestActivate()` log spam.

### 2. Intelligent JDB Auto-Navigation
* **Zero-Latency Navigation:** Refined the JDB output buffer and regex handling. The app now instantly opens the relevant `.java` source file and scrolls to the exact line of execution.
* **Visual Hit Detection:** The current execution line is highlighted with a high-visibility green selection, distinct from standard breakpoints.

### 3. Restored Professional Toolset
* **Sidebar & Project Mapping:** Restored the dynamic folder tree that correctly maps project-relative structures passed via the command line.
* **Full Command Suite:** Re-integrated the complete button bar, including:
    * **Execution:** `RESUME`, `NEXT`, `STEP`
    * **Inspection:** `LOCALS`, `STACK`, `THREADS`, `PRINT`, `DUMP`
    * **Management:** `🔴 BP` (Breakpoint Toggle), `💾 SAVE` (Phase Export)

### 4. Health & Persistence
* **Phase Management:** Full support for `.hawk_bps.json` and `.sahin_bps.json` persistence. Switch between complex breakpoint groups via the "PHASE" dropdown.
* **The "Eye" (Health Pulse):** Restored the rhythmic background pulse to indicate the monitor is alive and actively watching the JDWP transport.

---

## 🛠 Technical Environment
* **Backend:** Qt 6/5 (Wayland QPA)
* **Platform:** Forced Native Wayland (`QT_QPA_PLATFORM=wayland`)
* **Protocol:** xdg-shell / xdg-activation (via Layer-Shift bypass)

---

## 📝 Commit Summary
`feat(wayland): implement native focus-stealing bypass and JDB auto-nav`
* *Implemented Hide/Show mapping trick for focus.*
* *Restored sidebar folder tree building.*
* *Restored persistence and search logic.*
## [1.1.0] - 2026-04-29
### Added
- **Predator Focus Engine**: Implemented native `QWindow` handle manipulation to bypass Wayland/GNOME focus restrictions.
- **High-Performance Navigation**: Automatic source file opening and line highlighting now synchronized with breakpoint hits.
- **Safe Activation**: 3-second `WindowStaysOnTopHint` handshake for flicker-free window presentation.

### Fixed
- **Double-Splash Bug**: Removed destructive `QWidget::setWindowFlags` calls that caused UI recreation and flickering.
- **Wayland Stability**: Added platform checks to prevent Segmentation Faults when `QX11Info` is unavailable.
- **Scroll Alignment**: Fixed a race condition where the editor view would reset during window activation.

---

## [1.0.0] - 2026-04-25
### Added
- Initial stable release of Hawk Java Debugger.
- Integrated `jdb` process management and output parsing.
- Support for visual breakpoints and project-wide source teleportation.
