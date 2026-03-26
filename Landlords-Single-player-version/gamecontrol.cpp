#include "gamecontrol.h"

#include "playhand.h"

#include <QDebug>
#include <QRandomGenerator>
#include <QTimer>
GameControl::GameControl(QObject *parent)
    : QObject{parent}
{

}

void GameControl::playerInit()
{
    // 对象实例化
    m_robotLeft = new Robot("机器人A", this);
    m_robotRight = new Robot("机器人B", this);
    m_user = new UserPlayer("我自己", this);

    // 头像的显示
    m_robotLeft->setDirection(Player::Left);
    m_robotRight->setDirection(Player::Right);
    m_user->setDirection(Player::Right);

    // 性别
    Player::Sex sex;
    sex = (Player::Sex)QRandomGenerator::global()->bounded(2);
    m_robotLeft->setSex(sex);
    sex = (Player::Sex)QRandomGenerator::global()->bounded(2);
    m_robotRight->setSex(sex);
    sex = (Player::Sex)QRandomGenerator::global()->bounded(2);
    m_user->setSex(sex);

    // 出牌顺序
    // user
    m_user->setPrevPlayer(m_robotLeft);
    m_user->setNextPlayer(m_robotRight);

    // left robot
    m_robotLeft->setPrevPlayer(m_robotRight);
    m_robotLeft->setNextPlayer(m_user);

    // right robot
    m_robotRight->setPrevPlayer(m_user);
    m_robotRight->setNextPlayer(m_robotLeft);

    // 指定当前玩家
    m_currPlayer = m_user;
    m_nextToBet = m_user;

    // 处理玩家发射出的信号
    connect(m_user, &UserPlayer::notifyGrabLordBet, this, &GameControl::onGrabBet);
    connect(m_robotLeft, &UserPlayer::notifyGrabLordBet, this, &GameControl::onGrabBet);
    connect(m_robotRight, &UserPlayer::notifyGrabLordBet, this, &GameControl::onGrabBet);

    // 传递出牌玩家对象和玩家打出的牌
    connect(this, &GameControl::pendingInfo, m_robotLeft, &Robot::storePendingInfo);
    connect(this, &GameControl::pendingInfo, m_robotRight, &Robot::storePendingInfo);
    connect(this, &GameControl::pendingInfo, m_user, &Robot::storePendingInfo);

    //处理玩家出牌
    connect(m_robotLeft, &Robot::notifyPlayHand, this, &GameControl::onPlayerHand);
    connect(m_robotRight, &Robot::notifyPlayHand, this, &GameControl::onPlayerHand);
    connect(m_user, &Robot::notifyPlayHand, this, &GameControl::onPlayerHand);

}

Robot *GameControl::getLeftRobot()
{
    return m_robotLeft;
}

Robot *GameControl::getRightRobot()
{
    return m_robotRight;
}

UserPlayer *GameControl::getUserPlayer()
{
    return m_user;
}

void GameControl::setCurrentPlayer(Player *play)
{
    m_currPlayer = play;
}

Player *GameControl::getCurrentPlayer()
{
    return m_currPlayer;
}

Player *GameControl::getPendPlayer()
{
    return m_pendPlayer;
}

Cards GameControl::getPendCards()
{
    return m_pendCards;
}

void GameControl::initAllCards()
{
    m_allCards.clear();
    for(Card::CardPoint p = Card::Card_3;p < Card::Card_SJ ;p++){
        for(Card::CardSuit s = Card::Diamond;s<Card::Suit_End;s++){
            Card c(p,s);
            m_allCards.add(c);
        }
    }
    m_allCards.add(Card(Card::Card_SJ,Card::Suit_Begin));
    m_allCards.add(Card(Card::Card_BJ,Card::Suit_Begin));
}

Card GameControl::takeOneCard()
{
    return m_allCards.takeRandCard();
}

Cards GameControl::getSurplusCards()
{
    return m_allCards;
}

void GameControl::resetCardData()
{
    //洗牌
    initAllCards();
    //清空所有玩家的牌
    m_robotRight->clearCards();
    m_robotLeft->clearCards();
    m_user->clearCards();
    //初始化出牌玩家和牌
    m_pendPlayer = nullptr;
    m_pendCards.clear();
}

