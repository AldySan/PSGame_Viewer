#pragma once

#include <QQuickImageProvider>

namespace psg {

// Serves in-memory images to QML: the cover art ("psg/image/cover"),
// gallery pictures ("psg/image/gallery/<n>") and trophy icons
// ("psg/image/trophy/<n>"). The byte arrays are owned by the viewer
// controller; the provider only converts them to QImage.
class ImageStore
{
public:
    void setImage(const QString &key, const QByteArray &bytes);
    void setImages(const QString &prefix, const QList<QByteArray> &list);
    void clear();

    QByteArray image(const QString &key) const;

private:
    QHash<QString, QByteArray> m_images;
};

class PsgImageProvider : public QQuickImageProvider
{
public:
    explicit PsgImageProvider(ImageStore *store);

    QImage requestImage(const QString &id, QSize *size,
                        const QSize &requestedSize) override;

private:
    ImageStore *m_store;
};

} // namespace psg
