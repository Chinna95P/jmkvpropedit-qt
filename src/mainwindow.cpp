#include "mainwindow.h"
#include "trackeditor.h"

#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QDragEnterEvent>
#include <QDirIterator>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QMimeData>
#include <QMimeDatabase>
#include <QPalette>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QRegularExpression>
#include <QSettings>
#include <QShowEvent>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTabWidget>
#include <QUrl>
#include <QVBoxLayout>
#include <QWindow>

#ifdef HAVE_KWINDOWSYSTEM
#include <KWindowEffects>
#endif

namespace {
const QStringList mediaExtensions = {"mkv", "mka", "mks", "mk3d", "webm"};

QString shellQuote(const QString &text)
{
    if (!text.contains(QRegularExpression("[^A-Za-z0-9_./:=+-]")))
        return text;
    QString escaped = text;
    escaped.replace('\'', "'\\''");
    return "'" + escaped + "'";
}

QPushButton *browseButton(QLineEdit *edit, const QString &caption, const QString &filter)
{
    auto *button = new QPushButton(QObject::tr("Browse…"), edit->parentWidget());
    QObject::connect(button, &QPushButton::clicked, edit, [edit, caption, filter] {
        const QString path = QFileDialog::getOpenFileName(edit, caption, {}, filter);
        if (!path.isEmpty()) edit->setText(path);
    });
    return button;
}
}

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    m_systemPalette = QApplication::palette();
    setWindowTitle(tr("JMkvpropedit Qt"));
    setObjectName("JMkvpropeditQt");
    resize(1120, 720);
    setAcceptDrops(true);

    auto *central = new QWidget(this);
    auto *outer = new QVBoxLayout(central);
    m_tabs = new QTabWidget(central);
    m_tabs->addTab(makeInputPage(), tr("Input"));
    m_tabs->addTab(makeGeneralPage(), tr("General"));
    m_video = new TrackEditor("video", "v", m_tabs);
    m_audio = new TrackEditor("audio", "a", m_tabs);
    m_subtitles = new TrackEditor("subtitles", "s", m_tabs);
    m_tabs->addTab(m_video, tr("Video"));
    m_tabs->addTab(m_audio, tr("Audio"));
    m_tabs->addTab(m_subtitles, tr("Subtitles"));
    m_tabs->addTab(makeAttachmentsPage(), tr("Attachments"));
    m_tabs->addTab(makeOptionsPage(), tr("Options"));
    m_output = new QPlainTextEdit(m_tabs);
    m_output->setReadOnly(true);
    m_output->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    m_tabs->addTab(m_output, tr("Output"));
    outer->addWidget(m_tabs);

    auto *bottom = new QHBoxLayout;
    m_progress = new QProgressBar(central);
    m_progress->setTextVisible(true);
    bottom->addWidget(m_progress, 1);
    auto *preview = new QPushButton(tr("Preview commands"), central);
    auto *process = new QPushButton(tr("Process files"), central);
    auto *cancel = new QPushButton(tr("Cancel"), central);
    process->setDefault(true);
    bottom->addWidget(preview);
    bottom->addWidget(process);
    bottom->addWidget(cancel);
    outer->addLayout(bottom);
    setCentralWidget(central);

    connect(preview, &QPushButton::clicked, this, &MainWindow::previewCommands);
    connect(process, &QPushButton::clicked, this, &MainWindow::beginProcessing);
    connect(cancel, &QPushButton::clicked, this, [this] {
        m_queue.clear();
        if (m_process.state() != QProcess::NotRunning) m_process.kill();
        m_output->appendPlainText(tr("\nCancelled."));
    });
    connect(&m_process, &QProcess::readyReadStandardOutput, this, [this] {
        m_output->appendPlainText(QString::fromLocal8Bit(m_process.readAllStandardOutput()).trimmed());
    });
    connect(&m_process, &QProcess::readyReadStandardError, this, [this] {
        m_output->appendPlainText(QString::fromLocal8Bit(m_process.readAllStandardError()).trimmed());
    });
    connect(&m_process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
            [this](int code, QProcess::ExitStatus status) {
        ++m_completed;
        m_progress->setValue(m_completed);
        if (status != QProcess::NormalExit || code != 0)
            m_output->appendPlainText(tr("Command failed with exit code %1; continuing batch.\n").arg(code));
        startNext();
    });
    loadSettings();
    connect(m_mkvpropedit, &QLineEdit::editingFinished, this, &MainWindow::saveSettings);
    connect(m_mkvmerge, &QLineEdit::editingFinished, this, &MainWindow::saveSettings);
}

