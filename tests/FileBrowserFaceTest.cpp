#include "app/ComposeFilesDetail.h"

#include <QTemporaryDir>
#include <QFile>
#include <QtTest>

using namespace gazer;

class FileBrowserFaceTest final : public QObject {
    Q_OBJECT

private slots:
    void historySaveIsNotAWorkspaceExport();
    void speakAndSettingsKeepJson();
    void topicExportIsOneWorkspaceFile();
    void audioUsesTheCallerFilters();
    void clipLeafDropsReservedNames();
    void typedFileMatchesItsSuffix();
};

void FileBrowserFaceTest::historySaveIsNotAWorkspaceExport()
{
    const files_detail::BrowserFace face = files_detail::browserFace(
        files_detail::BrowserJob::HistoryClip, files_detail::BrowserMode::Export, {});
    QCOMPARE(face.writer, files_detail::BrowserFace::Writer::Clip);
    QCOMPARE(face.exportSuffix, QStringLiteral(".mp3"));
    QCOMPARE(face.patterns, QStringList{QStringLiteral("*.mp3")});
    QCOMPARE(face.documentName, QStringLiteral("Save MP3"));
    QVERIFY(face.duringItemEdit);
    QVERIFY(face.ownsEnter);
    QCOMPARE(face.caption, QStringLiteral("Save MP3"));
    QCOMPARE(face.emptyFolder, QStringLiteral("No folders or MP3 files"));
    QCOMPARE(face.dwellHint, QStringLiteral("Dwell a folder or an MP3 file"));
    QCOMPARE(face.selection, files_detail::BrowserFace::Selection::Typed);
}

void FileBrowserFaceTest::speakAndSettingsKeepJson()
{
    const files_detail::BrowserFace speak = files_detail::browserFace(
        files_detail::BrowserJob::Speak, files_detail::BrowserMode::Export, {});
    QCOMPARE(speak.writer, files_detail::BrowserFace::Writer::Workspace);
    QCOMPARE(speak.exportSuffix, QStringLiteral(".json"));
    QCOMPARE(speak.documentName, QStringLiteral("Export"));
    QVERIFY(speak.caption.isEmpty());
    QVERIFY(!speak.duringItemEdit);
    QVERIFY(!speak.ownsEnter);
    QCOMPARE(speak.patterns, QStringList{QStringLiteral("*.json")});

    const files_detail::BrowserFace settings = files_detail::browserFace(
        files_detail::BrowserJob::Settings, files_detail::BrowserMode::Import, {});
    QCOMPARE(settings.writer, files_detail::BrowserFace::Writer::Settings);
    QCOMPARE(settings.documentName, QStringLiteral("Import settings"));
    QCOMPARE(settings.caption, QStringLiteral("Import settings"));
    QCOMPARE(settings.exportSuffix, QStringLiteral(".json"));
}

void FileBrowserFaceTest::topicExportIsOneWorkspaceFile()
{
    const files_detail::BrowserFace face = files_detail::browserFace(
        files_detail::BrowserJob::Topic, files_detail::BrowserMode::Export, {});
    QCOMPARE(face.writer, files_detail::BrowserFace::Writer::Topic);
    QCOMPARE(face.exportSuffix, QStringLiteral(".json"));
    QCOMPARE(face.documentName, QStringLiteral("Export"));
    QCOMPARE(face.patterns, QStringList{QStringLiteral("*.json")});
    QVERIFY(face.duringItemEdit);
    QVERIFY(face.ownsEnter);
}

void FileBrowserFaceTest::audioUsesTheCallerFilters()
{
    const QStringList filters{QStringLiteral("*.wav"), QStringLiteral("*.mp3")};
    const files_detail::BrowserFace face = files_detail::browserFace(
        files_detail::BrowserJob::Audio, files_detail::BrowserMode::Import, filters);
    QCOMPARE(face.patterns, filters);
    QCOMPARE(face.writer, files_detail::BrowserFace::Writer::Workspace);
    QCOMPARE(face.selection, files_detail::BrowserFace::Selection::Audio);
    QCOMPARE(face.documentName, QStringLiteral("Audio"));
    QCOMPARE(face.caption, QStringLiteral("Choose audio"));
    QCOMPARE(face.emptyFolder, QStringLiteral("No folders or audio files"));
    QVERIFY(face.duringItemEdit);
    QVERIFY(!face.ownsEnter);
}

void FileBrowserFaceTest::clipLeafDropsReservedNames()
{
    QCOMPARE(files_detail::historyClipLeaf(QStringLiteral("Hello, world!")),
             QStringLiteral("Hello-world.mp3"));
    QCOMPARE(files_detail::historyClipLeaf(QStringLiteral("file/name")),
             QStringLiteral("file-name.mp3"));
    QVERIFY(files_detail::historyClipLeaf(QStringLiteral("CON")).startsWith(QStringLiteral("gazer-clip-")));
    QVERIFY(files_detail::historyClipLeaf(QString()).endsWith(QStringLiteral(".mp3")));
    QCOMPARE(files_detail::exportStem(QStringLiteral("Hello, world!"), QStringLiteral("gazer-topic")),
             QStringLiteral("Hello-world"));
    QVERIFY(files_detail::exportStem(QStringLiteral("CON"), QStringLiteral("gazer-topic"))
                .startsWith(QStringLiteral("gazer-topic-")));
    QVERIFY(files_detail::reservedWindowsStem(QStringLiteral("lpt2")));
    QVERIFY(!files_detail::reservedWindowsStem(QStringLiteral("computer")));
}

void FileBrowserFaceTest::typedFileMatchesItsSuffix()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString mp3 = dir.filePath(QStringLiteral("clip.mp3"));
    QFile file(mp3);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QCOMPARE(file.write("x"), qint64(1));
    file.close();
    QCOMPARE(files_detail::fileInDir(dir.path(), QStringLiteral("clip"), QStringLiteral(".mp3")), mp3);
    QVERIFY(files_detail::fileInDir(dir.path(), QStringLiteral("clip"), QStringLiteral(".json")).isEmpty());
    QVERIFY(files_detail::fileInDir(dir.path(), QStringLiteral("missing"), QStringLiteral(".mp3")).isEmpty());
}

QObject* createFileBrowserFaceTest()
{
    return new FileBrowserFaceTest;
}

#include "FileBrowserFaceTest.moc"
