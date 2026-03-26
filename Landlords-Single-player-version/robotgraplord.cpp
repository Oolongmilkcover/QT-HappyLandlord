#include "robotgraplord.h"

RobotGrapLord::RobotGrapLord(Player* player,QObject *parent)
    :QThread(parent)
{
    m_player = player;
}

void RobotGrapLord::run()
{
    msleep(1400);
    m_player->thinkCallLord();
}


