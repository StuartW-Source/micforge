#include "calibprofiles.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>

namespace {
const char *const kTakes[] = {"room", "you", "others"};

bool copyReplacing(const QString &from, const QString &to) {
    if (!QFile::exists(from)) return !QFile::exists(to) || QFile::remove(to); // the take doesn't exist there either
    const QString tmp = to + ".part";
    QFile::remove(tmp);
    if (!QFile::copy(from, tmp)) return false;
    QFile::remove(to);
    return QFile::rename(tmp, to);
}
} // namespace

const QStringList &CalibrationProfiles::profileParams() {
    static const QStringList keys = {
        "inputGain", "inputChannel", "micType", "needOthers", "needNoise", "needBroadcast",
        "compThreshold", "compMakeup",
        "gateFocus", "gateScLow", "gateScHigh", "gateThreshold", "gateHysteresis", "gateLookahead",
        "gateCloseCheck", "gateTilt", "gateTiltDir"};
    return keys;
}

QString CalibrationProfiles::dirFor(const QString &device) const {
    const QByteArray h = QCryptographicHash::hash(device.toUtf8(), QCryptographicHash::Sha1).toHex().left(16);
    return m_dir + "/devices/" + QString::fromLatin1(h);
}

bool CalibrationProfiles::has(const QString &device) const {
    return !device.isEmpty() && QFile::exists(dirFor(device) + "/profile.ini");
}

bool CalibrationProfiles::save(const QString &device, const QString &description, const QVariantMap &params) {
    if (device.isEmpty() || !m_settings.value("calib/valid").toBool()) return false;
    const QString dir = dirFor(device);
    if (!QDir().mkpath(dir)) return false;
    bool ok = true;
    for (const char *t : kTakes) ok &= copyReplacing(m_dir + "/" + t + ".f32", dir + "/" + t + ".f32");
    QSettings p(dir + "/profile.ini", QSettings::IniFormat);
    p.clear();
    p.setValue("device", device);
    p.setValue("description", description);
    p.setValue("savedAt", QDateTime::currentDateTime());
    m_settings.beginGroup("calib");
    for (const QString &k : m_settings.childKeys()) p.setValue("calib/" + k, m_settings.value(k));
    m_settings.endGroup();
    for (const QString &k : profileParams())
        if (params.contains(k)) p.setValue("params/" + k, params.value(k));
    p.sync();
    return ok && p.status() == QSettings::NoError;
}

QVariantMap CalibrationProfiles::restore(const QString &device) {
    if (!has(device)) return {};
    const QString dir = dirFor(device);
    QSettings p(dir + "/profile.ini", QSettings::IniFormat);
    for (const char *t : kTakes) copyReplacing(dir + "/" + t + ".f32", m_dir + "/" + t + ".f32");
    m_settings.remove("calib");
    p.beginGroup("calib");
    for (const QString &k : p.childKeys()) m_settings.setValue("calib/" + k, p.value(k));
    p.endGroup();
    m_settings.setValue("calib/device", device);
    QVariantMap params;
    p.beginGroup("params");
    for (const QString &k : p.childKeys()) params[k] = p.value(k).toDouble();
    p.endGroup();
    return params;
}

QVariantList CalibrationProfiles::list() const {
    QVariantList out;
    const QDir root(m_dir + "/devices");
    for (const QString &sub : root.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        QSettings p(root.filePath(sub) + "/profile.ini", QSettings::IniFormat);
        if (!p.contains("device")) continue;
        out << QVariantMap{{"device", p.value("device")}, {"description", p.value("description")},
                           {"savedAt", p.value("savedAt")}};
    }
    return out;
}
