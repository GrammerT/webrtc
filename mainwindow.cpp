#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::onCaptureAudioLog(std::string log)
{

}


void MainWindow::on_pushButton_clicked()
{
    m_capture = CC::ICCExtenedAudio::create(this);
    if(m_capture->initAudioCapture())
    {
        m_capture->setAudioDataCallback([&](uint8_t *data,int32_t sample_num,int32_t channels,int32_t byte_per_sample){

        });

        m_capture->startAudioCapture();
    }
}
