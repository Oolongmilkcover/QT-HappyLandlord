#include "GameMaster.h"
#include "server.h"
#include "ui_server.h"
#include <QMessageBox>
#include <QTcpSocket>
#include<QBigEndianStorageType>
#include <QPainter>
#include <QPropertyAnimation>
#include <QRandomGenerator>
#include <QSlider>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QtEndian>
Server::Server(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    Server::setWindowTitle("斗地主联机版服务端");
    Server::setWindowIcon(QIcon(QPixmap("")));
    ui->port->setText("8080");
    m_GM = new GameMaster;
    m_GM->start();

    //建立连接
    m_server = new QTcpServer(this);

    // 绑定新连接信号
    connect(ui->startBtn,&QPushButton::clicked,this,[=]{
        bool flag = start(ui->port->text().toUShort());
        if(flag){
            QMessageBox::information(this,"服务器开启","成功");
            ui->startBtn->setDisabled(true);
        }else{
            QMessageBox::information(this,"服务器开启","失败");
            m_server->deleteLater();
            delete ui;
        }
    });
    connect(this, &Server::playerAuthSuccess, m_GM, &GameMaster::onPlayerAuthSuccess);
    connect(this, &Server::gameStart, m_GM, &GameMaster::onGameStart);
    //添加玩家到三人
    connect(m_server, &QTcpServer::newConnection, this, &Server::onNewConnection);
    //玩家断开连接，用机器人替换
    connect(this,&Server::playerDisconnected,m_GM,&GameMaster::playerDisconnected);
    //编译json信号
    connect(this,&Server::needParseJson,m_GM,&GameMaster::OnParseJson);
    //给客户端发送数据
    connect(m_GM,&GameMaster::callClientInit,this,&Server::callClientInit);
    //接收发的牌P
    connect(m_GM,&GameMaster::cardsReady,this,&Server::onDealCards);
    //打印日志
    connect(m_GM,&GameMaster::sendLogToServer,this,[=](const QString& log){
        ui->log->append(log);
    });
    connect(m_GM, &GameMaster::gameControlReady, this, [=](){
        //叫下一个人抢地主
        connect(m_GM->getGameCtl(),&GameControl::callNextToBet,this,&Server::onCallNextToBet);
        //通知客户端谁成地主了
        connect(m_GM->getGameCtl(),&GameControl::tellWhoBeLoard,this,&Server::onTellWhoBeLoard);
        //通知别的客户端出牌情况
        connect(m_GM->getGameCtl(),&GameControl::sendPlayHand,this,&Server::onSendPlayHand);
        //结算分数
        connect(m_GM->getGameCtl(),&GameControl::sendScore,this,&Server::onSendScore);
    });
}

Server::~Server()
{
    delete ui;
}

bool Server::start(quint16 port)
{
    if (m_server->listen(QHostAddress::Any, port)) {
        return true;
    }
    return false;
}

void Server::broadcastMessage(const QByteArray &data)
{
    for(int i = 0;i<m_fdlist.size();i++){
        QTcpSocket* client = m_fdlist[i];
        // 先判断指针有效，再判断连接状态
        if (client && client->state() == QTcpSocket::ConnectedState) {
            client->write(data);
        }
    }
}

void Server::sendMessageToClient(QTcpSocket *client, const QByteArray &data)
{
    if (client && client->state() == QTcpSocket::ConnectedState) {
        client->write(data);
    }
}

void Server::callClientInit(QVector<QString> playerNames, QVector<int> playerSex)
{

    for(int i = 0;i<playerNames.size();i++){
        QVector<QString> names;
        QVector<int> sexs;
        for(int j = 0;j<playerNames.size();j++){
            if(j!=i){
                names.push_back(playerNames[j]);
                sexs.push_back(playerSex[j]);
            }
        }
        qDebug() << "send to" << playerNames[i] << "other sex:" << sexs[0] << sexs[1];
        QJsonObject obj;
        obj.insert("cmd","read");
        QJsonObject playerObj;
        playerObj.insert("player_name1",names[0]);
        playerObj.insert("player_sex1",sexs[0]);
        playerObj.insert("player_name2",names[1]);
        playerObj.insert("player_sex2",sexs[1]);
        obj.insert("data",playerObj);
        QJsonDocument doc(obj);
        QByteArray json = doc.toJson();
        qint32 len = qToBigEndian(json.size());
        QByteArray data((char*)(&len),sizeof(qint32));
        data.append(json);
        QTcpSocket* client = m_nameToSocket.value(playerNames[i]);
        sendMessageToClient(client, data);
    }


}