QWidget *MainWindow::makeInputPage()
{
    auto *page = new QWidget(m_tabs);
    auto *layout = new QVBoxLayout(page);
    layout->addWidget(new QLabel(tr("Add Matroska files or folders. Track selectors are populated from the first file."), page));
    m_files = new QListWidget(page);
    m_files->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_files->setAlternatingRowColors(true);
    layout->addWidget(m_files);
    auto *buttons = new QHBoxLayout;
    auto *addFilesButton = new QPushButton(tr("Add files…"), page);
    auto *addFolderButton = new QPushButton(tr("Add folder…"), page);
    auto *remove = new QPushButton(tr("Remove selected"), page);
    auto *clear = new QPushButton(tr("Clear"), page);
    buttons->addWidget(addFilesButton);
    buttons->addWidget(addFolderButton);
    buttons->addWidget(remove);
    buttons->addWidget(clear);
    buttons->addStretch();
    layout->addLayout(buttons);
    connect(addFilesButton, &QPushButton::clicked, this, [this] {
        addFiles(QFileDialog::getOpenFileNames(this, tr("Add Matroska files"), {},
                                               tr("Matroska files (*.mkv *.mka *.mks *.mk3d *.webm)")));
    });
    connect(addFolderButton, &QPushButton::clicked, this, [this] {
        const QString folder = QFileDialog::getExistingDirectory(this, tr("Add folder"));
        if (folder.isEmpty()) return;
        QStringList paths;
        QDirIterator it(folder, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString path = it.next();
            if (mediaExtensions.contains(QFileInfo(path).suffix(), Qt::CaseInsensitive)) paths << path;
        }
        addFiles(paths);
    });
    connect(remove, &QPushButton::clicked, this, [this] { qDeleteAll(m_files->selectedItems()); });
    connect(clear, &QPushButton::clicked, m_files, &QListWidget::clear);
    return page;
}

QWidget *MainWindow::makeGeneralPage()
{
    auto *page = new QWidget(m_tabs);
    auto *layout = new QVBoxLayout(page);
    auto *titleBox = new QGroupBox(tr("Segment title"), page);
    auto *titleLayout = new QFormLayout(titleBox);
    m_applyTitle = new QCheckBox(tr("Change title"), titleBox);
    m_titleMode = new QComboBox(titleBox);
    m_titleMode->addItems({tr("Fixed text"), tr("Use file name without extension"), tr("Clear title")});
    m_title = new QLineEdit(titleBox);
    titleLayout->addRow(m_applyTitle);
    titleLayout->addRow(tr("Mode:"), m_titleMode);
    titleLayout->addRow(tr("Title:"), m_title);
    connect(m_titleMode, &QComboBox::currentIndexChanged, m_title, [this](int i) { m_title->setEnabled(i == 0); });
    layout->addWidget(titleBox);

    auto *metadataBox = new QGroupBox(tr("Chapters and tags"), page);
    auto *form = new QFormLayout(metadataBox);
    m_chaptersMode = new QComboBox(metadataBox);
    m_chaptersMode->addItems({tr("Keep unchanged"), tr("Remove"), tr("Import XML file")});
    m_chaptersFile = new QLineEdit(metadataBox);
    auto *chaptersRow = new QWidget(metadataBox);
    auto *chaptersLayout = new QHBoxLayout(chaptersRow);
    chaptersLayout->setContentsMargins(0, 0, 0, 0);
    chaptersLayout->addWidget(m_chaptersFile);
    chaptersLayout->addWidget(browseButton(m_chaptersFile, tr("Select chapters XML"), tr("XML files (*.xml);;All files (*)")));
    form->addRow(tr("Chapters:"), m_chaptersMode);
    form->addRow(tr("Chapters file:"), chaptersRow);
    m_tagsMode = new QComboBox(metadataBox);
    m_tagsMode->addItems({tr("Keep unchanged"), tr("Remove all"), tr("Import XML file")});
    m_tagsFile = new QLineEdit(metadataBox);
    auto *tagsRow = new QWidget(metadataBox);
    auto *tagsLayout = new QHBoxLayout(tagsRow);
    tagsLayout->setContentsMargins(0, 0, 0, 0);
    tagsLayout->addWidget(m_tagsFile);
    tagsLayout->addWidget(browseButton(m_tagsFile, tr("Select tags XML"), tr("XML files (*.xml);;All files (*)")));
    form->addRow(tr("Tags:"), m_tagsMode);
    form->addRow(tr("Tags file:"), tagsRow);
    m_extraArguments = new QLineEdit(metadataBox);
    m_extraArguments->setPlaceholderText(tr("Advanced mkvpropedit arguments"));
    form->addRow(tr("Additional arguments:"), m_extraArguments);
    layout->addWidget(metadataBox);
    layout->addStretch();
    return page;
}

