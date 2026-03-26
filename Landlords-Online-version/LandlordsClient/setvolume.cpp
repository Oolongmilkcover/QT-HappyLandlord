#include "setvolume.h"
#include "ui_setvolume.h"

setVolume::setVolume(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::setVolume)
{
    ui->setupUi(this);
    ui->label->setStyleSheet("QLabel {color: white; background: transparent;}");
    ui->label_2->setStyleSheet("QLabel {color: white; background: transparent;}");
}

setVolume::~setVolume()
{
    delete ui;
}



