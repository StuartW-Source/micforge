// System-wide keyboard shortcuts that work while another app has focus (mute, push-to-talk).
// Linux: the XDG GlobalShortcuts portal (KDE Plasma 6, GNOME 48+, Hyprland…): the desktop asks the user
// to confirm or choose the keys. Windows (planned): RegisterHotKey / a keyboard hook.
#pragma once

#include <QMap>
#include <QString>
#include <QVector>

#include <functional>
#include <memory>

namespace mf {

class IGlobalShortcuts {
public:
    struct Shortcut { QString id, description, preferredTrigger; };
    virtual ~IGlobalShortcuts() = default;
    // Ask the desktop for these shortcuts (may show a confirmation dialog the first time).
    virtual void bind(const QVector<Shortcut> &shortcuts) = 0;
    // Open the desktop's dialog to change the keys, if it has one.
    virtual void configure() {}
    virtual bool available() const = 0;
    virtual QString status() const = 0;                 // plain English, for Settings
    virtual QMap<QString, QString> triggers() const = 0; // id -> key description, e.g. "Ctrl+Alt+M"

    std::function<void(const QString &id)> onActivated, onDeactivated; // key pressed / released
    std::function<void()> onChanged;                                    // status or triggers changed

    // Tests can replace the implementation.
    static std::function<std::unique_ptr<IGlobalShortcuts>()> &factoryOverride();
};

std::unique_ptr<IGlobalShortcuts> createGlobalShortcuts();

} // namespace mf
