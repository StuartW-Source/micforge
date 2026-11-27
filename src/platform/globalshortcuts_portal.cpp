// XDG desktop portal implementation of IGlobalShortcuts (org.freedesktop.portal.GlobalShortcuts).
#include "platform/globalshortcuts.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusObjectPath>
#include <QDBusPendingCall>
#include <QDBusReply>
#include <QObject>
#include <QRandomGenerator>

namespace {

struct PortalShortcut {
    QString id;
    QVariantMap props;
};
QDBusArgument &operator<<(QDBusArgument &a, const PortalShortcut &s) {
    a.beginStructure(); a << s.id << s.props; a.endStructure(); return a;
}
const QDBusArgument &operator>>(const QDBusArgument &a, PortalShortcut &s) {
    a.beginStructure(); a >> s.id >> s.props; a.endStructure(); return a;
}

} // namespace
Q_DECLARE_METATYPE(PortalShortcut)

namespace mf {

std::function<std::unique_ptr<IGlobalShortcuts>()> &IGlobalShortcuts::factoryOverride() {
    static std::function<std::unique_ptr<IGlobalShortcuts>()> f;
    return f;
}

namespace {

const char *kService = "org.freedesktop.portal.Desktop";
const char *kPath = "/org/freedesktop/portal/desktop";
const char *kIface = "org.freedesktop.portal.GlobalShortcuts";

class PortalShortcuts final : public QObject, public IGlobalShortcuts {
    Q_OBJECT
public:
    PortalShortcuts() {
        qDBusRegisterMetaType<PortalShortcut>();
        qDBusRegisterMetaType<QList<PortalShortcut>>();
        QDBusInterface probe(kService, kPath, kIface, QDBusConnection::sessionBus());
        m_version = probe.isValid() ? probe.property("version").toUInt() : 0;
        m_status = m_version ? tr("Not set up yet.") : tr("This desktop doesn't offer system-wide shortcuts (no GlobalShortcuts portal).");
        auto bus = QDBusConnection::sessionBus();
        bus.connect(kService, kPath, kIface, "Activated", this, SLOT(activated(QDBusObjectPath, QString, qulonglong, QVariantMap)));
        bus.connect(kService, kPath, kIface, "Deactivated", this, SLOT(deactivated(QDBusObjectPath, QString, qulonglong, QVariantMap)));
        bus.connect(kService, kPath, kIface, "ShortcutsChanged", this, SLOT(shortcutsChanged(QDBusMessage)));
    }

    bool available() const override { return m_version > 0; }
    QString status() const override { return m_status; }
    QMap<QString, QString> triggers() const override { return m_triggers; }

    void bind(const QVector<Shortcut> &shortcuts) override {
        if (!available()) return;
        m_wanted = shortcuts;
        if (!m_pendingStep.isEmpty()) { m_rebind = true; return; } // a request is in flight: redo when it answers
        if (m_session.isEmpty()) createSession();
        else if (!m_bound) bindNow();
        else {
            // A session's shortcuts can only be bound once: close it and start a new one with the new list.
            QDBusConnection::sessionBus().asyncCall(QDBusMessage::createMethodCall(
                kService, m_session, "org.freedesktop.portal.Session", "Close"));
            m_session.clear();
            m_bound = false;
            createSession();
        }
    }

