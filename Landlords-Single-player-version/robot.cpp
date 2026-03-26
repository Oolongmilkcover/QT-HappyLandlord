#include "robot.h"
#include"robotgraplord.h"
#include "strategy.h"
#include "robotplayhand.h"

#include <QDebug>
Robot::Robot(QObject *parent)
    : Player{parent}
{
    m_type = Player::Robot;
    m_gameover = false;
}

void Robot::prepareCallLord()
{
    RobotGrapLord* subThread = new RobotGrapLord(this);
    connect(subThread, &RobotGrapLord::finished, this, [=](){
        qDebug() << "RobotGrapLord 子线程对象析构..." << ", Robot name: " << this->getName();
        subThread->deleteLater();
    });
    subThread->start();
}

void Robot::preparePlayHand()
{
    RobotPlayHand* subThread = new RobotPlayHand(this);
    connect(subThread, &RobotGrapLord::finished, this, [=](){
        qDebug() << "RobotPlayHand 子线程对象析构..." << ", Robot name: " << this->getName();
        subThread->deleteLater();
    });
    subThread->start();
}

void Robot::thinkCallLord()
{
    int weigth = 0;
    Cards tmp = m_cards;
    Strategy st(this , m_cards);
    weigth += st.getRangeCards(Card::Card_SJ,Card::Card_BJ).cardCount()*6;

    QVector<Cards> Array = st.pickOptimalSeqSingles(); //顺子
    weigth += Array.size() * 5;
    tmp.remove(Array);

    Array.clear();
    Array = st.findCardsByCount(4);//炸弹
    weigth += Array.size() * 5;
    tmp.remove(Array);

    weigth += m_cards.pointCount(Card::Card_2)*3;
    Cards card2 = st.getRangeCards(Card::Card_2,Card::Card_2);
    tmp.remove(card2);



    Array.clear();
    Array = Strategy(this , tmp).findCardsByCount(3);//三张
    weigth += Array.size() * 4;
    tmp.remove(Array);



    Array.clear();
    Array =Strategy(this , tmp).findCardsByCount(2);//两种张
    weigth += Array.size();

    if(weigth>=22){
        grabLordBet(3);
    }else if(weigth< 22 && weigth>=18){
        grabLordBet(2);
    }else if(weigth < 18 && weigth>10){
        grabLordBet(1);
    }else{
        grabLordBet(0);
    }

}

void Robot::thinkPlayHand()
{
    if(m_gameover){
        return;
    }
    Strategy st(this,m_cards);
    Cards cs = st.makeStrategy();
    playHand(cs);
}

void Robot::setGameOver(bool flag)
{
    m_gameover = flag;
}
















