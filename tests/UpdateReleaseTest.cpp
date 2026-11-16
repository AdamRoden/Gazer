#include "app/UpdateRelease.h"

#include <QtTest>

using namespace gazer;

class UpdateReleaseTest final : public QObject {
    Q_OBJECT

private slots:
    void parsesTripletFromTagAndAsset();
    void ordersVersions();
    void picksNewestMsiAndSkipsDrafts();
};

void UpdateReleaseTest::parsesTripletFromTagAndAsset()
{
    const VersionTriplet plain = parseVersionTriplet(QStringLiteral("0.7.3"));
    QVERIFY(plain.ok);
    QCOMPARE(plain.major, 0);
    QCOMPARE(plain.minor, 7);
    QCOMPARE(plain.patch, 3);

    const VersionTriplet beta = parseVersionTriplet(QStringLiteral("v0.8.0-beta"));
    QVERIFY(beta.ok);
    QCOMPARE(beta.major, 0);
    QCOMPARE(beta.minor, 8);
    QCOMPARE(beta.patch, 0);

    const VersionTriplet asset = parseVersionTriplet(QStringLiteral("Gazer-1.2.9-beta.msi"));
    QVERIFY(asset.ok);
    QCOMPARE(asset.patch, 9);
    QVERIFY(!parseVersionTriplet(QStringLiteral("notes")).ok);
}

void UpdateReleaseTest::ordersVersions()
{
    const VersionTriplet older = parseVersionTriplet(QStringLiteral("0.7.3"));
    const VersionTriplet same = parseVersionTriplet(QStringLiteral("0.7.3-beta"));
    const VersionTriplet newer = parseVersionTriplet(QStringLiteral("0.7.4"));
    QCOMPARE(compareVersions(older, same), 0);
    QVERIFY(compareVersions(older, newer) < 0);
    QVERIFY(compareVersions(newer, older) > 0);
}

void UpdateReleaseTest::picksNewestMsiAndSkipsDrafts()
{
    const QByteArray json = R"([
        {"draft": true, "prerelease": false, "assets": [
            {"name": "Gazer-9.0.0.msi", "browser_download_url": "https://example.test/draft.msi"}
        ]},
        {"draft": false, "prerelease": true, "assets": [
            {"name": "Gazer-0.7.3-beta.msi", "browser_download_url": "https://example.test/old.msi"},
            {"name": "notes.txt", "browser_download_url": "https://example.test/notes"}
        ]},
        {"draft": false, "prerelease": true, "assets": [
            {"name": "Gazer-0.7.5-beta.msi", "browser_download_url": "https://example.test/beta.msi"}
        ]},
        {"draft": false, "prerelease": false, "assets": [
            {"name": "Gazer-0.7.4.msi", "browser_download_url": "https://example.test/stable.msi"},
            {"name": "Gazer-0.7.5.msi", "browser_download_url": "https://example.test/same.msi"}
        ]}
    ])";

    const GazerMsiRelease found = newestGazerMsi(json, QStringLiteral("0.7.3"));
    QVERIFY(found.found);
    QCOMPARE(found.version, QStringLiteral("0.7.5"));
    QCOMPARE(found.name, QStringLiteral("Gazer-0.7.5.msi"));
    QCOMPARE(found.url, QStringLiteral("https://example.test/same.msi"));

    const GazerMsiRelease current = newestGazerMsi(json, QStringLiteral("0.7.5"));
    QVERIFY(!current.found);

    const GazerMsiRelease betaOnly = newestGazerMsi(json, QStringLiteral("0.7.4"));
    QVERIFY(betaOnly.found);
    QCOMPARE(betaOnly.version, QStringLiteral("0.7.5"));
}

QObject* createUpdateReleaseTest()
{
    return new UpdateReleaseTest;
}

#include "UpdateReleaseTest.moc"
