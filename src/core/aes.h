#pragma once

#include <QByteArray>

namespace psg::aes {

// AES-128 block cipher, used for the PS3 NPDRM retail keystream and the
// trophy ESFM decryption. Only the raw block modes are exposed; padding
// is handled by the callers (both uses operate on exact 16-byte blocks).

class Aes128 {
public:
    explicit Aes128(const QByteArray &key); // 16 bytes

    void encryptBlock(const uchar *in, uchar *out) const;
    void decryptBlock(const uchar *in, uchar *out) const;

private:
    void expandKey(const uchar *key);
    uchar m_roundKeys[11][16];
};

// Raw ECB/CBC on 16-byte multiples. Return QByteArray() on invalid input.
QByteArray ecbEncrypt(const QByteArray &key, const QByteArray &data);
QByteArray cbcEncrypt(const QByteArray &key, const QByteArray &iv,
                      const QByteArray &data);
QByteArray cbcDecrypt(const QByteArray &key, const QByteArray &iv,
                      const QByteArray &data);

} // namespace psg::aes