void Server::onSendPlayHand(const QString&name, const QJsonArray &cardsArr, bool isWin, const QString&nextName,bool timeout)
{
    QJsonObject obj;
    obj.insert("cmd","play_hand");
    QJsonObject dataObj;
    dataObj.insert("player_id",name);
    dataObj.insert("cards",cardsArr);
    dataObj.insert("gameover",isWin);
    dataObj.insert("nextplayhand",nextName);
    dataObj.insert("timeout",timeout);
    obj.insert("data",dataObj);
    QJsonDocument doc(obj);
    QByteArray json = doc.toJson();
    qint32 len = qToBigEndian(json.size());
    QByteArray data((char*)(&len),sizeof(qint32));
    data.append(json);
    for(int i=0;i<m_Players.size();i++){
        if(m_Players[i]==name){
            continue;
        }
        QTcpSocket* client = m_nameToSocket[m_Players[i]];
        sendMessageToClient(client,data);
    }
}

void Server::onSendScore(const QString& lefName, int lefScore, const QString& rigName, int rigScore, const QString& midName, int midScore)
{
    QJsonObject obj;
    obj.insert("cmd","game_over");
    QJsonObject dataObj;
    dataObj.insert("name_1",lefName);
    dataObj.insert("score_1",lefScore);
    dataObj.insert("name_2",rigName);
    dataObj.insert("score_2",rigScore);
    dataObj.insert("name_3",midName);
    dataObj.insert("score_3",midScore);
    obj.insert("data",dataObj);
    QJsonDocument doc(obj);
    QByteArray json = doc.toJson();
    qint32 len = qToBigEndian(json.size());
    QByteArray data((char*)(&len),sizeof(qint32));
    data.append(json);
    broadcastMessage(data);
}



void Server::onNewConnection()
{
    // 最多3个玩家，满了直接拒绝
    if (m_authPlayerNum >= 3) {
        qDebug() << "房间已满，拒绝新连接";
        QTcpSocket* client = m_server->nextPendingConnection();
        if (client) {
            client->disconnectFromHost();
            client->deleteLater();
        }
        return;
    }

    // 接收新客户端
    QTcpSocket* client = m_server->nextPendingConnection();
    if (!client) return;

    // 加入fd列表，先不绑定名字
    m_fdlist.push_back(client);
    qDebug() << "新客户端连接，等待认证... 当前连接数：" << m_fdlist.size();

    // 绑定信号槽
    connect(client, &QTcpSocket::readyRead, this, &Server::onClientReadyRead);
    connect(client, &QTcpSocket::disconnected, this, &Server::onClientDisconnected);
}