QWidget *MainWindow::makeAttachmentsPage()
{
    auto *page = new QWidget(m_tabs);
    auto *layout = new QVBoxLayout(page);
    layout->addWidget(new QLabel(tr("The listed files will be attached to every input file."), page));
    m_attachments = new QListWidget(page);
    layout->addWidget(m_attachments);
    auto *buttons = new QHBoxLayout;
    auto *add = new QPushButton(tr("Add attachments…"), page);
    auto *remove = new QPushButton(tr("Remove selected"), page);
    buttons->addWidget(add);
    buttons->addWidget(remove);
    buttons->addStretch();
    layout->addLayout(buttons);
    connect(add, &QPushButton::clicked, this, [this] {
        for (const QString &path : QFileDialog::getOpenFileNames(this, tr("Add attachments")))
            m_attachments->addItem(path);
    });
    connect(remove, &QPushButton::clicked, this, [this] { qDeleteAll(m_attachments->selectedItems()); });
    return page;
}

QWidget *MainWindow::makeOptionsPage()
{
    auto *page = new QWidget(m_tabs);
    auto *form = new QFormLayout(page);
    m_mkvpropedit = new QLineEdit("mkvpropedit", page);
    m_mkvmerge = new QLineEdit("mkvmerge", page);
    m_appearance = new QComboBox(page);
    m_appearance->addItems({tr("Follow system / Kvantum"), tr("Dark"), tr("Light")});
    form->addRow(tr("mkvpropedit program:"), m_mkvpropedit);
    form->addRow(tr("mkvmerge program:"), m_mkvmerge);
    form->addRow(tr("Appearance:"), m_appearance);
    auto *note = new QLabel(tr("System mode uses the active Qt widget style. On this system that is Kvantum; the app also requests KWin blur behind its translucent region."), page);
    note->setWordWrap(true);
    form->addRow(note);
    connect(m_appearance, &QComboBox::currentIndexChanged, this, [this](int mode) {
        applyAppearance(mode);
        saveSettings();
    });
    return page;
}

void MainWindow::addFiles(const QStringList &paths)
{
    QSet<QString> existing;
    for (int i = 0; i < m_files->count(); ++i) existing.insert(m_files->item(i)->text());
    bool firstAdded = m_files->count() == 0;
    for (const QString &input : paths) {
        const QString path = QFileInfo(input).canonicalFilePath();
        if (!path.isEmpty() && !existing.contains(path)) {
            m_files->addItem(path);
            existing.insert(path);
        }
    }
    if (firstAdded && m_files->count()) probeFirstFile();
}

