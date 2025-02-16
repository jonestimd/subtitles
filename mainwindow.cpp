#include "mainwindow.h"
#include <QHBoxLayout>
#include <QMenuBar>
#include <QFileDialog>
#include <QStatusBar>
#include <QToolBar>
#include <QRegularExpression>
#include <QMessageBox>
#include <QCloseEvent>

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

    using ThemeIcon = QIcon::ThemeIcon;
    auto openAction = toolbar->addAction(QIcon::fromTheme(ThemeIcon::DocumentOpen), tr("Open file (ctrl+o)"), QKeySequence("ctrl+o"));
    connect(openAction, SIGNAL(triggered(bool)), this, SLOT(openFile()));

    auto importAction = toolbar->addAction(QIcon::fromTheme(ThemeIcon::EditCopy), tr("Import file (ctrl+i)"), QKeySequence("ctrl+i"));
    connect(importAction, SIGNAL(triggered(bool)), this, SLOT(importFile()));

    saveAction = toolbar->addAction(QIcon::fromTheme(ThemeIcon::DocumentSaveAs), tr("Save subtitles (ctrl+s)"), QKeySequence("ctrl+s"));
    saveAction->setEnabled(false);
    connect(saveAction, SIGNAL(triggered(bool)), this, SLOT(saveFile()));

    nextAction = toolbar->addAction(QIcon::fromTheme(ThemeIcon::GoNext), tr("Next subtitle (ctrl+n)"), QKeySequence("ctrl+n"));
    nextAction->setEnabled(false);
    connect(nextAction, SIGNAL(triggered(bool)), this, SLOT(nextImage()));

    previousAction = toolbar->addAction(QIcon::fromTheme(ThemeIcon::GoPrevious), tr("Previous subtitle (ctrl+p)"), QKeySequence("ctrl+p"));
    previousAction->setEnabled(false);
    connect(previousAction, SIGNAL(triggered(bool)), this, SLOT(previousImage()));

    toolbar->addSeparator();

    auto italicsAction = toolbar->addAction(QIcon::fromTheme(ThemeIcon::FormatTextItalic), tr("Italics (alt+i"), QKeySequence("alt+i"));
    connect(italicsAction, SIGNAL(triggered(bool)), this, SLOT(italics()));

    auto boldAction = toolbar->addAction(QIcon::fromTheme(ThemeIcon::FormatTextBold), tr("Bold (alt+b"), QKeySequence("alt+b"));
    connect(boldAction, SIGNAL(triggered(bool)), this, SLOT(bold()));

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
            subtitleText->setPlainText(text);
            subtitle->text = text;
        } else subtitleText->setPlainText(subtitle->text);
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
            for (const auto &line : std::as_const(inputText)) {
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
            int i = 0;
            for (int line = 0; line < inputText.length() && i < subtitles.length(); i++) {
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
                        if (subtitles[i]->startTime != times[1] || subtitles[i]->endTime != times[2]) {
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
            if (i > currentIndex) {
                currentIndex = i;
                readImage();
            }
        }
    });
}

void MainWindow::saveFile() {
    auto filename = QFileDialog::getSaveFileName(this, tr("Save subtitles"), inputDir, tr("Subtitles (*.srt)"),
                                                 nullptr, QFileDialog::DontConfirmOverwrite);
    if (filename.isEmpty()) return;
    if (filename.indexOf('.') < 0) filename.append(".srt");
    if (QFileInfo::exists(filename)) {
        auto answer = QMessageBox::question(this, tr("Replace file"), tr("Replace existing file?"));
        if (answer != QMessageBox::Yes) return;
    }
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
        unsaved = false;
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

void changeFormat(QTextEdit *input, const QString &tag) {
    auto size = tag.length();
    auto etag = QString(tag).insert(1, '/');
    auto cursor = input->textCursor();
    if (cursor.hasSelection()) {
        auto start = cursor.selectionStart();
        auto end = cursor.selectionEnd();
        cursor.clearSelection();
        auto text = input->toPlainText();
        if (text.first(start).endsWith(tag)) {
            if (text.indexOf(etag) == end) text.remove(end, size+1);
            text.remove(start-size, size);
            start -= size;
            end -= size;
        } else {
            text.insert(end, etag);
            text.insert(start, tag);
            start += size;
            end += size;
        }
        input->setPlainText(text);
        cursor = input->textCursor();
        cursor.setPosition(start);
        cursor.setPosition(end, QTextCursor::KeepAnchor);
        input->setTextCursor(cursor);
    } else {
        auto text = input->toPlainText().first(cursor.anchor());
        auto begin = text.count(tag);
        auto end = text.count(etag);
        if (begin > end) cursor.insertText(etag);
        else cursor.insertText(tag);
    }
}

void MainWindow::italics() {
    changeFormat(subtitleText, "<i>");
}

void MainWindow::bold() {
    changeFormat(subtitleText, "<b>");
}

void MainWindow::textChanged() {
    if (currentIndex < subtitles.length()) {
        subtitles[currentIndex]->text = subtitleText->toPlainText();
        unsaved = true;
    }
}

void MainWindow::closeEvent(QCloseEvent *event) {
    if (!subtitles.isEmpty() && unsaved) {
        auto answer = QMessageBox::question(this, tr("Unsaved changes"), tr("Discard unsaved changes?"));
        if (answer != QMessageBox::Yes) {
            event->ignore();
            return;
        }
    }
    QWidget::closeEvent(event);
}
