#include "editor/LayoutEditorWindow.h"

#include "editor/LayoutEditorCanvas.h"
#include "editor/LayoutEditorProperties.h"
#include "editor/LayoutEditorSession.h"
#include "layout/PageEdit.h"

#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHash>
#include <QLabel>
#include <QList>
#include <QPair>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSet>
#include <QStatusBar>
#include <QStringList>
#include <QVBoxLayout>

namespace gazer {

namespace {

constexpr int kBrowseResult = QDialog::Accepted + 1;
constexpr int kOpenListRows = 12;

bool isLayerSplitStem(const QString& stem, const QSet<QString>& stems)
{
    static const QStringList kSuffixes = {QStringLiteral("_sym_shift"), QStringLiteral("_shift"),
                                          QStringLiteral("_sym")};
    for (const QString& suffix : kSuffixes) {
        if (stem.size() > suffix.size() && stem.endsWith(suffix)
            && stems.contains(stem.left(stem.size() - suffix.size()))) {
            return true;
        }
    }
    return false;
}

QList<QPair<QString, QString>> openableLayoutFiles(const QString& userDir, const QString& layoutsDir)
{
    QHash<QString, QString> nameToPath;
    auto addDir = [&](const QString& dir) {
        if (dir.isEmpty()) {
            return;
        }
        const QFileInfoList files =
            QDir(dir).entryInfoList({QStringLiteral("*.xml")}, QDir::Files, QDir::Name);
        for (const QFileInfo& fi : files) {
            const QString key = fi.fileName().toLower();
            if (!nameToPath.contains(key)) {
                nameToPath.insert(key, fi.absoluteFilePath());
            }
        }
    };
    addDir(userDir);
    addDir(layoutsDir);
    QSet<QString> stems;
    for (auto it = nameToPath.cbegin(); it != nameToPath.cend(); ++it) {
        stems.insert(QFileInfo(it.value()).completeBaseName());
    }
    QList<QPair<QString, QString>> out;
    for (auto it = nameToPath.cbegin(); it != nameToPath.cend(); ++it) {
        const QFileInfo fi(it.value());
        if (isLayerSplitStem(fi.completeBaseName(), stems)) {
            continue;
        }
        out.append({fi.fileName(), it.value()});
    }
    return out;
}

} // namespace

bool LayoutEditorWindow::openFile(const QString& path, QString* error)
{
    if (!maybeSave()) {
        return false;
    }
    const bool ok = m_session->loadFromFile(path, error);
    if (ok) {
        frameLoadedPage();
    }
    return ok;
}

bool LayoutEditorWindow::openLayoutId(const QString& layoutId, QString* error)
{
    if (layoutId.isEmpty()) {
        if (error) {
            *error = QStringLiteral("Page id is empty");
        }
        return false;
    }
    const QString userPath = QDir(m_userDir).filePath(layoutId + QStringLiteral(".xml"));
    const QString shipPath = QDir(m_layoutsDir).filePath(layoutId + QStringLiteral(".xml"));
    const QString path = QFileInfo::exists(userPath) ? userPath : shipPath;
    return openFile(path, error);
}

bool LayoutEditorWindow::maybeSave()
{
    if (!applyCodeView()) {
        return false;
    }
    if (!m_session->isDirty()) {
        return true;
    }
    const auto r = QMessageBox::question(
        this, QStringLiteral("Unsaved page"),
        QStringLiteral("Save changes to %1?").arg(m_session->document().id),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save);
    if (r == QMessageBox::Cancel) {
        return false;
    }
    if (r == QMessageBox::Discard) {
        return true;
    }
    save();
    return !m_session->isDirty();
}

bool LayoutEditorWindow::promptNewPage()
{
    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("New page"));
    auto* form = new QFormLayout(&dlg);
    auto* idEdit = new QLineEdit(QStringLiteral("untitled"));
    auto* nameEdit = new QLineEdit(QStringLiteral("Untitled"));
    auto* tmpl = new QComboBox;
    tmpl->addItem(QStringLiteral("Blank"), int(EditorTemplate::Blank));
    tmpl->addItem(QStringLiteral("Full keyboard"), int(EditorTemplate::Keyboard));
    tmpl->addItem(QStringLiteral("Keyboard row"), int(EditorTemplate::KeyboardRow));
    tmpl->addItem(QStringLiteral("Settings row"), int(EditorTemplate::SettingsRow));
    tmpl->addItem(QStringLiteral("Zone chip"), int(EditorTemplate::EdgeChip));
    form->addRow(QStringLiteral("Id"), idEdit);
    form->addRow(QStringLiteral("Name"), nameEdit);
    form->addRow(QStringLiteral("Template"), tmpl);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    if (dlg.exec() != QDialog::Accepted) {
        return false;
    }
    m_session->newFromTemplate(EditorTemplate(tmpl->currentData().toInt()), idEdit->text(),
                               nameEdit->text());
    return true;
}