void MainWindow::addInputFiles(const QStringList &paths)
{
    addFiles(paths);
}

void MainWindow::probeFirstFile()
{
    QProcess probe;
    probe.start(m_mkvmerge ? m_mkvmerge->text() : QStringLiteral("mkvmerge"), {"-J", m_files->item(0)->text()});
    if (!probe.waitForFinished(8000) || probe.exitCode() != 0) {
        statusBar()->showMessage(tr("Could not inspect tracks; selectors can still be added manually."), 5000);
        return;
    }
    const QJsonDocument document = QJsonDocument::fromJson(probe.readAllStandardOutput());
    const QJsonArray tracks = document.object().value("tracks").toArray();
    m_video->populate(tracks);
    m_audio->populate(tracks);
    m_subtitles->populate(tracks);
    statusBar()->showMessage(tr("Track selectors loaded from %1").arg(QFileInfo(m_files->item(0)->text()).fileName()), 5000);
}

QStringList MainWindow::argumentsFor(const QString &file) const
{
    QStringList args{file};
    if (m_applyTitle->isChecked()) {
        QString title = m_title->text();
        if (m_titleMode->currentIndex() == 1) title = QFileInfo(file).completeBaseName();
        if (m_titleMode->currentIndex() == 2) title.clear();
        args << "--edit" << "info" << "--set" << "title=" + title;
    }
    if (m_chaptersMode->currentIndex() == 1) args << "--chapters" << "";
    if (m_chaptersMode->currentIndex() == 2) args << "--chapters" << m_chaptersFile->text();
    if (m_tagsMode->currentIndex() == 1) args << "--tags" << "all:";
    if (m_tagsMode->currentIndex() == 2) args << "--tags" << "all:" + m_tagsFile->text();
    args << m_video->arguments() << m_audio->arguments() << m_subtitles->arguments();
    QMimeDatabase mime;
    for (int i = 0; i < m_attachments->count(); ++i) {
        const QString path = m_attachments->item(i)->text();
        args << "--attachment-name" << QFileInfo(path).fileName()
             << "--attachment-mime-type" << mime.mimeTypeForFile(path).name()
             << "--add-attachment" << path;
    }
    if (!m_extraArguments->text().trimmed().isEmpty())
        args << QProcess::splitCommand(m_extraArguments->text());
    return args;
}

QString MainWindow::displayCommand(const QString &program, const QStringList &arguments)
{
    QStringList parts{shellQuote(program)};
    for (const QString &argument : arguments) parts << shellQuote(argument);
    return parts.join(' ');
}

void MainWindow::previewCommands()
{
    m_output->clear();
    for (int i = 0; i < m_files->count(); ++i)
        m_output->appendPlainText(displayCommand(m_mkvpropedit->text(), argumentsFor(m_files->item(i)->text())) + "\n");
    m_tabs->setCurrentWidget(m_output);
}

void MainWindow::beginProcessing()
{
    if (m_process.state() != QProcess::NotRunning) return;
    if (m_files->count() == 0) {
        QMessageBox::information(this, tr("No input files"), tr("Add at least one Matroska file first."));
        return;
    }
    if (QStandardPaths::findExecutable(m_mkvpropedit->text()).isEmpty() && !QFileInfo::exists(m_mkvpropedit->text())) {
        QMessageBox::critical(this, tr("mkvpropedit not found"), tr("Check the program path on the Options tab."));
        return;
    }
    if (QMessageBox::question(this, tr("Edit files in place"),
                              tr("mkvpropedit will modify %1 file(s) in place. Continue?").arg(m_files->count())) != QMessageBox::Yes)
        return;
    m_queue.clear();
    for (int i = 0; i < m_files->count(); ++i) m_queue << m_files->item(i)->text();
    m_completed = 0;
    m_progress->setRange(0, m_queue.size());
    m_progress->setValue(0);
    m_output->clear();
    m_tabs->setCurrentWidget(m_output);
    startNext();
}

