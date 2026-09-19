#include "input/VirtualGamepad.h"

#include "input/VigemLib.h"
#include "utils/Log.h"

#include <QHash>
#include <QtGlobal>
#include <memory>

namespace gazer {

namespace {

QString normalizeButton(QString name)
{
    name = name.trimmed().toLower();
    name.remove(QLatin1Char(' '));
    name.remove(QLatin1Char('_'));
    name.remove(QLatin1Char('-'));
    return name;
}

std::int16_t toStick(double v)
{
    v = qBound(-1.0, v, 1.0);
    if (v >= 0.0) {
        return static_cast<std::int16_t>(qRound(v * 32767.0));
    }
    return static_cast<std::int16_t>(qRound(v * 32768.0));
}

std::uint8_t toTrigger(double v)
{
    v = qBound(0.0, v, 1.0);
    return static_cast<std::uint8_t>(qRound(v * 255.0));
}

XusbReport toXusb(const VirtualGamepad::Report& r)
{
    XusbReport x;
    x.wButtons = r.buttons;
    x.bLeftTrigger = r.leftTrigger;
    x.bRightTrigger = r.rightTrigger;
    x.sThumbLX = r.leftX;
    x.sThumbLY = r.leftY;
    x.sThumbRX = r.rightX;
    x.sThumbRY = r.rightY;
    return x;
}

} // namespace

struct VirtualGamepad::Impl {
    bool connected = false;
    bool dryRun = false;
    QString backend = QStringLiteral("stub");
    Report report{};
    VigemLib lib;
    VigemLib::Client client = nullptr;
    VigemLib::Target target = nullptr;
};

VirtualGamepad::VirtualGamepad()
    : m(std::make_unique<Impl>())
{
}

VirtualGamepad::~VirtualGamepad()
{
    teardown();
}

bool VirtualGamepad::isConnected() const
{
    return m->connected;
}

QString VirtualGamepad::backendName() const
{
    return m->backend;
}

VirtualGamepad::Report VirtualGamepad::report() const
{
    return m->report;
}

void VirtualGamepad::setDryRun(bool on)
{
    m->dryRun = on;
}

void VirtualGamepad::setOverrideDll(const std::optional<QString>& path)
{
    m->lib.setOverridePath(path);
}

bool VirtualGamepad::lookupButton(const QString& name, std::uint16_t* bit, QString* error)
{
    static const QHash<QString, std::uint16_t> kBits = {
        {QStringLiteral("a"), 0x1000},
        {QStringLiteral("b"), 0x2000},
        {QStringLiteral("x"), 0x4000},
        {QStringLiteral("y"), 0x8000},
        {QStringLiteral("lb"), 0x0100},
        {QStringLiteral("leftshoulder"), 0x0100},
        {QStringLiteral("leftbumper"), 0x0100},
        {QStringLiteral("rb"), 0x0200},
        {QStringLiteral("rightshoulder"), 0x0200},
        {QStringLiteral("rightbumper"), 0x0200},
        {QStringLiteral("back"), 0x0020},
        {QStringLiteral("select"), 0x0020},
        {QStringLiteral("start"), 0x0010},
        {QStringLiteral("l3"), 0x0040},
        {QStringLiteral("ls"), 0x0040},
        {QStringLiteral("leftthumb"), 0x0040},
        {QStringLiteral("r3"), 0x0080},
        {QStringLiteral("rs"), 0x0080},
        {QStringLiteral("rightthumb"), 0x0080},
        {QStringLiteral("dpadup"), 0x0001},
        {QStringLiteral("up"), 0x0001},
        {QStringLiteral("dpaddown"), 0x0002},
        {QStringLiteral("down"), 0x0002},
        {QStringLiteral("dpadleft"), 0x0004},
        {QStringLiteral("left"), 0x0004},
        {QStringLiteral("dpadright"), 0x0008},
        {QStringLiteral("right"), 0x0008},
        {QStringLiteral("guide"), 0x0400},
        {QStringLiteral("xbox"), 0x0400},
    };
    const auto it = kBits.constFind(normalizeButton(name));
    if (it == kBits.cend()) {
        if (error) {
            *error = QStringLiteral("Unknown gamepad button '%1'").arg(name);
        }
        return false;
    }
    if (bit) {
        *bit = *it;
    }
    return true;
}

void VirtualGamepad::teardown()
{
    if (!m) {
        return;
    }
    if (m->lib.isLoaded() && m->client && m->target) {
        (void)m->lib.target_remove(m->client, m->target);
    }
    if (m->lib.isLoaded() && m->target && m->lib.target_free) {
        m->lib.target_free(m->target);
    }
    if (m->lib.isLoaded() && m->client) {
        if (m->lib.disconnect) {
            m->lib.disconnect(m->client);
        }
        if (m->lib.free) {
            m->lib.free(m->client);
        }
    }
    m->target = nullptr;
    m->client = nullptr;
    m->connected = false;
    if (!m->dryRun) {
        m->backend = QStringLiteral("stub");
    }
}

bool VirtualGamepad::pushReport(QString* error)
{
    if (m->dryRun) {
        return true;
    }
    if (!m->lib.isLoaded() || !m->client || !m->target) {
        if (error) {
            *error = QStringLiteral("Virtual gamepad is not connected");
        }
        return false;
    }
    const VigemLib::Error err = m->lib.target_x360_update(m->client, m->target, toXusb(m->report));
    if (err != VigemLib::kOk) {
        if (error) {
            *error = VigemLib::errorMessage(err);
        }
        return false;
    }
    return true;
}

bool VirtualGamepad::ensureConnected(QString* error)
{
    if (m->connected) {
        return true;
    }
    if (m->dryRun) {
        m->connected = true;
        m->backend = QStringLiteral("dry");
        m->report = {};
        return true;
    }
    QString err;
    if (!m->lib.load(&err)) {
        if (error) {
            *error = err;
        }
        return false;
    }
    m->client = m->lib.alloc();
    if (!m->client) {
        teardown();
        if (error) {
            *error = QStringLiteral("ViGEm alloc failed");
        }
        return false;
    }
    const VigemLib::Error ce = m->lib.connect(m->client);
    if (ce != VigemLib::kOk) {
        teardown();
        if (error) {
            *error = VigemLib::errorMessage(ce);
        }
        return false;
    }
    m->target = m->lib.target_x360_alloc();
    if (!m->target) {
        teardown();
        if (error) {
            *error = QStringLiteral("ViGEm pad alloc failed");
        }
        return false;
    }
    const VigemLib::Error ae = m->lib.target_add(m->client, m->target);
    if (ae != VigemLib::kOk) {
        teardown();
        if (error) {
            *error = VigemLib::errorMessage(ae);
        }
        return false;
    }
    m->report = {};
    m->connected = true;
    m->backend = QStringLiteral("ViGEm X360");
    GAZER_INFO << "Virtual gamepad connected" << m->lib.dllPath();
    return pushReport(error);
}

bool VirtualGamepad::pressButton(const QString& button, QString* error)
{
    if (!ensureConnected(error)) {
        return false;
    }
    std::uint16_t bit = 0;
    if (!lookupButton(button, &bit, error)) {
        return false;
    }
    m->report.buttons = static_cast<std::uint16_t>(m->report.buttons | bit);
    return pushReport(error);
}

bool VirtualGamepad::releaseButton(const QString& button, QString* error)
{
    if (!ensureConnected(error)) {
        return false;
    }
    std::uint16_t bit = 0;
    if (!lookupButton(button, &bit, error)) {
        return false;
    }
    m->report.buttons = static_cast<std::uint16_t>(m->report.buttons & ~bit);
    return pushReport(error);
}

bool VirtualGamepad::setAxis(const QString& axis, double value, QString* error)
{
    if (!ensureConnected(error)) {
        return false;
    }
    const QString a = axis.trimmed().toLower();
    if (a == QLatin1String("lx")) {
        m->report.leftX = toStick(value);
    } else if (a == QLatin1String("ly")) {
        m->report.leftY = toStick(value);
    } else if (a == QLatin1String("rx")) {
        m->report.rightX = toStick(value);
    } else if (a == QLatin1String("ry")) {
        m->report.rightY = toStick(value);
    } else if (a == QLatin1String("lt")) {
        m->report.leftTrigger = toTrigger(value);
    } else if (a == QLatin1String("rt")) {
        m->report.rightTrigger = toTrigger(value);
    } else {
        if (error) {
            *error = QStringLiteral("Unknown gamepad axis '%1' (lx ly rx ry lt rt)").arg(axis);
        }
        return false;
    }
    return pushReport(error);
}

} // namespace gazer