LayoutEditorWindow::CatalogChoice LayoutEditorWindow::promptOpenCatalog()
{
    if (m_layoutsDir.isEmpty() && m_userDir.isEmpty()) {
        return CatalogChoice::Browse;
    }
    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("Open page"));
    auto* lay = new QVBoxLayout(&dlg);
    auto* list = new QListWidget;
    for (const auto& file : openableLayoutFiles(m_userDir, m_layoutsDir)) {
        auto* item = new QListWidgetItem(file.first);
        item->setData(Qt::UserRole, file.second);
        list->addItem(item);
    }
    list->sortItems();
    list->setCurrentRow(0);
    const int rowH = list->count() > 0 ? qMax(list->sizeHintForRow(0), 20) : 22;
    list->setMinimumHeight(rowH * kOpenListRows);
    lay->addWidget(list);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    auto* browse = buttons->addButton(QStringLiteral("Browse…"), QDialogButtonBox::ActionRole);
    lay->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    connect(browse, &QPushButton::clicked, &dlg, [&dlg]() { dlg.done(kBrowseResult); });
    connect(list, &QListWidget::itemDoubleClicked, &dlg, &QDialog::accept);
    const int result = dlg.exec();
    if (result == kBrowseResult) {
        return CatalogChoice::Browse;
    }
    if (result != QDialog::Accepted || !list->currentItem()) {
        return CatalogChoice::Cancel;
    }
    const QString path = list->currentItem()->data(Qt::UserRole).toString();
    QString err;
    if (!m_session->loadFromFile(path, &err)) {
        QMessageBox::warning(this, QStringLiteral("Open failed"), err);
        return CatalogChoice::Cancel;
    }
    return CatalogChoice::Loaded;
}

bool LayoutEditorWindow::openFromFileDialog()
{
    const QString path =
        QFileDialog::getOpenFileName(this, QStringLiteral("Open page"), defaultDir(), xmlFilter());
    if (path.isEmpty()) {
        return false;
    }
    QString err;
    if (!m_session->loadFromFile(path, &err)) {
        QMessageBox::warning(this, QStringLiteral("Open failed"), err);
        return false;
    }
    return true;
}

void LayoutEditorWindow::newFile()
{
    if (!maybeSave()) {
        return;
    }
    if (promptNewPage()) {
        frameLoadedPage();
    }
}

void LayoutEditorWindow::open()
{
    if (!maybeSave()) {
        return;
    }
    switch (promptOpenCatalog()) {
    case CatalogChoice::Loaded:
        frameLoadedPage();
        return;
    case CatalogChoice::Cancel:
        return;
    case CatalogChoice::Browse:
        break;
    }
    if (openFromFileDialog()) {
        frameLoadedPage();
    }
}

void LayoutEditorWindow::save()
{
    if (!applyCodeView()) {
        return;
    }
    if (!confirmIssues(QStringLiteral("save"))) {
        return;
    }
    if (m_session->filePath().isEmpty()) {
        saveAs();
        return;
    }
    if (isShippedPath(m_session->filePath()) && !m_userDir.isEmpty()) {
        const QString dest = userCopyPath(m_session->filePath());
        QString err;
        if (!m_session->saveTo(dest, &err)) {
            QMessageBox::warning(this, QStringLiteral("Save failed"), err);
            return;
        }
        statusBar()->showMessage(
            QStringLiteral("Saved user copy %1 (shipped file unchanged)").arg(dest), 5000);
        return;
    }
    QString err;
    if (!m_session->save(&err)) {
        QMessageBox::warning(this, QStringLiteral("Save failed"), err);
    }
}

