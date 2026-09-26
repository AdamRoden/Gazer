#include "predict/WordPredictor.h"

#include <QCoreApplication>
#include <QFile>
#include <QTextStream>

namespace {

bool readLines(const QString& path, QStringList* lines, QTextStream& err)
{
    QFile in(path);
    if (!in.open(QIODevice::ReadOnly | QIODevice::Text)) {
        err << "Cannot open " << path << "\n";
        return false;
    }
    while (!in.atEnd()) {
        const QString line = QString::fromUtf8(in.readLine()).trimmed();
        if (!line.isEmpty() && !line.startsWith(QLatin1Char('#'))) {
            lines->push_back(line);
        }
    }
    return true;
}

} // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QTextStream err(stderr);
    if (argc != 4) {
        err << "Usage: BuildPredictModel sentences.txt word-ranks.txt model.bin\n";
        return 2;
    }
    QStringList lines;
    if (!readLines(QString::fromLocal8Bit(argv[1]), &lines, err)) {
        return 1;
    }
    QStringList rankLines;
    if (!readLines(QString::fromLocal8Bit(argv[2]), &rankLines, err)) {
        return 1;
    }
    QVector<gazer::WordPredictor::UsageCount> usage;
    usage.reserve(rankLines.size());
    for (const QString& line : rankLines) {
        const int tab = line.indexOf(QLatin1Char('\t'));
        if (tab <= 0) {
            continue;
        }
        bool ok = false;
        const int count = line.mid(tab + 1).trimmed().toInt(&ok);
        if (!ok || count <= 0) {
            continue;
        }
        gazer::WordPredictor::UsageCount row;
        row.word = line.left(tab).trimmed();
        row.count = count;
        usage.push_back(row);
    }
    gazer::WordPredictor model;
    QString error;
    if (!model.buildFromSentences(lines, usage, &error)) {
        err << error << "\n";
        return 1;
    }
    if (!model.saveFile(QString::fromLocal8Bit(argv[3]), &error)) {
        err << error << "\n";
        return 1;
    }
    err << "words loaded from usage list: " << usage.size() << "\n";
    return 0;
}
