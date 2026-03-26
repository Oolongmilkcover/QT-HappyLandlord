#include "gamemaster.h"
#include "server.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>



GameMaster::GameMaster(QObject *parent)
    : QThread(parent)
{

}

void GameMaster::OnParseJson(QByteArray json)
{
    //日志
    QJsonDocument doc = QJsonDocument::fromJson(json);
    if(doc.isObject()){
        QJsonObject obj = doc.object();
        QString status = obj["cmd"].toString();
        //第一阶段会发送玩家基本信息和心跳
        if(status == "heartbeat"){
            qDebug()<<obj["msg"].toString()<<"连接稳定";
        }else if(status == "ready"){
            QString log;
            m_readyNum++;
            if(m_readyNum==3){
                //三人准备好后进入发牌模式并清零准备人数以便游戏结束后再开启
                onDispatchCard();
                m_readyNum = 0;
                log.append("所有玩家都准备就绪，游戏准备开始");
            }
            sendLogToServer(log);
        }else if(status == "call_lord"){
            QString log;
            QJsonObject dataObj = obj["data"].toObject();
            QString name = dataObj["player_id"].toString();
            int bet = dataObj["point"].toInt();
            m_gameCtl->goOnGrabBet(name,bet);
            //日志
            if(bet==0){
                log.append("玩家:"+QString(name)+"没有抢地主");
            }else{
                log.append("玩家:"+QString(name)+"抢地主,抢了"+QString::number(bet)+"分！");
            }
            sendLogToServer(log);
        }else if(status == "play_hand"){
            QString log;
            //任务是向别的客户端发送消息
            QJsonObject dataObj = obj["data"].toObject();
            QString name = dataObj["player_id"].toString(); //
            bool isWin = dataObj["isWin"].toBool(); //
            QJsonArray cardsArr = dataObj["cards"].toArray();
            Cards playHandCards;//未来可拓展为增加机器人对战
            for(int i = 0;i<cardsArr.size();i++){
                QString strCard = cardsArr[i].toString();
                Card card = Server::strToCard(strCard);
                playHandCards<<card;
            }
            //分析牌型并向别的客户端发送消息
            m_gameCtl->dealPlayHandCards(cardsArr,name,playHandCards,isWin);
            log.append("玩家:"+QString(name)+"出了"+QString::number(cardsArr.size())+"张牌");
            sendLogToServer(log);
        }else if(status == "continue"){
            // 记录准备状态，三人准备好后重新发牌
            QString log;
            m_continueReadyCount++;
            if(m_continueReadyCount == 3){
                m_continueReadyCount = 0;
                // 重置游戏数据，重新发牌
                m_gameCtl->resetCardData();
                onDispatchCard();
                log.append("所有玩家都准备就绪，游戏准备开始");
                sendLogToServer(log);
            }
        }
    }

}

void GameMaster::playerDisconnected(int number)
{

    m_gameCtl->removeLogoutPlayer(number);
}

void GameMaster::run()
{
    //子线程开始游戏
    m_gameCtl = new GameControl;
    emit gameControlReady();
    connect(m_gameCtl,&GameControl::DispatchCard,this,&GameMaster::onDispatchCard);
    connect(m_gameCtl,&GameControl::haveLog,this,&GameMaster::sendLogToServer);
    exec();
}

void GameMaster::gameInit()
{
    //通知客户端其余玩家的信息
    emit callClientInit(m_playerNames,m_playerSex);
    m_gameCtl->playerInit();
    m_gameCtl->clearPlayerScore();
}



void GameMaster::onDispatchCard()
{
    //先进行卡牌初始化
    m_gameCtl->resetCardData();
    Cards leftCards;
    Cards middleCards;
    Cards rightCards;
    Cards lordCards;
    QVector<Cards> cardsList;
    //开始分配
    cardsList<<leftCards<<middleCards<<rightCards<<lordCards;
    for(int i = 0;i<17;i++){
        cardsList[0].add(m_gameCtl->takeOneCard());
        cardsList[1].add(m_gameCtl->takeOneCard());
        cardsList[2].add(m_gameCtl->takeOneCard());
    }
    cardsList[3].add(m_gameCtl->getSurplusCards());
    emit cardsReady(cardsList);
}

GameControl* GameMaster::getGameCtl()
{
    return m_gameCtl;
}

void GameMaster::onGameStart()
{
    if (m_playerNames.size() == 3) {
        gameInit();
    }
}

void GameMaster::onPlayerAuthSuccess(QString playerName, int playerSex)
{
    // 保存玩家真实信息
    m_playerNames.push_back(playerName);
    m_playerSex.push_back(playerSex);
    // 添加到GameControl
    m_gameCtl->addNewPlayer(playerName, playerSex);
    qDebug() << "GameMaster添加玩家：" << playerName;
}