void LayoutEditorWindow::saveAs()
{
    if (!applyCodeView()) {
        return;
    }
    if (!confirmIssues(QStringLiteral("save"))) {
        return;
    }
    const QString suggest = m_session->filePath().isEmpty() || isShippedPath(m_session->filePath())
                                ? QDir(defaultDir()).filePath(m_session->document().id + QStringLiteral(".xml"))
                                : m_session->filePath();
    const QString path =
        QFileDialog::getSaveFileName(this, QStringLiteral("Save page"), suggest, xmlFilter());
    if (path.isEmpty()) {
        return;
    }
    QString err;
    if (!m_session->saveTo(path, &err)) {
        QMessageBox::warning(this, QStringLiteral("Save failed"), err);
    }
}

void LayoutEditorWindow::closeDoc()
{
    if (!maybeSave()) {
        return;
    }
    m_session->closeDocument();
}

void LayoutEditorWindow::importFile()
{
    if (!maybeSave()) {
        return;
    }
    const QString path =
        QFileDialog::getOpenFileName(this, QStringLiteral("Import page"), defaultDir(), xmlFilter());
    if (path.isEmpty()) {
        return;
    }
    QString err;
    if (!m_session->importFromFile(path, &err)) {
        QMessageBox::warning(this, QStringLiteral("Import failed"), err);
    }
}

void LayoutEditorWindow::exportFile()
{
    const QString suggest =
        QDir(defaultDir()).filePath(m_session->document().id + QStringLiteral(".xml"));
    const QString path =
        QFileDialog::getSaveFileName(this, QStringLiteral("Export page"), suggest, xmlFilter());
    if (path.isEmpty()) {
        return;
    }
    QString err;
    if (!m_session->exportTo(path, &err)) {
        QMessageBox::warning(this, QStringLiteral("Export failed"), err);
    }
}

void LayoutEditorWindow::testLive()
{
    if (!applyCodeView()) {
        return;
    }
    if (!confirmIssues(QStringLiteral("test on desktop"))) {
        return;
    }
    if (!m_test) {
        QMessageBox::information(this, QStringLiteral("Test"),
                                 QStringLiteral("Live test is only available while Gazer is running."));
        return;
    }
    QString err;
    if (!m_test(m_session->document(), &err)) {
        QMessageBox::warning(this, QStringLiteral("Test failed"),
                             err.isEmpty() ? QStringLiteral("Could not open page") : err);
        return;
    }
    statusBar()->showMessage(QStringLiteral("Live board opened for %1").arg(m_session->document().id),
                             4000);
}

QString LayoutEditorWindow::defaultDir() const
{
    if (!m_userDir.isEmpty()) {
        return m_userDir;
    }
    if (!m_layoutsDir.isEmpty()) {
        return m_layoutsDir;
    }
    if (!m_session->filePath().isEmpty()) {
        return QFileInfo(m_session->filePath()).absolutePath();
    }
    return QStringLiteral(".");
}

QString LayoutEditorWindow::xmlFilter() const
{
    return QStringLiteral("Page XML (*.xml);;All files (*.*)");
}

bool LayoutEditorWindow::isShippedPath(const QString& path) const
{
    if (path.isEmpty() || m_layoutsDir.isEmpty()) {
        return false;
    }
    const QString dir = QDir::cleanPath(QFileInfo(path).absolutePath());
    const QString shipped = QDir::cleanPath(QDir(m_layoutsDir).absolutePath());
    if (dir.compare(shipped, Qt::CaseInsensitive) == 0) {
        return true;
    }
    if (!m_userDir.isEmpty()) {
        const QString user = QDir::cleanPath(QDir(m_userDir).absolutePath());
        if (dir.compare(user, Qt::CaseInsensitive) == 0) {
            return false;
        }
    }
    return false;
}

QString LayoutEditorWindow::userCopyPath(const QString& path) const
{
    return QDir(m_userDir).filePath(QFileInfo(path).fileName());
}

bool LayoutEditorWindow::confirmIssues(const QString& action)
{
    const QVector<EditorIssue> issues = m_session->validate(m_catalogIds);
    if (issues.isEmpty()) {
        return true;
    }
    QStringList lines;
    const int n = qMin(8, issues.size());
    for (int i = 0; i < n; ++i) {
        lines.push_back(issues[i].message);
    }
    const QString body =
        QStringLiteral("This page has problems:\n\n• %1\n\n%2 anyway?")
            .arg(lines.join(QStringLiteral("\n• ")),
                 action.left(1).toUpper() + action.mid(1));
    return QMessageBox::warning(this, QStringLiteral("Page issues"), body,
                                QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
           == QMessageBox::Yes;
}

} // namespace gazer
