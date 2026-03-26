#include "gamecontrol.h"

#include "playhand.h"

#include <QDebug>
#include <QRandomGenerator>
#include <QTimer>
GameControl::GameControl(QObject *parent)
    : QObject{parent}
{

}



void GameControl::playerInit(QString midName,Player::Sex midSex,
                             QString lefName,Player::Sex lefSex,
                             QString rigName,Player::Sex rigSex)
{
    // 对象实例化
    m_middleUser = new UserPlayer(midName, this);
    m_leftUser = new UserPlayer(lefName, this);
    m_rightUser = new UserPlayer(rigName, this);



    // 头像的显示朝向
    m_middleUser->setDirection(Player::Right);
    m_leftUser->setDirection(Player::Left);
    m_rightUser->setDirection(Player::Right);

    // 性别
    m_middleUser->setSex(midSex);
    m_leftUser->setSex(lefSex);
    m_rightUser->setSex(rigSex);

    //设置相对前后方便调用
    m_middleUser->setPrevPlayer(m_leftUser);
    m_middleUser->setNextPlayer(m_rightUser);

    m_leftUser->setPrevPlayer(m_rightUser);
    m_leftUser->setNextPlayer(m_middleUser);

    m_rightUser->setPrevPlayer(m_middleUser);
    m_rightUser->setNextPlayer(m_leftUser);

    // 出牌顺序由服务器通知

    //当前玩家出牌玩家有服务器指示

    // 处理玩家发射出的信号
    connect(m_middleUser, &UserPlayer::notifyGrabLordBet, this, &GameControl::onGrabBet);


    //处理玩家出牌
    connect(m_leftUser, &UserPlayer::notifyPlayHand, this, &GameControl::onPlayerHand);
    connect(m_rightUser, &UserPlayer::notifyPlayHand, this, &GameControl::onPlayerHand);
    connect(m_middleUser, &UserPlayer::notifyPlayHand, this, &GameControl::onPlayerHand);

}

UserPlayer *GameControl::getLeftUser()
{
    return m_leftUser;
}

UserPlayer *GameControl::getRightUser()
{
    return m_rightUser;
}

UserPlayer *GameControl::getMiddleUser()
{
    return m_middleUser;
}



void GameControl::resetCardData()
{
    //清空所有玩家的牌
    m_leftUser->clearCards();
    m_rightUser->clearCards();
    m_middleUser->clearCards();

    m_pendPlayer = nullptr;
    m_pendCards.clear();
    m_currentPlayer = nullptr;
}

void GameControl::becomeLord(Player* player,Cards lordCards)
{
    //将角色设置为地主
    player->setRole(Player::Lord);
    player->getNextPlayer()->setRole(Player::Farmer);
    player->getPrevPlayer()->setRole(Player::Farmer);
    //玩家得到地主牌
    player->storeDispatchCard(lordCards);
    emit lordChanged(player);
    m_betRecord.reset();
}


int GameControl::getPlayerMaxBet()
{
    return m_betRecord.bet;
}

void GameControl::onGrabBet(Player *player, int bet,bool belord)
{
    if (belord) {
        if(bet==0||m_betRecord.bet>=bet){
            emit notifyGrabLordBet(player,bet,false);
            qDebug()<<"bet==0||m_betRecord.bet>=bet";
        }else if(bet>0&&m_betRecord.bet==0){
            //第一个叫地主的玩家
            emit notifyGrabLordBet(player,bet,true);
            qDebug()<<"bet>0&&m_betRecord.bet==0";
        }else{
            //第二三个叫地主的玩家
            emit notifyGrabLordBet(player,bet,false);
            qDebug()<<"bet>0&&m_betRecord.bet==0";
        }
        return;
    }
    //1.通知主界面玩家叫地主了
    if(bet==0||m_betRecord.bet>=bet){
        emit notifyGrabLordBet(player,0,false);
    }else if(bet>0&&m_betRecord.bet==0){
        //第一个叫地主的玩家
        emit notifyGrabLordBet(player,bet,true);
    }else{
        //第二三个叫地主的玩家
        emit notifyGrabLordBet(player,bet,false);
    }
    //3.下注不够三分，进行比较
    if(m_betRecord.bet<bet){
        m_betRecord.bet = bet;
        m_betRecord.player = player;
    }
    if(player == m_middleUser&&!belord){
        emit sendPointToServer(bet);
    }
}

//重点
void GameControl::onPlayerHand(Player *player, Cards &cards)
{

    m_pendPlayer = player;
    m_pendCards = cards;
    //1.将玩家出牌的信号转发给主界面
    emit notifyPlayHand(player,cards);
    //3.如果玩家的牌出完
    if(player->getCards().isEmpty()){
        Player* prev = player->getPrevPlayer();
        Player* next = player->getNextPlayer();
        if(player->getRole() == Player::Lord){
            player->setWin(true);
            prev->setWin(false);
            next->setWin(false);
        }else{
            player->setWin(true);
            if(prev->getRole()==Player::Lord){
                prev->setWin(false);
                next->setWin(true);
            }else{
                prev->setWin(true);
                next->setWin(false);
            }
        }
    }
}

BetRecord GameControl::getBetRecord()
{
    return m_betRecord;
}






void GameControl::setCurrentPlayer(Player *player)
{
    m_currentPlayer = player;
}



Player *GameControl::getCurrentPlayer()
{
    return m_currentPlayer;
}







Player *GameControl::getPendPlayer()
{
    return m_pendPlayer;
}



Cards GameControl::getPendCards()
{
    return m_pendCards;
}