void Server::onClientReadyRead()
{
    QTcpSocket* client = qobject_cast<QTcpSocket*>(sender());
    if (!client || !m_fdlist.contains(client)) {
        qDebug() << "无效的客户端socket";
        return;
    }

    // 1. 把新数据读到缓存（处理粘包/拆包）
    m_recvBuffer += client->readAll();

    // 2. 循环解析完整消息
    while (m_recvBuffer.size() >= (int)sizeof(qint32)) {
        // 先读长度（4字节）
        qint32 len = 0;
        memcpy(&len, m_recvBuffer.constData(), sizeof(qint32));
        len = qFromBigEndian(len); // 转主机字节序

        // 检查缓存是否够一条完整消息
        if (m_recvBuffer.size() < (int)(sizeof(qint32) + len)) {
            break; // 数据不足，等待下一次
        }

        // 3. 提取完整 JSON 数据
        QByteArray jsonData = m_recvBuffer.mid(sizeof(qint32), len);
        m_recvBuffer.remove(0, sizeof(qint32) + len);

        // 4. 先解析JSON，优先处理connect认证指令
        QJsonDocument doc = QJsonDocument::fromJson(jsonData);
        if (doc.isNull() || !doc.isObject()) {
            qDebug() << "无效的JSON数据";
            continue;
        }
        QJsonObject obj = doc.object();
        QString cmd = obj["cmd"].toString();

        // ========== 处理connect认证指令 ==========
        if (cmd == "connect") {
            // 已经认证过的客户端，忽略重复的connect
            if (m_socketToName.contains(client)) {
                qDebug() << "客户端已认证，忽略重复connect";
                continue;
            }

            // 解析客户端传来的真实名字和性别
            QJsonObject dataObj = obj["data"].toObject();
            QString playerName = dataObj["player_name"].toString();
            int playerSex = dataObj["player_sex"].toInt();

            // 名字不能为空，且不能重复
            if (playerName.isEmpty() || m_nameToSocket.contains(playerName)) {
                qDebug() << "玩家名字无效或重复：" << playerName;
                client->disconnectFromHost();
                continue;
            }

            // 完成认证：绑定真实名字和socket
            m_nameToSocket.insert(playerName, client);
            m_socketToName.insert(client, playerName);
            m_Players.push_back(playerName); // 按连接顺序存真实名字
            m_authPlayerNum++;

            qDebug() << "玩家认证成功：" << playerName << "，当前认证人数：" << m_authPlayerNum;

            // 把玩家信息传给GameMaster，添加到GameControl
            emit playerAuthSuccess(playerName, playerSex);

            // 3人都认证完成，初始化游戏
            if (m_authPlayerNum == 3) {
                // 随机决定第一个叫地主的玩家（用真实名字）
                int randomIdx = QRandomGenerator::global()->bounded(3);
                m_nextToBet = m_Players[randomIdx];
                qDebug() << "3人已就绪，首个叫地主玩家：" << m_nextToBet;
                emit gameStart();
            }
            continue; // connect指令处理完，不传给GameMaster
        }

        // ========== 其他指令：只有已认证的客户端能发 ==========
        if (!m_socketToName.contains(client)) {
            qDebug() << "未认证客户端发送指令，断开连接";
            client->disconnectFromHost();
            continue;
        }

        // 认证通过，把其他指令传给GameMaster处理
        emit needParseJson(jsonData);
    }
}

void Server::onClientDisconnected()
{
    QTcpSocket* client = qobject_cast<QTcpSocket*>(sender());
    if (!client) return;

    // 从fd列表移除
    m_fdlist.removeOne(client);

    // 如果是已认证的玩家，清理名字映射
    if (m_socketToName.contains(client)) {
        QString playerName = m_socketToName[client];
        qDebug() << "玩家断开连接：" << playerName;

        // 清理所有映射
        m_nameToSocket.remove(playerName);
        m_socketToName.remove(client);
        m_Players.removeOne(playerName);
        m_authPlayerNum--;

        // 通知GameMaster玩家断开
        int playerIdx = m_Players.indexOf(playerName) + 1; // 兼容原来的编号逻辑
        emit playerDisconnected(playerIdx);
    } else {
        qDebug() << "未认证客户端断开连接";
    }

    // 销毁socket
    client->deleteLater();
    close();
}

