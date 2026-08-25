#pragma once

#include <QMainWindow>
#include <QPalette>
#include <QProcess>

class QCheckBox;
class QComboBox;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QProgressBar;
class QTabWidget;
class TrackEditor;

class MainWindow final : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    void addInputFiles(const QStringList &paths);

protected:
    void closeEvent(QCloseEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    QWidget *makeInputPage();
    QWidget *makeGeneralPage();
    QWidget *makeAttachmentsPage();
    QWidget *makeOptionsPage();
    void addFiles(const QStringList &paths);
    void probeFirstFile();
    QStringList argumentsFor(const QString &file) const;
    void previewCommands();
    void beginProcessing();
    void startNext();
    void applyAppearance(int mode);
    void loadSettings();
    void saveSettings();
    static QString displayCommand(const QString &program, const QStringList &arguments);

    QTabWidget *m_tabs = nullptr;
    QListWidget *m_files = nullptr;
    QCheckBox *m_applyTitle = nullptr;
    QComboBox *m_titleMode = nullptr;
    QLineEdit *m_title = nullptr;
    QComboBox *m_chaptersMode = nullptr;
    QLineEdit *m_chaptersFile = nullptr;
    QComboBox *m_tagsMode = nullptr;
    QLineEdit *m_tagsFile = nullptr;
    QLineEdit *m_extraArguments = nullptr;
    TrackEditor *m_video = nullptr;
    TrackEditor *m_audio = nullptr;
    TrackEditor *m_subtitles = nullptr;
    QListWidget *m_attachments = nullptr;
    QLineEdit *m_mkvpropedit = nullptr;
    QLineEdit *m_mkvmerge = nullptr;
    QComboBox *m_appearance = nullptr;
    QPlainTextEdit *m_output = nullptr;
    QProgressBar *m_progress = nullptr;
    QProcess m_process;
    QStringList m_queue;
    int m_completed = 0;
    bool m_blurApplied = false;
    QPalette m_systemPalette;
};
