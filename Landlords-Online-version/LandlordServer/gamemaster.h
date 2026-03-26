#ifndef GAMEMASTER_H
#define GAMEMASTER_H

#include <QThread>
#include <QObject>
#include <QByteArray>
#include <QTcpSocket>
#include "gamecontrol.h"
class GameMaster : public QThread
{
    Q_OBJECT
public:
    explicit GameMaster(QObject *parent = nullptr);
    //解析Json
    void OnParseJson(QByteArray json);
    //玩家掉线，机器人顶替
    void playerDisconnected(int number);
    void run()override;

    //玩家到齐，开始初始化
    void gameInit();
    //发牌
    void onDispatchCard();
    //得到玩家对象
    GameControl* getGameCtl();
    void onGameStart();
    void onPlayerAuthSuccess(QString playerName, int playerSex);
signals:
    void callClientInit(QVector<QString> names,QVector<int> sexs);
    void cardsReady(QVector<Cards>& cardsList);
    void updateClientName(QString oldName, QString newName, QTcpSocket* client);
    void gameControlReady();
    void sendLogToServer(const QString& log);
private:
    GameControl* m_gameCtl;
    QVector<QString> m_playerNames;
    QVector<int> m_playerSex;
    int m_readyNum = 0;
    int m_continueReadyCount = 0;
};

#endif // GAMEMASTER_H
