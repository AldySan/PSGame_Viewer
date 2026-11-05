#include "imagestore.h"

#include <QBuffer>
#include <QImage>
#include <QMutexLocker>

namespace psg {

void ImageStore::setImage(const QString &key, const QByteArray &bytes)
{
    QMutexLocker lock(&m_mutex);
    m_images.insert(key, bytes);
}

void ImageStore::setImages(const QString &prefix, const QList<QByteArray> &list)
{
    QMutexLocker lock(&m_mutex);
    for (int i = 0; i < list.size(); ++i)
        m_images.insert(QStringLiteral("%1/%2").arg(prefix).arg(i), list.at(i));
}

void ImageStore::clear()
{
    QMutexLocker lock(&m_mutex);
    m_images.clear();
}

QByteArray ImageStore::image(const QString &key) const
{
    QMutexLocker lock(&m_mutex);
    return m_images.value(key);
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
    return img;
}

} // namespace psg
