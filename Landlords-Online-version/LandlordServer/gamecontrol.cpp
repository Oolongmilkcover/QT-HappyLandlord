#include "gamecontrol.h"
#include "playhand.h"
#include <QDebug>
#include <QRandomGenerator>
#include <QTimer>
GameControl::GameControl(QObject *parent)
    : QObject{parent}
{

}

void GameControl::addNewPlayer(QString name, int sex)
{
    UserPlayer* player = new UserPlayer(name,this);
    if(sex==1){
        player->setSex(Player::Sex::Man);
    }else{
        player->setSex(Player::Sex::Woman);
    }
    m_namePlayerMap.insert(name,player);
    m_playerNameMap.insert(player,name);
    m_Players.push_back(player);
    QString sexStr = sex==0?"男":"女";
    QString log;
    log.append("添加新玩家:"+QString(name)+"性别:"+QString(sexStr));
    emit haveLog(log);

}
//number指他是第几个连接服务器之人
void GameControl::removeLogoutPlayer(int number)
{
    // 1. 边界检查
    if (number < 1 || number > (int)m_Players.size()) {
        qDebug() << "removeLogoutPlayer: 无效玩家编号" << number;
        return;
    }
    UserPlayer* p = m_Players[number-1];

    // 2. 查找玩家
    auto it = std::find(m_Players.begin(), m_Players.end(), p);
    if (it == m_Players.end()) {
        qDebug() << "removeLogoutPlayer: 未找到玩家指针";
        return;
    }

    m_Players.erase(it);
    delete p;
    qDebug() << "已清除一名玩家的数据";
}

void GameControl::playerInit()
{

    //按连接服务器的顺序      中左右
    m_middlePlayer = m_Players[0];
    m_leftPlayer = m_Players[1];
    m_rightPlayer = m_Players[2];

    // 头像的显示
    m_leftPlayer->setDirection(Player::Left);
    m_rightPlayer->setDirection(Player::Right);
    m_middlePlayer->setDirection(Player::Right);

    // 出牌顺序
    // user
    m_middlePlayer->setPrevPlayer(m_leftPlayer);
    m_middlePlayer->setNextPlayer(m_rightPlayer);

    // left
    m_leftPlayer->setPrevPlayer(m_rightPlayer);
    m_leftPlayer->setNextPlayer(m_middlePlayer);

    // right
    m_rightPlayer->setPrevPlayer(m_middlePlayer);
    m_rightPlayer->setNextPlayer(m_leftPlayer);

    // 指定当前玩家
    m_currPlayer = m_middlePlayer;
    m_nextToBet = m_middlePlayer;

    // 处理玩家发射出的信号
    connect(m_middlePlayer, &UserPlayer::notifyGrabLordBet, this, &GameControl::onGrabBet);
    connect(m_leftPlayer, &UserPlayer::notifyGrabLordBet, this, &GameControl::onGrabBet);
    connect(m_rightPlayer, &UserPlayer::notifyGrabLordBet, this, &GameControl::onGrabBet);

    // 传递出牌玩家对象和玩家打出的牌
    connect(this, &GameControl::pendingInfo, m_leftPlayer, &Player::storePendingInfo);
    connect(this, &GameControl::pendingInfo, m_rightPlayer, &Player::storePendingInfo);
    connect(this, &GameControl::pendingInfo, m_middlePlayer, &Player::storePendingInfo);

    //处理玩家出牌
    connect(m_leftPlayer, &Player::notifyPlayHand, this, &GameControl::onPlayerHand);
    connect(m_rightPlayer, &Player::notifyPlayHand, this, &GameControl::onPlayerHand);
    connect(m_middlePlayer, &Player::notifyPlayHand, this, &GameControl::onPlayerHand);

}


UserPlayer* GameControl::getLeftPlayer()
{
    return m_leftPlayer;
}

UserPlayer* GameControl::getRightPlayer()
{
    return m_rightPlayer;
}

UserPlayer *GameControl::getMiddlePlayer()
{
    return m_middlePlayer;
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
    Cards cards = m_allCards;
    m_allCards.clear();
    return cards;

}

void GameControl::resetCardData()
{
    //洗牌
    initAllCards();
    //清空所有玩家的牌
    m_rightPlayer->clearCards();
    m_leftPlayer->clearCards();
    m_middlePlayer->clearCards();
    //初始化出牌玩家和牌
    m_pendPlayer = nullptr;
    m_pendCards.clear();
    // 重置叫地主相关状态
    m_curBet = 0;
    m_betRecord.reset();
    //重置当前玩家为第一个叫地主的玩家（后续会被Server的随机覆盖，这里留作保险
    m_currPlayer = m_middlePlayer;
    m_nextToBet = m_middlePlayer;
}


void GameControl::becomeLord(Player* player,int bet)
{
    // 设置角色
    player->setRole(Player::Lord);
    player->getPrevPlayer()->setRole(Player::Farmer);
    player->getNextPlayer()->setRole(Player::Farmer);

    m_curBet = bet;
    player->setPendPlayer(player);
    m_currPlayer = player;
    m_betRecord.reset();
    //准备给客户端发送谁是地主
    QString name  = m_playerNameMap[player];
    QString log = QString("玩家:"+QString(name)+"成为地主");
    emit haveLog(log);
    emit tellWhoBeLoard(name,bet);

}