    void configure() override {
        if (m_session.isEmpty() || m_version < 2) return;
        QDBusMessage m = QDBusMessage::createMethodCall(kService, kPath, kIface, "ConfigureShortcuts");
        m << QVariant::fromValue(QDBusObjectPath(m_session)) << QString() << QVariantMap{};
        QDBusConnection::sessionBus().asyncCall(m);
    }

private slots:
    void activated(const QDBusObjectPath &session, const QString &id, qulonglong, const QVariantMap &) {
        if (session.path() == m_session && onActivated) onActivated(id);
    }
    void deactivated(const QDBusObjectPath &session, const QString &id, qulonglong, const QVariantMap &) {
        if (session.path() == m_session && onDeactivated) onDeactivated(id);
    }
    // Takes the whole message: QtDBus can't connect a slot with a QDBusArgument parameter by signature.
    void shortcutsChanged(const QDBusMessage &msg) {
        const QVariantList args = msg.arguments();
        if (args.size() < 2) return;
        if (qvariant_cast<QDBusObjectPath>(args.at(0)).path() != m_session) return;
        readTriggers(args.at(1).value<QDBusArgument>());
    }
    void response(uint code, const QVariantMap &results) {
        const QString step = m_pendingStep;
        m_pendingStep.clear();
        if (code != 0) {
            m_rebind = false;
            m_status = code == 1 ? tr("You cancelled the shortcut setup.") : tr("The desktop couldn't set up shortcuts.");
            changed();
            return;
        }
        if (step == "session") {
            m_session = results.value("session_handle").toString();
            if (m_session.isEmpty()) m_session = qvariant_cast<QDBusObjectPath>(results.value("session_handle")).path();
            bindNow();
        } else if (step == "bind") {
            m_bound = true;
            readTriggers(results.value("shortcuts").value<QDBusArgument>());
            m_status = tr("Ready.");
            changed();
        }
        if (m_rebind && m_pendingStep.isEmpty()) {
            m_rebind = false;
            if (!sameIds(m_sent, m_wanted)) bind(m_wanted); // the list changed while the desktop was asking
        }
    }

private:
    // The portal answers each request with a Response signal on a predictable path; subscribe first.
    QString expectResponse(const QString &step) {
        const QString token = "micforge" + QString::number(QRandomGenerator::global()->generate());
        QString sender = QDBusConnection::sessionBus().baseService().mid(1).replace('.', '_');
        const QString path = QString("/org/freedesktop/portal/desktop/request/%1/%2").arg(sender, token);
        QDBusConnection::sessionBus().connect(kService, path, "org.freedesktop.portal.Request", "Response", this,
                                              SLOT(response(uint, QVariantMap)));
        m_pendingStep = step;
        return token;
    }
    void createSession() {
        const QString token = expectResponse("session");
        QDBusMessage m = QDBusMessage::createMethodCall(kService, kPath, kIface, "CreateSession");
        m << QVariantMap{{"handle_token", token}, {"session_handle_token", "micforge_session"}};
        QDBusConnection::sessionBus().asyncCall(m);
        m_status = tr("Waiting for the desktop…");
        changed();
    }
    static bool sameIds(const QVector<Shortcut> &a, const QVector<Shortcut> &b) {
        if (a.size() != b.size()) return false;
        for (int i = 0; i < a.size(); ++i) if (a[i].id != b[i].id || a[i].description != b[i].description) return false;
        return true;
    }
    void bindNow() {
        m_sent = m_wanted;
        QList<PortalShortcut> list;
        for (const Shortcut &s : m_wanted) {
            QVariantMap props{{"description", s.description}};
            if (!s.preferredTrigger.isEmpty()) props["preferred_trigger"] = s.preferredTrigger;
            list << PortalShortcut{s.id, props};
        }
        const QString token = expectResponse("bind");
        QDBusMessage m = QDBusMessage::createMethodCall(kService, kPath, kIface, "BindShortcuts");
        m << QVariant::fromValue(QDBusObjectPath(m_session)) << QVariant::fromValue(list) << QString()
          << QVariantMap{{"handle_token", token}};
        QDBusConnection::sessionBus().asyncCall(m);
        m_status = tr("Confirm the shortcuts in the desktop's dialog…");
        changed();
    }
    void readTriggers(const QDBusArgument &arg) {
        QList<PortalShortcut> list;
        arg >> list;
        m_triggers.clear();
        for (const auto &s : list) m_triggers[s.id] = s.props.value("trigger_description").toString();
        changed();
    }
    void changed() { if (onChanged) onChanged(); }

    uint m_version = 0;
    bool m_bound = false, m_rebind = false;
    QString m_session, m_pendingStep, m_status;
    QVector<Shortcut> m_wanted, m_sent;
    QMap<QString, QString> m_triggers;
};

} // namespace

std::unique_ptr<IGlobalShortcuts> createGlobalShortcuts() {
    if (auto &f = IGlobalShortcuts::factoryOverride()) return f();
    return std::make_unique<PortalShortcuts>();
}

} // namespace mf

#include "globalshortcuts_portal.moc"
