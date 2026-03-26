#ifndef GAMECONTROL_H
#define GAMECONTROL_H
#include"userplayer.h"
#include"cards.h"
#include <QObject>
#include<player.h>
struct BetRecord{
    BetRecord(){
        reset();
    }
    void reset(){
        player = nullptr;
        bet = 0;
        times = 0;
    }
    Player* player = nullptr;
    int bet = 0;
    int times = 0;
};
class GameControl : public QObject
{
    Q_OBJECT
public:
    //游戏状态
    enum GameStatus{
        DispatchCard, // 发牌状态
        CallingLord,    //叫地主状态
        PlayingHand   //出牌状态
    };

    //玩家状态
    enum PlayerStatus{
        ThinkingForCallLord, //考虑是否叫地主
        ThinkingForPlayHand,  //考虑出牌
        Winning //赢咯
    };

    explicit GameControl(QObject *parent = nullptr);

    //初始化玩家 中左右
    void playerInit(QString midName,Player::Sex midSex,
                    QString lefName,Player::Sex lefSex,
                    QString rigName,Player::Sex rigSex);

    //得到玩家的实例对象
    UserPlayer* getLeftUser();
    UserPlayer* getRightUser();
    UserPlayer* getMiddleUser();

    void setCurrentPlayer(Player* play);
    Player* getCurrentPlayer();

    //得到出牌玩家和打出的牌
    Player* getPendPlayer();
    Cards getPendCards();


    //重置卡牌数据
    void resetCardData();

    //成为地主
    void becomeLord(Player* player,Cards lordCards);

    //清空所有玩家的得分
    void clearPlayerScore();

    //得到玩家下注的最高分数
    int getPlayerMaxBet();

    //处理叫地主
    void onGrabBet(Player* player,int bet,bool beLord = false);

    //处理出牌
    void onPlayerHand(Player* player ,  Cards& cards);

    //下局抢地主的
    void setNextToBet(Player* player);

    //清理betRecord
    BetRecord getBetRecord();

signals:
    //通知玩家抢地主了
    void notifyGrabLordBet(Player* player , int bet,bool flag);
    //通知玩家出牌了
    void notifyPlayHand(Player* player,Cards&cards);
    //发出赌注信号让gamepanel发给服务器
    void sendPointToServer(int bet);
    void lordChanged(Player* player);
private:
    UserPlayer* m_leftUser = nullptr;
    UserPlayer* m_rightUser= nullptr;
    UserPlayer* m_middleUser= nullptr;
    Player* m_currentPlayer = nullptr;
    BetRecord m_betRecord;
    bool m_isWin = false;
    Player* m_pendPlayer = nullptr;
    Cards m_pendCards;
};

#endif // GAMECONTROL_H
