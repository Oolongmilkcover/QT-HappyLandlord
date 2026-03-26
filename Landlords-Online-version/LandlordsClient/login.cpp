#include "gamepanel.h"
#include "login.h"
#include "ui_login.h"

#include <QMessageBox>

LogIn::LogIn(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::LogIn)
{
    ui->setupUi(this);
    ui->ip->setText("127.0.0.1");
    ui->port->setText("8080");

    connect(ui->play,&QPushButton::clicked,this,[=]{
        if(ui->ip->text().isEmpty()||ui->port->text().isEmpty()||ui->playerName->text().isEmpty()){
            QMessageBox::information(this,"提示","ip或者端口或用户名不合法");
            return;
        }
        int  sex;
        if(ui->isMan->isChecked()){
            sex = 0;
        }else{
            sex = 1;
        }
        GamePanel* panel = new GamePanel(this,ui->ip->text(),ui->port->text().toUShort(),
                                         ui->playerName->text(),sex);
        panel->show();
        //close();
        });
}

LogIn::~LogIn()
{
    delete ui;
}
