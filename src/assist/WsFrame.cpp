#include "assist/WsFrame.h"

namespace gazer::WsFrame {
namespace {

void appendBe(QByteArray* out, quint64 value, int bytes)
{
    for (int shift = (bytes - 1) * 8; shift >= 0; shift -= 8) {
        out->append(char((value >> shift) & 0xFF));
    }
}

} // namespace

QByteArray clientFrame(quint8 opcode, const QByteArray& payload, quint32 maskKey)
{
    QByteArray out;
    out.reserve(14 + payload.size());
    out.append(char(0x80 | (opcode & 0x0F)));
    const quint64 n = quint64(payload.size());
    if (n < 126) {
        out.append(char(0x80 | int(n)));
    } else if (n <= 0xFFFF) {
        out.append(char(0x80 | 126));
        appendBe(&out, n, 2);
    } else {
        out.append(char(0x80 | 127));
        appendBe(&out, n, 8);
    }
    const uchar mask[4] = {uchar((maskKey >> 24) & 0xFF), uchar((maskKey >> 16) & 0xFF),
                           uchar((maskKey >> 8) & 0xFF), uchar(maskKey & 0xFF)};
    out.append(reinterpret_cast<const char*>(mask), 4);
    for (int i = 0; i < payload.size(); ++i) {
        out.append(char(uchar(payload.at(i)) ^ mask[i & 3]));
    }
    return out;
}

Incoming serverFrame(const QByteArray& buffer)
{
    Incoming in;
    if (buffer.size() < 2) {
        return in;
    }
    const uchar b0 = uchar(buffer.at(0));
    const uchar b1 = uchar(buffer.at(1));
    if ((b0 & 0x70) != 0) {
        in.status = Incoming::Status::Error;
        return in;
    }
    in.fin = (b0 & 0x80) != 0;
    in.opcode = b0 & 0x0F;
    switch (in.opcode) {
    case kOpCont:
    case kOpText:
    case kOpBinary:
    case kOpClose:
    case kOpPing:
    case kOpPong:
        break;
    default:
        in.status = Incoming::Status::Error;
        return in;
    }
    const bool masked = (b1 & 0x80) != 0;
    quint64 len = b1 & 0x7F;
    int pos = 2;
    if (len == 126) {
        if (buffer.size() < pos + 2) {
            return in;
        }
        len = (quint64(uchar(buffer.at(pos))) << 8) | uchar(buffer.at(pos + 1));
        pos += 2;
    } else if (len == 127) {
        if (buffer.size() < pos + 8) {
            return in;
        }
        len = 0;
        for (int i = 0; i < 8; ++i) {
            len = (len << 8) | uchar(buffer.at(pos + i));
        }
        pos += 8;
        if (len > (quint64(1) << 31)) {
            in.status = Incoming::Status::Error;
            return in;
        }
    }
    uchar mask[4] = {};
    if (masked) {
        if (buffer.size() < pos + 4) {
            return in;
        }
        for (int i = 0; i < 4; ++i) {
            mask[i] = uchar(buffer.at(pos + i));
        }
        pos += 4;
    }
    if (quint64(buffer.size() - pos) < len) {
        return in;
    }
    in.payload.resize(int(len));
    for (quint64 i = 0; i < len; ++i) {
        uchar c = uchar(buffer.at(pos + int(i)));
        if (masked) {
            c ^= mask[i & 3];
        }
        in.payload[int(i)] = char(c);
    }
    in.consumed = pos + int(len);
    in.status = Incoming::Status::Ok;
    return in;
}

} // namespace gazer::WsFrame
