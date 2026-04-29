# 🦅 Hawk Releases

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
