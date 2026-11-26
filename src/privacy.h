// Removing personal details from text that may be shared (diagnostic reports). Qt Core only.
#pragma once

#include <QDir>
#include <QRegularExpression>
#include <QString>
#include <QSysInfo>

namespace mf::privacy {

// Replace file paths by their last component: "/home/x/plugins/ADelay.vst3" -> "…/ADelay.vst3".
inline QString stripPaths(QString t) {
    static const QRegularExpression path(R"((?:~|/)[^\s:'"“”]*/([^\s/:'"“”]+))");
    t.replace(path, QStringLiteral("…/\\1"));
    return t;
}

// Remove the home folder, user name and computer name. `user`/`host`/`home` are injectable for tests.
inline QString scrub(QString t, QString home = QDir::homePath(),
                     QString user = qEnvironmentVariable("USER", qEnvironmentVariable("USERNAME")),
                     QString host = QSysInfo::machineHostName()) {
    if (home.size() > 1) t.replace(home, QStringLiteral("~"));
    // Computer name first: it often contains the user name ("alex-desktop").
    if (host.size() > 1)
        t.replace(QRegularExpression("(?<![A-Za-z0-9])" + QRegularExpression::escape(host) + "(?![A-Za-z0-9])",
                                     QRegularExpression::CaseInsensitiveOption),
                  QStringLiteral("<computer>"));
    if (user.size() > 1)
        t.replace(QRegularExpression("(?<![A-Za-z0-9])" + QRegularExpression::escape(user) + "(?![A-Za-z0-9])",
                                     QRegularExpression::CaseInsensitiveOption),
                  QStringLiteral("<user>"));
    return t;
}

// What kind of device a PipeWire node is, without its (possibly personal) name.
inline QString deviceKind(const QString &n) {
    if (n.startsWith("bluez")) return QStringLiteral("Bluetooth");
    if (n.contains(".usb-")) return QStringLiteral("USB");
    if (n.contains(".pci-") || n.contains("platform-")) return QStringLiteral("built-in");
    if (n.isEmpty()) return QStringLiteral("none");
    return QStringLiteral("other");
}

} // namespace mf::privacy
