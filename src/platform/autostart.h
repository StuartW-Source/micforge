// Start-at-login integration. Linux: XDG autostart (~/.config/autostart/micforge.desktop), which
// KDE/GNOME run as a systemd user unit. Windows (planned): HKCU\...\Run.
// Decision record: docs/ARCHITECTURE.md, "Start-up lifecycle".
#pragma once

#include <QString>

#include <memory>

namespace mf {

class IAutostart {
public:
    virtual ~IAutostart() = default;
    virtual bool enabled() const = 0;
    // Create or remove the login entry. `hidden` starts MicForge in the tray (--minimized).
    virtual void setEnabled(bool on, bool hidden) = 0;
    // Keep an existing entry pointing at an installed copy of MicForge (never at a development build).
    virtual void refresh(bool hidden) = 0;
    virtual QString describe() const = 0; // for diagnostics, e.g. the entry's path
};

std::unique_ptr<IAutostart> createAutostart();

} // namespace mf
