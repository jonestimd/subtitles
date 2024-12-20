#include "mainwindow.h"
#include <QHBoxLayout>
#include <QMenuBar>
#include <QFileDialog>
#include <QStatusBar>
#include <QToolBar>
#include <QRegularExpression>
#include <QMessageBox>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , tessApi()
{
    QWidget *center = new QWidget;
    QHBoxLayout *layout = new QHBoxLayout(center);
    subtitleImage = new QLabel;
    layout->addWidget(subtitleImage);
    subtitleText = new QTextEdit;
    subtitleText->setFontPointSize(12);
    subtitleText->setMinimumWidth(400);
    connect(subtitleText, SIGNAL(textChanged()), this, SLOT(textChanged()));
    layout->addWidget(subtitleText);

    setCentralWidget(center);

    auto toolbar = new QToolBar(this);
    toolbar->setMovable(false);
    toolbar->setIconSize(QSize{16,16});
    addToolBar(toolbar);

    auto openAction = toolbar->addAction(QIcon::fromTheme(QIcon::ThemeIcon::DocumentOpen), tr("Open file (ctrl+o)"), QKeySequence("ctrl+o"));
    connect(openAction, SIGNAL(triggered(bool)), this, SLOT(openFile()));

    auto importAction = toolbar->addAction(QIcon::fromTheme(QIcon::ThemeIcon::EditCopy), tr("Import file (ctrl+i)"), QKeySequence("ctrl+i"));
    connect(importAction, SIGNAL(triggered(bool)), this, SLOT(importFile()));

    saveAction = toolbar->addAction(QIcon::fromTheme(QIcon::ThemeIcon::DocumentSaveAs), tr("Save subtitles (ctrl+s)"), QKeySequence("ctrl+s"));
    saveAction->setEnabled(false);
    connect(saveAction, SIGNAL(triggered(bool)), this, SLOT(saveFile()));

    nextAction = toolbar->addAction(QIcon::fromTheme(QIcon::ThemeIcon::GoNext), tr("Next subtitle (ctrl+n)"), QKeySequence("ctrl+n"));
    nextAction->setEnabled(false);
    connect(nextAction, SIGNAL(triggered(bool)), this, SLOT(nextImage()));

    previousAction = toolbar->addAction(QIcon::fromTheme(QIcon::ThemeIcon::GoPrevious), tr("Previous subtitle (ctrl+p)"), QKeySequence("ctrl+p"));
    previousAction->setEnabled(false);
    connect(previousAction, SIGNAL(triggered(bool)), this, SLOT(previousImage()));

    auto statusBar = new QStatusBar(this);
    setStatusBar(statusBar);
    statusLabel = new QLabel(tr("no file loaded"));
    statusBar->addWidget(statusLabel);
}

MainWindow::~MainWindow() {}

void MainWindow::show() {
    QMainWindow::show();
    auto tessDir = getenv("TESSDATA_DIR");
    if (tessApi.Init(tessDir == NULL ? "/usr/share/tesseract-ocr/5/tessdata" : tessDir, "eng")) {
        QMessageBox::critical(this, tr("Tesseract error"), tr("Failed to initialize.\nCheck value of TESSDATA_DIR env variable."));
        close();
    }
}

void MainWindow::readImage() {
    auto subtitle = subtitles[currentIndex];
    auto filename = subtitle->imageFile;
    auto filepath = QFileInfo(inputDir, filename);
    if (filepath.exists()) {
        subtitleImage->setPixmap(QPixmap(filepath.absoluteFilePath()));
        auto image = subtitleImage->pixmap().toImage();
        if (subtitle->text.isEmpty()) {
            tessApi.SetImage(image.bits(), image.width(), image.height(),
                             image.depth() / 8, image.bytesPerLine());
            auto text = tessApi.GetUTF8Text();
            subtitleText->setText(text);
            subtitle->text = text;
        } else subtitleText->setText(subtitle->text);
        statusLabel->setText(tr("%1 of %2").arg(currentIndex + 1).arg(subtitles.length()));
        nextAction->setEnabled(subtitles.length() > currentIndex + 1);
        previousAction->setEnabled(currentIndex > 0);
    } else QMessageBox::critical(this, tr("Missing file"), tr("File not found: %1").arg(filepath.fileName()));
}

