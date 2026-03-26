#ifndef GAMECONTROL_H
#define GAMECONTROL_H
#include"userplayer.h"
#include"cards.h"
#include <QMap>
#include <QObject>
#include<player.h>
#include <QJsonArray>
struct BetRecord{
    BetRecord(){
        reset();
    }
    void reset(){
        player = nullptr;
        bet = 0;
        times = 0;
    }
    Player* player;
    int bet;
    int times; //第几次叫地主
};
class GameControl : public QObject
{
    Q_OBJECT
public:
    explicit GameControl(QObject *parent = nullptr);
    //添加一个新玩家
    void addNewPlayer(QString name , int sex);
    //删除一个离线玩家
    void removeLogoutPlayer(int number);
    //初始化玩家
    void playerInit();

    //得到玩家的实例对象
    UserPlayer* getLeftPlayer();
    UserPlayer* getRightPlayer();
    UserPlayer* getMiddlePlayer();
    void setCurrentPlayer(Player* play);
    Player* getCurrentPlayer();

    //得到出牌玩家和打出的牌
    Player* getPendPlayer();
    Cards getPendCards();

    //初始化扑克牌
    void initAllCards();

    //每次发一张牌
    Card takeOneCard();
    //得到最后的三张牌
    Cards getSurplusCards();

    //重置卡牌数据
    void resetCardData();

    //准备叫地主
    void startLordCard();
    //成为地主
    void becomeLord(Player* player,int bet);

    //清空所有玩家的得分
    void clearPlayerScore();

    //得到玩家下注的最高分数
    int getPlayerMaxBet();

    //处理叫地主
    void onGrabBet(Player* player,int bet);

    //处理出牌
    void onPlayerHand(Player* player ,  Cards& cards , bool isWin);

    //下局抢地主的
    void setNextToBet(Player* player);

    //分析牌型并向别的客户端发送消息
    void dealPlayHandCards(QJsonArray& cardsArr,QString name,Cards& playHandCards,bool& isWin);

    void goOnGrabBet(const QString& name,int bet);

    Player* getLastWinner() const;
signals:
    //通知玩家出牌了
    void notifyPlayHand(Player* player,Cards&cards);
    //给其他玩家传递出牌数据
    void pendingInfo(Player* player,Cards&cards);
    //叫下一个人抢地主
    void callNextToBet(const QString& name , int bet , const QString& nextName);
    //通知客户端谁成地主了
    void tellWhoBeLoard(const QString& name,int bet);
    //通知别的客户端出牌情况
    void sendPlayHand(const QString& name,const QJsonArray& cardsArr,bool isWin,const QString &nextName,bool timeout);
    //结算分数
    void sendScore(const QString& lefName,int lefScore,const QString& rigName,int rigScore,const QString& midName,int midScore);
    //通知发牌
    void DispatchCard();
    //传输日志
    void haveLog(const QString& log);
private:
    UserPlayer* m_leftPlayer     = nullptr;
    UserPlayer* m_rightPlayer= nullptr;
    UserPlayer* m_middlePlayer   = nullptr;
    Player* m_currPlayer= nullptr;
    Player* m_nextToBet= nullptr;
    Cards m_pendCards;
    Player* m_pendPlayer= nullptr;
    Cards m_allCards;
    BetRecord m_betRecord;
    int m_curBet = 1;
    QVector<UserPlayer*> m_Players;
    QMap<Player*,QString> m_playerNameMap;
    QMap<QString,Player*> m_namePlayerMap;
    Player* m_lastWinner = nullptr;
};

#endif // GAMECONTROL_H
