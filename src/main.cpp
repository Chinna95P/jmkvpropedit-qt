#include "mainwindow.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QFileInfo>
#include <QIcon>
#include <QTimer>

int main(int argc, char **argv)
{
    QApplication::setApplicationName("JMkvpropedit Qt");
    QApplication::setApplicationDisplayName("JMkvpropedit Qt");
    QApplication::setOrganizationName("JMkvpropeditQt");
    QApplication::setDesktopFileName("io.github.brunorex.JMkvpropeditQt");
    QApplication::setApplicationVersion("0.1.0");
    QApplication app(argc, argv);
    app.setWindowIcon(QIcon(":/icons/app.svg"));
    QCommandLineParser parser;
    parser.setApplicationDescription("Native Qt 6 batch editor for Matroska properties");
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption screenshotOption("screenshot", "Save a UI screenshot and exit", "file");
    parser.addOption(screenshotOption);
    parser.addPositionalArgument("files", "Matroska files to add", "[files...]");
    parser.process(app);
    MainWindow window;
    window.addInputFiles(parser.positionalArguments());
    window.show();
    if (parser.isSet(screenshotOption)) {
        QTimer::singleShot(400, &window, [&app, &window, &parser, &screenshotOption] {
            window.grab().save(parser.value(screenshotOption));
            app.quit();
        });
    }
    return app.exec();
}
