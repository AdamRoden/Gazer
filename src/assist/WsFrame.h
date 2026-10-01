#pragma once

#include <QByteArray>
#include <QtGlobal>

namespace gazer::WsFrame {

inline constexpr quint8 kOpCont = 0x0;
inline constexpr quint8 kOpText = 0x1;
inline constexpr quint8 kOpBinary = 0x2;
inline constexpr quint8 kOpClose = 0x8;
inline constexpr quint8 kOpPing = 0x9;
inline constexpr quint8 kOpPong = 0xA;

struct Incoming {
    enum class Status { NeedMore, Ok, Error };
    Status status = Status::NeedMore;
    bool fin = false;
    quint8 opcode = 0;
    QByteArray payload;
    int consumed = 0;
};

/// Masked client frame (RFC 6455). `maskKey` is the four mask bytes, big-endian.
[[nodiscard]] QByteArray clientFrame(quint8 opcode, const QByteArray& payload, quint32 maskKey);

/// One server frame. Masked frames are accepted and unmasked. RSV bits are an error.
[[nodiscard]] Incoming serverFrame(const QByteArray& buffer);

} // namespace gazer::WsFrame
