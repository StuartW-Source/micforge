// XDG autostart implementation of IAutostart (Linux desktops).
#include "platform/autostart.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>

namespace mf {
namespace {

class XdgAutostart final : public IAutostart {
public:
    bool enabled() const override { return QFile::exists(path()); }

    void setEnabled(bool on, bool hidden) override {
        if (on) write(hidden);
        else QFile::remove(path());
    }

    void refresh(bool hidden) override {
        if (!enabled()) return;
        const QString self = QCoreApplication::applicationFilePath();
        const bool installed = self.startsWith("/usr/") || self.startsWith(QDir::homePath() + "/.local/bin/");
        QString current;
        if (QFile f(path()); f.open(QIODevice::ReadOnly))
            for (const QByteArray &line : f.readAll().split('\n'))
                if (line.startsWith("Exec=")) current = QString::fromUtf8(line.mid(5)).section('"', 1, 1);
        if (current == self) return;
        if (installed || !QFileInfo::exists(current)) {
            qInfo("autostart: login entry now points at %s (was %s)", qPrintable(self), qPrintable(current));
            write(hidden);
        }
    }

    QString describe() const override { return path(); }

private:
    static QString path() {
        // The test instance (MICFORGE_TEST) must never touch the real login entry.
        if (qEnvironmentVariableIsSet("MICFORGE_TEST"))
            return QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) + "/test-autostart/micforge.desktop";
        return QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + "/autostart/micforge.desktop";
    }
    static void write(bool hidden) {
        QDir().mkpath(QFileInfo(path()).path());
        QFile f(path());
        if (!f.open(QIODevice::WriteOnly)) {
            qWarning("autostart: can't write %s", qPrintable(path()));
            return;
        }
        f.write(QString("[Desktop Entry]\nType=Application\nName=MicForge\n"
                        "Comment=Microphone processing for voice chat\n"
                        "Exec=\"%1\"%2\nIcon=micforge\n"
                        "X-GNOME-Autostart-enabled=true\nX-KDE-autostart-phase=2\n")
                    .arg(QCoreApplication::applicationFilePath(), hidden ? " --minimized" : "")
                    .toUtf8());
    }
};

} // namespace

std::unique_ptr<IAutostart> createAutostart() { return std::make_unique<XdgAutostart>(); }

} // namespace mf