void MainWindow::openFile() {
    if (!subtitles.isEmpty() && !subtitles[0]->text.isEmpty()) {
        auto answer = QMessageBox::question(this, tr("Unsaved changes"), tr("Discard unsaved changes?"));
        if (answer != QMessageBox::Yes) return;
    }
    QFileDialog::getOpenFileContent(tr("Subtitles (*.sub)"), [this](const QString &name, const QByteArray &content) {
        if (!name.isEmpty()) {
            while (!subtitles.isEmpty()) {
                delete subtitles.takeLast();
            }
            currentIndex = 0;
            inputDir = QFileInfo(name).path();
            auto inputText = QString(content).split("\n");
            for (auto line : inputText) {
                auto subtitle = new Subtitle(line.split("\t"));
                if (subtitle->isValid()) subtitles.append(subtitle);
                else if (!line.isEmpty()) {
                    QMessageBox::critical(this,
                                          tr("Invalid file"),
                                          tr("Invalid entry on line %1:\n\"%2\"").arg(subtitles.length()+1).arg(line));
                    break;
                }
            }
            if (!subtitles.isEmpty()) {
                readImage();
                saveAction->setEnabled(true);
            }
        }
    });
}

Q_GLOBAL_STATIC(QRegularExpression, timePattern , "^(" TIME_FORMAT ") +--> +(" TIME_FORMAT ")$");

void MainWindow::importFile() {
    QFileDialog::getOpenFileContent(tr("Subtitles (*.srt)"), [this](const QString &name, const QByteArray &content) {
        if (!name.isEmpty()) {
            auto inputText = QString(content).split("\n");
            bool ok;
            for (int line = 0, i = 0; line < inputText.length() && i < subtitles.length(); i++) {
                auto index = inputText[line].toUInt(&ok);
                if (!ok) {
                    QMessageBox::critical(this, tr("Input error"), tr("Invalid index at line %1").arg(line+1));
                    return;
                }
                if (index != i+1) {
                    QMessageBox::critical(this, tr("Input error"), tr("Wrong index at line %1").arg(line+1));
                    return;
                }
                if (++line < inputText.length()) {
                    auto match = timePattern->match(inputText[line]);
                    if (match.hasMatch()) {
                        auto times = match.capturedTexts();
                        if (subtitles[i]->startTime != times[0] || subtitles[i]->endTime != times[1]) {
                            QMessageBox::critical(this, tr("Input error"), tr("Timing mismatch at line %1").arg(line+1));
                            return;
                        }
                        if (++line < inputText.length()) {
                            QStringList lines;
                            while (line < inputText.length() && !inputText[line].isEmpty()) {
                                lines.append(inputText[line++]);
                            }
                            subtitles[i]->text = lines.join("\n");
                            while (line < inputText.length() && inputText[line].isEmpty()) line++;
                        } else QMessageBox::information(this, tr("Input error"), tr("Unexpected end of file"));
                    } else {
                        QMessageBox::critical(this, tr("Input error"), tr("Invalid timing at line %1").arg(line+1));
                        return;
                    }
                } else QMessageBox::information(this, tr("Input error"), tr("Unexpected end of file"));
            }
        }
    });
}

void MainWindow::saveFile() {
    auto filename = QFileDialog::getSaveFileName(this, tr("Save subtitles"), inputDir, tr("Subtitles (*.srt)"));
    if (QFileInfo(filename).exists()) {
        auto answer = QMessageBox::question(this, tr("Replace file"), tr("Replace existing file?"));
        if (answer != QMessageBox::Yes) return;
    }
    if (filename.indexOf('.') < 0) filename.append(".srt");
    QFile file(filename);
    if (file.open(QFile::WriteOnly)) {
        for (int i = 0; i < subtitles.length() && !subtitles[i]->text.isEmpty(); i++) {
            file.write(QString::number(i+1).toUtf8());
            file.write("\n");
            file.write(subtitles[i]->startTime.toUtf8());
            file.write(" --> ");
            file.write(subtitles[i]->endTime.toUtf8());
            file.write("\n");
            file.write(subtitles[i]->text.trimmed().toUtf8());
            file.write("\n\n");
        }
        file.close();
    } else QMessageBox::critical(this, tr("File error"), tr("Error opening file"));
}

void MainWindow::nextImage() {
    currentIndex++;
    readImage();
}

void MainWindow::previousImage() {
    currentIndex--;
    readImage();
}

void MainWindow::textChanged() {
    if (currentIndex < subtitles.length()) {
        subtitles[currentIndex]->text = subtitleText->toPlainText();
    }
}