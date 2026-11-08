#include "imagestore.h"

#include <QBuffer>
#include <QImage>
#include <QMutexLocker>

namespace psg {

void ImageStore::setImage(const QString &key, const QByteArray &bytes)
{
    QMutexLocker lock(&m_mutex);
    m_images.insert(key, bytes);
    ++m_generation;
}

void ImageStore::setImages(const QString &prefix, const QList<QByteArray> &list)
{
    QMutexLocker lock(&m_mutex);
    for (int i = 0; i < list.size(); ++i)
        m_images.insert(QStringLiteral("%1/%2").arg(prefix).arg(i), list.at(i));
    ++m_generation;
}

void ImageStore::clear()
{
    QMutexLocker lock(&m_mutex);
    m_images.clear();
    ++m_generation;
}

QByteArray ImageStore::image(const QString &key) const
{
    QMutexLocker lock(&m_mutex);
    return m_images.value(key);
}

quint64 ImageStore::generation() const
{
    QMutexLocker lock(&m_mutex);
    return m_generation;
}

PsgImageProvider::PsgImageProvider(ImageStore *store)
    : QQuickImageProvider(QQuickImageProvider::Image)
    , m_store(store)
{
}

QImage PsgImageProvider::requestImage(const QString &id, QSize *size,
                                      const QSize &requestedSize)
{
    if (!m_store)
        return QImage();
    QMutexLocker cacheLock(&m_cacheMutex);
    // small bounded decode cache: QML disables its own pixmap cache for
    // these images, so without this every delegate (re)creation - each
    // model change, each scroll past the cache buffer - re-decoded the
    // full-resolution PNG on this thread
    const quint64 gen = m_store->generation();
    if (gen != m_cacheGen) {
        m_cache.clear();
        m_order.clear();
        m_cacheGen = gen;
    }
    const auto hit = m_cache.constFind(id);
    if (hit != m_cache.cend()) {
        if (size)
            *size = hit.value().size();
        return hit.value();
    }
    // the id arrives as "<key>?<query>" when sourceSize is used
    const QString key = id.section(u'?', 0, 0);
    const QByteArray bytes = m_store->image(key);
    if (bytes.isEmpty())
        return QImage();
    QImage img;
    if (!img.loadFromData(bytes))
        return QImage();
    if (requestedSize.isValid() && requestedSize != img.size())
        img = img.scaled(requestedSize, Qt::KeepAspectRatio,
                         Qt::SmoothTransformation);
    if (size)
        *size = img.size();
    if (!img.isNull()) {
        while (m_order.size() >= kDecodeCacheMax) {
            m_cache.remove(m_order.takeFirst());
        }
        m_order.append(id);
        m_cache.insert(id, img);
    }
    return img;
}

} // namespace psg