void Server::onDealCards(QVector<Cards> &cardsList)
{
    if (cardsList.size() != 4 || m_Players.size() != 3) {
        qDebug() << "onDealCards: 数据异常，需要3个玩家+1组底牌";
        return;
    }



    Player* lastWinner = m_GM->getGameCtl()->getLastWinner();
    if (lastWinner && m_nameToSocket.contains(lastWinner->getName())) {
        m_nextToBet = lastWinner->getName();
    } else {
        int randomIdx = QRandomGenerator::global()->bounded(3);
        m_nextToBet = m_Players[randomIdx];
    }
    // {
    //     "cmd": "deal_cards",
    //     "msg": "发牌完成",
    //     "data": {
    //         "players": [
    //             {
    //                 "player_id": "刘一",
    //                 "cards": ["0_3", "1_5", "2_7", ...]
    //             },
    //             {
    //                 "player_id": "张三",
    //                 "cards": ["0_4", "1_6", "2_8", ...]
    //             },
    //             {
    //                 "player_id": "李四",
    //                 "cards": ["0_5", "1_9", "2_10", ...]
    //             }
    //                     ],
    //         "landlord_cards": ["3_13", "3_14", "0_2"],
    //         "nextcall": "刘一"
    //              }
    // }
    QJsonObject obj;
    obj.insert("cmd","deal_cards");
    QJsonObject dataObj;
    // 构建players数组
    QJsonArray playersArr;
    for(int i = 0;i<m_Players.size();i++){
        QJsonObject playerObj;
        playerObj.insert("player_id",m_Players[i]);
        QJsonArray cardsArr;
        CardList list = cardsList[i].toCardList();
        for(int j = 0;j<list.size();j++){
            QString str = cardToStr(list[j]);
            cardsArr.append(str);
        }
        playerObj.insert("cards",cardsArr);
        playersArr.append(playerObj);
    }
    dataObj.insert("players",playersArr);
    //地主牌
    CardList list = cardsList[3].toCardList();
    QJsonArray cardsArr;
    for(int i = 0;i<list.size();i++){
        QString str = cardToStr(list[i]);
        cardsArr.append(str);
    }
    dataObj.insert("landlord_cards",cardsArr);
    dataObj.insert("nextcall",m_nextToBet);
    obj.insert("data",dataObj);
    QJsonDocument doc(obj);
    QByteArray json = doc.toJson();
    qint32 len = qToBigEndian(json.size());
    QByteArray data((char*)(&len),sizeof(qint32));
    data.append(json);
    broadcastMessage(data);
}

QString Server::cardToStr(const Card &card)
{
    QString ret;
    ret.append(QString::number((int)card.suit()));
    ret.append("_");
    ret.append(QString::number((int)card.point()));
    return ret;
}

Card Server::strToCard(const QString& cardStr)
{
    if(cardStr.isEmpty()){
        return Card();
    }
    // 按下划线分割成两部分
    QStringList parts = cardStr.split('_');
    Card::CardSuit suit = (Card::CardSuit)parts[0].toInt();
    Card::CardPoint point = (Card::CardPoint)parts[1].toInt();
    return Card(point, suit);
}

void Server::onTellWhoBeLoard(const QString& name, int bet)
{
    QJsonObject obj;
    obj.insert("cmd","become_lord");
    QJsonObject dataObj;
    dataObj.insert("player_id",name);
    dataObj.insert("point",bet);
    obj.insert("data",dataObj);
    QJsonDocument doc(obj);
    QByteArray json = doc.toJson();
    qint32 len = qToBigEndian(json.size());
    QByteArray data((char*)(&len),sizeof(qint32));
    data.append(json);
    broadcastMessage(data);
}

void Server::printLog(const QByteArray &log)
{

}

void Server::onCallNextToBet(const QString& name, int bet, const QString& nextName)
{
    QJsonObject obj;
    obj.insert("cmd","call_lord");
    QJsonObject dataObj;
    dataObj.insert("player_id",name);
    dataObj.insert("point",bet);
    dataObj.insert("nextcall",nextName);
    obj.insert("data",dataObj);
    QJsonDocument doc(obj);
    QByteArray json = doc.toJson();
    qint32 len = qToBigEndian(json.size());
    QByteArray data((char*)(&len),sizeof(qint32));
    data.append(json);
    for(int i=0;i<m_Players.size();i++){
        if(m_Players[i]==name){
            continue;
        }
        QTcpSocket* client = m_nameToSocket[m_Players[i]];
        sendMessageToClient(client,data);
    }
}

