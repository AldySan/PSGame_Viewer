#pragma once

#include <QQuickImageProvider>

#include <QHash>
#include <QList>
#include <QMutex>

namespace psg {

// Serves in-memory images to QML: the cover art ("psg/image/cover"),
// gallery pictures ("psg/image/gallery/<n>") and trophy icons
// ("psg/image/trophy/<n>"). The byte arrays are owned by the viewer
// controller; the provider only converts them to QImage.
//
// requestImage() runs on the QML pixmap reader thread while the GUI
// thread inserts/clears images as files are opened, so every access is
// guarded by a mutex: an unguarded QHash read during a concurrent
// insert/clear is a data race and crashes the renderer.
class ImageStore
{
public:
    void setImage(const QString &key, const QByteArray &bytes);
    void setImages(const QString &prefix, const QList<QByteArray> &list);
    void clear();

    QByteArray image(const QString &key) const;
    // bumped on every mutation; the provider keys its decode cache on it
    // so stale decodes never outlive the bytes they came from
    quint64 generation() const;

private:
    mutable QMutex m_mutex;
    QHash<QString, QByteArray> m_images;
    quint64 m_generation = 0;
};

class PsgImageProvider : public QQuickImageProvider
{
public:
    explicit PsgImageProvider(ImageStore *store);

    QImage requestImage(const QString &id, QSize *size,
                        const QSize &requestedSize) override;

private:
    // bounded FIFO cache of decoded (already scaled) results
    static constexpr int kDecodeCacheMax = 128;

    ImageStore *m_store;
    QMutex m_cacheMutex;
    QHash<QString, QImage> m_cache;
    QList<QString> m_order;
    quint64 m_cacheGen = 0;
};

} // namespace psg
