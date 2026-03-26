#include "userplayer.h"

UserPlayer::UserPlayer(QObject *parent)
    : Player{parent}
{

}

void UserPlayer::prepareCallLord()
{

}

void UserPlayer::preparePlayHand()
{
    emit startCountDown();
}

//
