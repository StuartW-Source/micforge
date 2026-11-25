#pragma once

#include "plugins/plugins.h"

#include <QObject>
#include <QTimer>
#include <QVariantList>

#include <memory>
#include <vector>

// Manages the user's third-party plugins (VST3 / LV2): discovery, the loaded chain, parameters,
// saving/restoring, and safe hand-over of instances to the audio thread.
class PluginManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList available READ available NOTIFY availableChanged)
    Q_PROPERTY(bool scanning READ scanning NOTIFY scanningChanged)
    Q_PROPERTY(QString scanStatus READ scanStatus NOTIFY scanningChanged)
    Q_PROPERTY(QVariantList chain READ chain NOTIFY chainChanged)
    Q_PROPERTY(int revision READ revision NOTIFY valuesChanged) // bump = re-read values
    Q_PROPERTY(int maxPlugins READ maxPlugins CONSTANT)

public:
    PluginManager(mf::PluginRack *rack, double sampleRate, QObject *parent = nullptr);
    ~PluginManager() override;

    QVariantList available() const { return m_available; }
    bool scanning() const { return m_scanning; }
    QString scanStatus() const { return m_scanStatus; }
    QVariantList chain() const;
    int revision() const { return m_revision; }
    int maxPlugins() const { return mf::PluginRack::kSlots; }

    Q_INVOKABLE void rescan(bool force = false);
    Q_INVOKABLE bool add(const QString &format, const QString &id);
    Q_INVOKABLE void remove(int slot);
    Q_INVOKABLE void move(int slot, int delta);
    Q_INVOKABLE void setBypass(int slot, bool bypass);
    Q_INVOKABLE void retry(int slot);
    Q_INVOKABLE QVariantList params(int slot) const;
    Q_INVOKABLE double value(int slot, int index) const;
    Q_INVOKABLE void setValue(int slot, int index, double v);
    Q_INVOKABLE void resetValue(int slot, int index);
    Q_INVOKABLE QString text(int slot, int index, double v) const;

    void restore();  // load the saved chain (call once at startup)
    void save() const;

    // Used by "micforge --scan-vst3 <bundle>": prints the bundle's effects as JSON and returns.
    static int runScanHelper(const QString &bundle);

signals:
    void availableChanged();
    void scanningChanged();
    void chainChanged();
    void valuesChanged();

private:
    struct Entry {
        mf::PluginInfo info;
        std::unique_ptr<mf::PluginInstance> inst;
        QString error;
        bool bypass = false;
        QVariantMap saved; // parameter values by id, applied when (re)loaded
    };
    bool instantiate(Entry &e);
    void publish();       // hand the current instances to the audio thread
    void scanVst3Next();
    void finishScan();
    Entry *entry(int slot);
    const Entry *entry(int slot) const;

    mf::PluginRack *m_rack;
    double m_rate;
    std::vector<Entry> m_entries;
    QVariantList m_available;
    std::vector<mf::PluginInfo> m_found;
    std::vector<QString> m_pendingBundles;
    bool m_scanning = false;
    bool m_forceScan = false;
    QString m_scanStatus;
    int m_revision = 0;
    QTimer m_saveTimer;
};
