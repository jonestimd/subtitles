#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QLabel>
#include <QMainWindow>
#include <QTextEdit>
#include "subtitle.h"
#include "tesseract/baseapi.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT
    QLabel *subtitleImage;
    QTextEdit *subtitleText;
    QLabel *statusLabel;
    tesseract::TessBaseAPI tessApi;
    QList<Subtitle*> subtitles;
    QString inputDir;
    int currentIndex = 0;
    bool unsaved = false;

    QAction *saveAction;
    QAction *nextAction;
    QAction *previousAction;

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    void show();

private:
    void readImage();

private slots:
    void openFile();
    void importFile();
    void saveFile();
    void nextImage();
    void previousImage();
    void italics();
    void bold();
    void textChanged();

protected:
    virtual void closeEvent(QCloseEvent *event) override;
};
#endif // MAINWINDOW_H
