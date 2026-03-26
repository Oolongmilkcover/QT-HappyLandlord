#include "player.h"
#include<QMetaMethod>
#include <QDebug>

Player::Player(QObject *parent)
    : QObject{parent}
{

}

Player::Player(QString name, QObject *parent)
    : Player(parent)
{
    m_name = name;
}

void Player::setName(QString name)
{
    m_name = name;
}

QString Player::getName()
{
    return m_name;
}

void Player::setRole(Role role)
{
    m_role = role;
}

void Player::setSex(Sex sex)
{
    m_sex = sex;
}

void Player::setDirection(Direction direction)
{
    m_direction = direction;
}

void Player::setType(Type type)
{
    m_type = type;
}

void Player::setScore(int score)
{
    m_score = score;
}

int Player::getScore()
{
    return m_score;
}

void Player::setWin(bool flag)
{
    m_isWin = flag;
}

bool Player::getWin()
{
    return m_isWin;
}

void Player::setPrevPlayer(Player *player)
{
    m_prev = player;
}

void Player::setNextPlayer(Player *player)
{
    m_next = player;
}

Player *Player::getPrevPlayer()
{
    return m_prev;
}

Player *Player::getNextPlayer()
{
    return m_next;
}

void Player::grabLordBet(int point)
{
    emit notifyGrabLordBet(this,point);
}

void Player::storeDispatchCard(const Card &card)
{
    m_cards.add(card);
    Cards  cs ;
    cs.add(card);
    emit notifyPickCards(this,cs);
}

void Player::storeDispatchCard(const Cards &cards)
{
    m_cards.add(cards);
    emit notifyPickCards(this,cards);
}

Cards Player::getCards()
{
    return m_cards;
}

void Player::clearCards()
{
    m_cards.clear();
}

void Player::playHand(Cards &cards)
{
    m_cards.remove(cards);
    emit notifyPlayHand(this,cards);
}



void Player::setPendingInfo(Player *player, const Cards &cards)
{
    m_pendPlayer = player;
    m_pendCards = cards;
}

void Player::setPendPlayer(Player* player)
{
    m_pendPlayer = player;
}


Player *Player::getPendPlayer()
{
    return m_pendPlayer;
}

Cards Player::getPendCards()
{
    return m_pendCards;
}

void Player::prepareCallLord()
{

}

void Player::preparePlayHand()
{

}

void Player::thinkCallLord()
{

}

void Player::thinkPlayHand()
{

}

void Player::storePendingInfo(Player *player, Cards &cards)
{
    m_pendPlayer = player;
    m_pendCards = cards;
}

Player::Type Player::getType()
{
    return m_type;
}

Player::Direction Player::getDirection()
{
    return m_direction;
}

Player::Sex Player::getSex()
{
    return m_sex;
}

Player::Role Player::getRole()
{
    return m_role;
}
