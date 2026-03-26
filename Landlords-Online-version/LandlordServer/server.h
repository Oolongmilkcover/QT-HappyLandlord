#ifndef SERVER_H
#define SERVER_H
#include "GameMaster.h"
#include<QTcpServer>
#include<QTcpServer>
#include <QMainWindow>
QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class Server : public QMainWindow
{
    Q_OBJECT

public:
    Server(QWidget *parent = nullptr);
    ~Server();

    // 启动服务器（指定端口）
    bool start(quint16 port);
    // 广播消息给所有客户端
    void broadcastMessage(const QByteArray& data);
    // 给指定客户端发消息
    void sendMessageToClient(QTcpSocket* client, const QByteArray& data);
    //给指定客户端发送其余玩家信息
    void callClientInit(QVector<QString> playerNames,QVector<int> playerSex);
    //通知别的客户端出牌情况
    void onSendPlayHand(const QString& name, const QJsonArray& cardsArr, bool isWin, const QString&nextName, bool timeout);
    //结算分数
    void onSendScore(const QString& lefName,int lefScore,const QString& rigName,int rigScore,const QString& midName,int midScore);
    //将卡翻译成字符串
    static QString cardToStr(const Card& card);
    //将字符串翻译成卡
    static Card strToCard(const QString& cardStr);


signals:
    //当三人到位或者同时点击继续游戏时开始游戏
    void gameStart();
    //发送需要解析信号
    void needParseJson(const QByteArray& json);
    //玩家断开连接，用机器人顶替
    void playerDisconnected(int number);

    void playerAuthSuccess(const QString& playerName, int playerSex);
private slots:
    // 处理新客户端连接
    void onNewConnection();
    // 处理客户端消息
    void onClientReadyRead();
    // 处理客户端断开连接
    void onClientDisconnected();
    //接收子线程传来的牌
    void onDealCards(QVector<Cards>&cardsList);
    //叫下一个人抢地主
    void onCallNextToBet(const QString& name,int bet,const QString& nextName);
    //通知客户端谁成地主了
    void onTellWhoBeLoard(const QString& name,int bet);
    //打印日志
    void printLog(const QByteArray& log);
private:
    Ui::MainWindow *ui;
    QTcpServer* m_server = nullptr;
    QVector<QString> m_Players; // 真实玩家名字列表（按连接顺序）
    QVector<QTcpSocket*> m_fdlist; // 所有已连接的socket（包括未认证）
    QMap<QString, QTcpSocket*> m_nameToSocket; // 真实名字 → socket（核心映射）
    QMap<QTcpSocket*, QString> m_socketToName; // socket → 真实名字（断开连接用）
    int m_readyNum = 0;
    int m_authPlayerNum = 0;
    QByteArray m_recvBuffer ;
    GameMaster* m_GM;
    QString m_nextToBet = nullptr;
    int m_playhandIndex = 0;
};
#endif // SERVER_H
