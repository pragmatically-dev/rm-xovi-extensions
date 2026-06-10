// hot-reload-poc — drop a .qmd into a folder on the tablet, see it live.
//
// A minimal proof-of-concept for the hot-reload exports of the (hot-reload
// branch) qt-resource-rebuilder. It watches /home/root/hot-reload for .qmd
// files; whenever one appears or changes it:
//
//   1. calls qt-resource-rebuilder$qrr_reload_external_diff(id, contents)
//      (id = the file name), which swaps the diff inside qmldiff and
//      re-registers ONLY the resource roots that diff targets;
//   2. clears the QML component cache so the now-unreferenced compilation
//      units can be evicted;
//   3. recompiles every URL-`source` Loader whose source matches one of the
//      diff's AFFECT targets, by toggling the loader's own `sourceUrl` —
//      xochitl's ToolLoader re-fires setSource(url, {required props}) itself,
//      which a bare `source=` assignment would miss.
//
// Usage (from your machine):   scp demo/tag-icon.qmd root@10.11.99.1:/home/root/hot-reload/
// The Tags tool icon changes on screen, no restart. Edit the icon name in the
// .qmd, scp again, it changes again.
//
// Scope: components loaded through URL-`source` Loaders (toolbar tools/menus)
// update visibly in place. Content inlined into long-lived views needs the
// caller to restructure it behind a reloadable Loader (out of scope here —
// this PoC only demonstrates the resource layer + the simplest UI bounce).
//
// xovi loads this before main(), so _xovi_construct() spins up a thread that
// waits for the QGuiApplication and hops onto the GUI thread; the watcher and
// every QQuickItem access live there.

#include <atomic>
#include <thread>

#include <dlfcn.h>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileSystemWatcher>
#include <QGuiApplication>
#include <QHash>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QRegularExpression>
#include <QTimer>
#include <QUrl>
#include <QWindow>

extern "C" {
#include "xovi.h"
}

namespace {

const char *kWatchDir = "/home/root/hot-reload";

QQuickItem *rootItem() {
    const auto windows = QGuiApplication::allWindows();
    for (QWindow *w : windows)
        if (auto *qw = qobject_cast<QQuickWindow *>(w); qw && qw->isVisible())
            return qw->contentItem();
    for (QWindow *w : windows)
        if (auto *qw = qobject_cast<QQuickWindow *>(w))
            return qw->contentItem();
    return nullptr;
}

void collectLoaders(QQuickItem *item, const QString &suffix, QList<QQuickItem *> &out) {
    if (item->inherits("QQuickLoader")) {
        const QUrl src = item->property("source").toUrl();
        if (!src.isEmpty() && src.toString().endsWith(suffix))
            out.append(item);
    }
    const auto children = item->childItems();
    for (QQuickItem *c : children)
        collectLoaders(c, suffix, out);
}

// The on-device-proven recipe: unload -> evict -> re-fire the loader's own
// setSource via its `sourceUrl` property (do NOT call setSource from C++).
int bounceLoaders(const QString &suffix) {
    QQuickItem *root = rootItem();
    if (!root)
        return 0;
    QList<QQuickItem *> loaders;
    collectLoaders(root, suffix, loaders);
    int bounced = 0;
    for (QQuickItem *loader : loaders) {
        const QUrl url = loader->property("source").toUrl();
        QQmlEngine *engine = qmlEngine(loader);
        if (!engine)
            continue;
        loader->setProperty("source", QUrl());                 // drop the unit ref
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        engine->clearComponentCache();                         // evict it
        QCoreApplication::processEvents(QEventLoop::AllEvents, 30);
        if (loader->property("sourceUrl").isValid()) {
            loader->setProperty("sourceUrl", QString());       // re-fire QML's own
            loader->setProperty("sourceUrl", url.toString());  // setSource(url, props)
        } else {
            loader->setProperty("source", url);                // plain URL loader
        }
        QCoreApplication::processEvents(QEventLoop::AllEvents, 30);
        bounced++;
    }
    return bounced;
}

class HotReloadWatcher : public QObject {
public:
    explicit HotReloadWatcher(QObject *parent = nullptr) : QObject(parent) {
        QDir().mkpath(kWatchDir);
        m_watcher.addPath(kWatchDir);
        connect(&m_watcher, &QFileSystemWatcher::directoryChanged,
                this, &HotReloadWatcher::scheduleScan);
        // editors/scp replace files, which can drop file watches — the
        // directory watch + rescan covers every case.
        m_debounce.setSingleShot(true);
        m_debounce.setInterval(250);
        connect(&m_debounce, &QTimer::timeout, this, &HotReloadWatcher::scan);
        fprintf(stderr, "[hot-reload-poc] watching %s — drop a .qmd there\n", kWatchDir);
        scan();
    }

private:
    void scheduleScan() { m_debounce.start(); }

    void scan() {
        const auto entries = QDir(kWatchDir).entryInfoList({"*.qmd"}, QDir::Files);
        for (const QFileInfo &fi : entries) {
            const QString sig = QString("%1:%2")
                .arg(fi.lastModified().toMSecsSinceEpoch()).arg(fi.size());
            if (m_seen.value(fi.filePath()) == sig)
                continue;
            m_seen.insert(fi.filePath(), sig);
            apply(fi);
        }
    }

    void apply(const QFileInfo &fi) {
        QFile f(fi.filePath());
        if (!f.open(QIODevice::ReadOnly))
            return;
        const QByteArray contents = f.readAll();
        const QByteArray id = fi.fileName().toUtf8();

        // xovigen types imports as zero-arg pointers; recast to the real signature.
        auto reload = (int (*)(const char *, const char *))
            qt_resource_rebuilder$qrr_reload_external_diff;
        const int roots = reload(id.constData(), contents.constData());
        if (roots < 0) {
            fprintf(stderr, "[hot-reload-poc] %s: parse FAILED — previous diff kept\n",
                    id.constData());
            return;
        }

        // Recompile the URL-Loaders that render the AFFECTed files.
        static const QRegularExpression affectRe(
            QStringLiteral("^\\s*AFFECT\\s+(\\S+)"), QRegularExpression::MultilineOption);
        int bounced = 0;
        auto it = affectRe.globalMatch(QString::fromUtf8(contents));
        while (it.hasNext()) {
            const QString target = it.next().captured(1);
            bounced += bounceLoaders(target.section('/', -1));
        }
        fprintf(stderr, "[hot-reload-poc] %s: %d root(s) re-registered, "
                        "%d loader(s) recompiled\n", id.constData(), roots, bounced);
    }

    QFileSystemWatcher m_watcher;
    QTimer m_debounce;
    QHash<QString, QString> m_seen;
};

std::atomic<bool> started{false};

void startOnGuiThread() {
    new HotReloadWatcher(QCoreApplication::instance());
}

}  // namespace

extern "C" void _xovi_construct() {
    // No QGuiApplication exists yet (xovi runs before main); wait for it.
    std::thread([] {
        while (QCoreApplication::instance() == nullptr)
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if (started.exchange(true))
            return;
        QMetaObject::invokeMethod(QCoreApplication::instance(), &startOnGuiThread,
                                  Qt::QueuedConnection);
    }).detach();
}

extern "C" char _xovi_shouldLoad() {
    // Only attach to GUI applications (the same guard qt-command-executor uses).
    return dlsym(RTLD_DEFAULT, "_Z21qRegisterResourceDataiPKhS0_S0_") != nullptr;
}