void GameControl::startLordCard()
{
    m_nextToBet->prepareCallLord();
    emit playerStatusChanged(m_nextToBet,ThinkingForCallLord);
}

void GameControl::becomeLord(Player* player,int bet)
{
    m_curBet = bet;
    player->setRole(Player::Lord);
    player->getPrevPlayer()->setRole(Player::Farmer);
    player->getNextPlayer()->setRole(Player::Farmer);

    player->setPendPlayer(player);
    m_currPlayer = player;
    player->storeDispatchCard(m_allCards);


    m_betRecord.reset();

    QTimer::singleShot(1000,this,[=]{
        emit gameStatusChanged(PlayingHand);
        emit playerStatusChanged(player,ThinkingForPlayHand);
        m_currPlayer->preparePlayHand();
    });

}

void GameControl::clearPlayerScore()
{
    m_robotLeft->setScore(0);
    m_robotRight->setScore(0);
    m_user->setScore(0);
}

int GameControl::getPlayerMaxBet()
{
    return m_betRecord.bet;
}

void GameControl::onGrabBet(Player *player, int bet)
{
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
    //2.抢地主的玩家是不是下注三分
    if(bet==3){
        //玩家直接成为地主
        becomeLord(player,bet);
        //清空数据
        m_betRecord.reset();
        return;
    }
    //3.下注不够三分，进行比较
    if(m_betRecord.bet<bet){
        m_betRecord.bet = bet;
        m_betRecord.player = player;
    }
    m_betRecord.times++;
    //如果抢了3次
    if(m_betRecord.times==3){
        //重开
        if(m_betRecord.bet==0){
            emit gameStatusChanged(DispatchCard);
        }else{
            becomeLord(m_betRecord.player,m_betRecord.bet);
        }
        m_betRecord.reset();
        return;
    }
    //4.切换玩家，通知下一个玩家抢地主
    m_currPlayer=player->getNextPlayer();
    //发送当前状态依旧是抢地主
    emit playerStatusChanged(m_currPlayer,ThinkingForCallLord);
    m_currPlayer->prepareCallLord();
}

void GameControl::onPlayerHand(Player *player, Cards &cards)
{

    //1.将玩家出牌的信号转发给主界面
    emit notifyPlayHand(player,cards);
    //2.如果不是空，给其他玩家发送信号，保存出牌玩家对象和打出的牌
    if(!cards.isEmpty()){
        m_pendCards = cards;
        m_pendPlayer = player;
        emit pendingInfo(player,cards);
    }
    //如果有炸弹，低分翻倍
    PlayHand::HandType type = PlayHand(cards).getHandType();
    if(type==PlayHand::Hand_Bomb||type==PlayHand::Hand_Bomb_Jokers){
        m_curBet = m_curBet*2;
    }
    //3.如果玩家的牌出完，计算本局游戏分数
    if(player->getCards().isEmpty()){
        Player* prev = player->getPrevPlayer();
        Player* next = player->getNextPlayer();
        if(player->getRole() == Player::Lord){
            player->setScore(player->getScore()+2*m_curBet);
            prev->setScore(prev->getScore()-m_curBet);
            next->setScore(next->getScore()-m_curBet);
            player->setWin(true);
            prev->setWin(false);
            next->setWin(false);
        }else{
            player->setWin(true);
            player->setScore(player->getScore()+m_curBet);
            if(prev->getRole()==Player::Lord){
                prev->setScore(prev->getScore()-2*m_curBet);
                next->setScore(next->getScore()+m_curBet);
                prev->setWin(false);
                next->setWin(true);
            }else{
                prev->setScore(prev->getScore()+m_curBet);
                next->setScore(next->getScore()-2*m_curBet);
                prev->setWin(true);
                next->setWin(false);
            }
        }
        emit playerStatusChanged(player,GameControl::Winning);
    }
    //4.牌没出完，下个玩家
    m_currPlayer = player->getNextPlayer();
    m_currPlayer->preparePlayHand();
    emit playerStatusChanged(m_currPlayer,GameControl::ThinkingForPlayHand);
}

void GameControl::setNextToBet(Player *player)
{
    m_nextToBet = player;
}


