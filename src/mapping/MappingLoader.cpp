#include "mapping/MappingLoader.h"

#include "utils/Log.h"

#include <QFile>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace gazer {

namespace {

InputOutput::Type parseType(const QString& s)
{
    static const QHash<QString, InputOutput::Type> kTypes = {
        {QStringLiteral("keyTap"), InputOutput::Type::KeyTap},
        {QStringLiteral("keyCombo"), InputOutput::Type::KeyCombo},
        {QStringLiteral("text"), InputOutput::Type::Text},
        {QStringLiteral("mouseClick"), InputOutput::Type::MouseClick},
        {QStringLiteral("mouseDoubleClick"), InputOutput::Type::MouseDoubleClick},
        {QStringLiteral("mouseDown"), InputOutput::Type::MouseDown},
        {QStringLiteral("mouseUp"), InputOutput::Type::MouseUp},
        {QStringLiteral("mouseMove"), InputOutput::Type::MouseMove},
        {QStringLiteral("mouseMoveTo"), InputOutput::Type::MouseMoveTo},
        {QStringLiteral("mouseScroll"), InputOutput::Type::MouseScroll},
        {QStringLiteral("mouseScrollH"), InputOutput::Type::MouseScrollH},
        {QStringLiteral("gamepadButton"), InputOutput::Type::GamepadButton},
        {QStringLiteral("gamepadAxis"), InputOutput::Type::GamepadAxis},
    };
    return kTypes.value(s, InputOutput::Type::Unknown);
}

bool parseOutput(const QJsonObject& obj, InputOutput& out, QString* error)
{
    out.type = parseType(obj.value(QStringLiteral("type")).toString());
    if (out.type == InputOutput::Type::Unknown) {
        if (error) {
            *error = QStringLiteral("Unknown output type: %1")
                         .arg(obj.value(QStringLiteral("type")).toString());
        }
        return false;
    }
    out.key = obj.value(QStringLiteral("key")).toString();
    out.value = obj.value(QStringLiteral("value")).toString();
    out.button = obj.value(QStringLiteral("button")).toString();
    out.axis = obj.value(QStringLiteral("axis")).toString();
    out.dx = obj.value(QStringLiteral("dx")).toInt(0);
    out.dy = obj.value(QStringLiteral("dy")).toInt(0);
    out.notches = obj.value(QStringLiteral("notches")).toInt(0);
    out.axisValue = obj.value(QStringLiteral("value")).toDouble(0.0);
    if (out.type == InputOutput::Type::Text) {
        out.value = obj.value(QStringLiteral("value")).toString();
    }
    if (out.type == InputOutput::Type::GamepadAxis) {
        out.axisValue = obj.value(QStringLiteral("axisValue")).toDouble(
            obj.value(QStringLiteral("value")).toDouble(0.0));
    }

    const QJsonArray keys = obj.value(QStringLiteral("keys")).toArray();
    for (const QJsonValue& v : keys) {
        out.keys.push_back(v.toString());
    }

    if (out.type == InputOutput::Type::KeyTap && out.key.isEmpty()) {
        if (error) {
            *error = QStringLiteral("keyTap requires key");
        }
        return false;
    }
    if (out.type == InputOutput::Type::KeyCombo && out.keys.isEmpty()) {
        if (error) {
            *error = QStringLiteral("keyCombo requires keys[]");
        }
        return false;
    }
    if (out.type == InputOutput::Type::Text && out.value.isEmpty()) {
        if (error) {
            *error = QStringLiteral("text requires value");
        }
        return false;
    }
    return true;
}

} // namespace

bool MappingLoader::loadFromFile(const QString& path, MappingProfile& out, QString* error)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        if (error) {
            *error = QStringLiteral("Cannot open mapping: %1").arg(path);
        }
        return false;
    }
    return loadFromJson(f.readAll(), out, error);
}

bool MappingLoader::loadFromJson(const QByteArray& json, MappingProfile& out, QString* error)
{
    QJsonParseError pe{};
    const QJsonDocument doc = QJsonDocument::fromJson(json, &pe);
    if (pe.error != QJsonParseError::NoError || !doc.isObject()) {
        if (error) {
            *error = QStringLiteral("JSON parse error: %1").arg(pe.errorString());
        }
        return false;
    }

    const QJsonObject root = doc.object();
    MappingProfile profile;
    profile.schemaVersion = root.value(QStringLiteral("schemaVersion")).toInt(1);
    profile.id = root.value(QStringLiteral("id")).toString();
    profile.name = root.value(QStringLiteral("name")).toString();
    if (profile.id.isEmpty()) {
        if (error) {
            *error = QStringLiteral("Mapping missing id");
        }
        return false;
    }

    const QJsonObject commands = root.value(QStringLiteral("commands")).toObject();
    for (auto it = commands.begin(); it != commands.end(); ++it) {
        if (!it.value().isArray()) {
            continue;
        }
        QVector<InputOutput> list;
        for (const QJsonValue& v : it.value().toArray()) {
            if (!v.isObject()) {
                continue;
            }
            InputOutput o;
            if (!parseOutput(v.toObject(), o, error)) {
                return false;
            }
            list.push_back(std::move(o));
        }
        profile.commands.insert(it.key(), list);
    }

    out = std::move(profile);
    GAZER_INFO << "Loaded mapping profile" << out.id << "commands:" << out.commands.size();
    return true;
}

} // namespace gazer