void MainWindow::startNext()
{
    if (m_queue.isEmpty()) {
        if (m_completed) m_output->appendPlainText(tr("\nBatch complete: %1 file(s) processed.").arg(m_completed));
        return;
    }
    const QString file = m_queue.takeFirst();
    const QStringList args = argumentsFor(file);
    m_output->appendPlainText("$ " + displayCommand(m_mkvpropedit->text(), args));
    m_process.start(m_mkvpropedit->text(), args);
}

void MainWindow::applyAppearance(int mode)
{
    QPalette palette;
    if (mode == 1) {
        palette.setColor(QPalette::Window, QColor(32, 35, 42, 235));
        palette.setColor(QPalette::WindowText, QColor(235, 238, 245));
        palette.setColor(QPalette::Base, QColor(24, 27, 33, 225));
        palette.setColor(QPalette::AlternateBase, QColor(40, 44, 53, 225));
        palette.setColor(QPalette::Text, QColor(235, 238, 245));
        palette.setColor(QPalette::Button, QColor(45, 49, 59, 235));
        palette.setColor(QPalette::ButtonText, QColor(235, 238, 245));
        palette.setColor(QPalette::Highlight, QColor(91, 139, 255));
        palette.setColor(QPalette::HighlightedText, Qt::white);
        palette.setColor(QPalette::PlaceholderText, QColor(150, 157, 170));
    } else if (mode == 2) {
        palette.setColor(QPalette::Window, QColor(244, 246, 250, 240));
        palette.setColor(QPalette::WindowText, QColor(30, 33, 38));
        palette.setColor(QPalette::Base, QColor(255, 255, 255, 235));
        palette.setColor(QPalette::AlternateBase, QColor(235, 239, 246, 235));
        palette.setColor(QPalette::Text, QColor(30, 33, 38));
        palette.setColor(QPalette::Button, QColor(235, 239, 246, 240));
        palette.setColor(QPalette::ButtonText, QColor(30, 33, 38));
        palette.setColor(QPalette::Highlight, QColor(50, 105, 220));
        palette.setColor(QPalette::HighlightedText, Qt::white);
    } else {
        palette = m_systemPalette;
    }
    QApplication::setPalette(palette);
}

void MainWindow::loadSettings()
{
    QSettings settings;
    m_mkvpropedit->setText(settings.value("tools/mkvpropedit", "mkvpropedit").toString());
    m_mkvmerge->setText(settings.value("tools/mkvmerge", "mkvmerge").toString());
    m_appearance->setCurrentIndex(settings.value("appearance/mode", 0).toInt());
    applyAppearance(m_appearance->currentIndex());
}

void MainWindow::saveSettings()
{
    QSettings settings;
    settings.setValue("tools/mkvpropedit", m_mkvpropedit->text());
    settings.setValue("tools/mkvmerge", m_mkvmerge->text());
    settings.setValue("appearance/mode", m_appearance->currentIndex());
}

void MainWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);
    if (m_blurApplied) return;
    m_blurApplied = true;
#ifdef HAVE_KWINDOWSYSTEM
    if (windowHandle()) KWindowEffects::enableBlurBehind(windowHandle(), true);
#endif
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    saveSettings();
    QMainWindow::closeEvent(event);
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls()) event->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent *event)
{
    QStringList files;
    for (const QUrl &url : event->mimeData()->urls()) {
        const QFileInfo info(url.toLocalFile());
        if (info.isFile() && mediaExtensions.contains(info.suffix(), Qt::CaseInsensitive)) {
            files << info.absoluteFilePath();
        } else if (info.isDir()) {
            QDirIterator it(info.absoluteFilePath(), QDir::Files, QDirIterator::Subdirectories);
            while (it.hasNext()) {
                const QString path = it.next();
                if (mediaExtensions.contains(QFileInfo(path).suffix(), Qt::CaseInsensitive)) files << path;
            }
        }
    }
    addFiles(files);
    event->acceptProposedAction();
}