void GameControl::clearPlayerScore()
{
    m_leftPlayer->setScore(0);
    m_rightPlayer->setScore(0);
    m_middlePlayer->setScore(0);
}

int GameControl::getPlayerMaxBet()
{
    return m_betRecord.bet;
}

void GameControl::onGrabBet(Player* player, int bet)
{
    QString name = m_playerNameMap[player];
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
            emit DispatchCard();
        }else{
            becomeLord(m_betRecord.player,m_betRecord.bet);
        }
        m_betRecord.reset();
        return;
    }
    //4.切换玩家，通知下一个玩家抢地主
    m_nextToBet =player->getNextPlayer();
    QString nextTobet = m_playerNameMap[m_nextToBet];
    emit callNextToBet(name,bet,nextTobet);
}

void GameControl::onPlayerHand(Player *player, Cards &cards,bool isWin)
{
    // 1. 仅有效出牌判断炸弹翻倍（修复：翻倍后本局有效，结束重置）
    if(!cards.isEmpty())
    {
        m_pendPlayer = player;
        m_pendCards = cards;

        PlayHand::HandType type = PlayHand(cards).getHandType();
        if(type==PlayHand::Hand_Bomb || type==PlayHand::Hand_Bomb_Jokers)
        {
            m_curBet *= 2;
        }
    }

    if(isWin)
    {
        qDebug()<<"m_curBet";
        QString log;
        Player* lord = nullptr;
        // 找到地主
        if(m_leftPlayer->getRole() == Player::Lord) lord = m_leftPlayer;
        else if(m_rightPlayer->getRole() == Player::Lord) lord = m_rightPlayer;
        else if(m_middlePlayer->getRole() == Player::Lord) lord = m_middlePlayer;

        // 3. 正确算分逻辑（核心修复）
        if (player->getRole() == Player::Lord)
        {
            log.append("地主获胜！");
            // 地主 +2分，两个农民 -1分
            lord->setScore(lord->getScore() + 2 * m_curBet);
            lord->getNextPlayer()->setScore(lord->getNextPlayer()->getScore()-m_curBet);
            lord->getPrevPlayer()->setScore(lord->getPrevPlayer()->getScore()-m_curBet);

            // 胜利状态
            lord->setWin(true);
            lord->getNextPlayer()->setWin(false);
            lord->getPrevPlayer()->setWin(false);
        }
        else
        {
            log.append("农民获胜！");
            //玩家是农民
            lord->setScore(lord->getScore() - 2 * m_curBet);
            lord->getNextPlayer()->setScore(lord->getNextPlayer()->getScore()+m_curBet);
            lord->getPrevPlayer()->setScore(lord->getPrevPlayer()->getScore()+m_curBet);

            // 胜利状态
            lord->setWin(false);
            lord->getNextPlayer()->setWin(true);
            lord->getPrevPlayer()->setWin(true);

        }
        m_lastWinner = player;
        m_curBet = 1;

        // ====================== 安全输出日志 ======================
        log.append(" 当前分数：");
        if (m_leftPlayer) log.append(m_playerNameMap.value(m_leftPlayer) + ":" + QString::number(m_leftPlayer->getScore()) + "分 ");
        if (m_rightPlayer) log.append(m_playerNameMap.value(m_rightPlayer) + ":" + QString::number(m_rightPlayer->getScore()) + "分 ");
        if (m_middlePlayer) log.append(m_playerNameMap.value(m_middlePlayer) + ":" + QString::number(m_middlePlayer->getScore()) + "分 ");

        emit haveLog(log);
    }
    else
    {
        m_currPlayer = player->getNextPlayer();
    }

}

void GameControl::setNextToBet(Player *player)
{
    m_nextToBet = player;
}

void GameControl::dealPlayHandCards(QJsonArray &cardsArr,QString name, Cards &playHandCards, bool &isWin)
{
    Player* player = m_namePlayerMap[name];
    onPlayerHand(player,playHandCards,isWin);
    Player* nextPlayer = player->getNextPlayer();
    QString nextName = m_playerNameMap[nextPlayer];
    bool timeout;
    if(m_pendPlayer==nullptr||nextPlayer == m_pendPlayer){
        timeout = false;
    }else{
        timeout = true;
    }
    emit sendPlayHand(name,cardsArr,isWin,nextName,timeout);
    qDebug()<<"timeout"<<timeout;
    if(isWin){
        //取得
        QString lefName = m_playerNameMap[m_leftPlayer];
        int lefScore = m_leftPlayer->getScore();
        QString rigName = m_playerNameMap[m_rightPlayer];
        int rigScore = m_rightPlayer->getScore();
        QString midName = m_playerNameMap[m_middlePlayer];
        int midScore = m_middlePlayer->getScore();
        emit sendScore(lefName,lefScore,rigName,rigScore,midName,midScore);
    }
}

void GameControl::goOnGrabBet(const QString &name, int bet)
{
    Player* player = m_namePlayerMap[name];
    onGrabBet(player,bet);
}

Player *GameControl::getLastWinner() const
{
    return m_lastWinner;
}


