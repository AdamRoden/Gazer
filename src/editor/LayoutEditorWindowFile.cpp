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
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QStatusBar>
#include <QStringList>
#include <QVBoxLayout>

namespace gazer {

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

bool LayoutEditorWindow::promptOpenCatalog()
{
    if (m_catalogIds.isEmpty() && m_userDir.isEmpty()) {
        return false;
    }
    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("Open page"));
    auto* lay = new QVBoxLayout(&dlg);
    auto* list = new QListWidget;
    QSet<QString> seen;
    auto addPath = [&](const QString& label, const QString& path) {
        if (path.isEmpty() || seen.contains(path) || !QFileInfo::exists(path)) {
            return;
        }
        seen.insert(path);
        auto* item = new QListWidgetItem(label);
        item->setData(Qt::UserRole, path);
        list->addItem(item);
    };
    for (int i = 0; i < m_catalogIds.size(); ++i) {
        const QString id = m_catalogIds[i];
        const QString label = i < m_catalogLabels.size() ? m_catalogLabels[i] : id;
        const QString user = QDir(m_userDir).filePath(id + QStringLiteral(".xml"));
        const QString shipped = QDir(m_layoutsDir).filePath(id + QStringLiteral(".xml"));
        addPath(label + (QFileInfo::exists(user) ? QStringLiteral("  (user)") : QString()),
                QFileInfo::exists(user) ? user : shipped);
    }
    if (!m_userDir.isEmpty()) {
        const QFileInfoList extra =
            QDir(m_userDir).entryInfoList({QStringLiteral("*.xml")}, QDir::Files, QDir::Name);
        for (const QFileInfo& fi : extra) {
            addPath(fi.completeBaseName() + QStringLiteral("  (user)"), fi.absoluteFilePath());
        }
    }
    list->setCurrentRow(0);
    lay->addWidget(list);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    lay->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    connect(list, &QListWidget::itemDoubleClicked, &dlg, &QDialog::accept);
    if (dlg.exec() != QDialog::Accepted || !list->currentItem()) {
        return false;
    }
    const QString path = list->currentItem()->data(Qt::UserRole).toString();
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
    if (promptOpenCatalog()) {
        frameLoadedPage();
        return;
    }
    const QString path =
        QFileDialog::getOpenFileName(this, QStringLiteral("Open page"), defaultDir(), xmlFilter());
    if (path.isEmpty()) {
        return;
    }
    QString err;
    if (!m_session->loadFromFile(path, &err)) {
        QMessageBox::warning(this, QStringLiteral("Open failed"), err);
        return;
    }
    frameLoadedPage();
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
