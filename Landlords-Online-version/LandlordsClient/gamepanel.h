#ifndef GAMEPANEL_H
#define GAMEPANEL_H
#include "cardpanel.h"
#include"gamecontrol.h"
#include <QLabel>
#include <QMainWindow>
#include <QMap>
#include <QMouseEvent>
#include <QTcpSocket>
#include <QTimer>
#include "animationwindow.h"
#include "countdown.h"
#include "bgmcontrol.h"
#include<QJsonArray>
#include<QJsonDocument>
#include<QJsonObject>
#include<QDebug>
#include<QJsonValue>
#include"login.h"
QT_BEGIN_NAMESPACE
namespace Ui {
class GamePanel;
}
QT_END_NAMESPACE

class GamePanel : public QMainWindow
{
    Q_OBJECT

public:
    GamePanel(LogIn* login,QString ip,unsigned short port , QString name , int sex , QWidget *parent = nullptr);
    ~GamePanel();
    enum AnimationType{
        Shunzi,
        LianDui,
        Plane,
        JokerBomb,
        Bomb,
        Bet
    };
    //初始化游戏控制类信息
    void GameControlInit(QString midName,Player::Sex midSex,
                         QString lefName,Player::Sex lefSex,
                         QString rigName,Player::Sex rigSex);
    //更新分数面板的分数
    void updatePlayerScore();
    //切割并存储图片
    void initCardMap();
    //裁剪图片
    void cropImage(QPixmap& pix,int x,int y,Card& c);
    //初始化游戏按钮组
    void initButtonGroup();
    //初始化玩家在窗口的上下文环境
    void initPlayContext();
    //初始化游戏场景
    void initGameScene();
    //发牌
    void startDispatchCard();
    //移动扑克牌
    void cardMoveStep(Player* player,int curPos);
    //处理分发得到的扑克牌
    void disposCard(Player* player,const Cards& cards);
    //更新扑克牌在窗口的显示
    void updatePlayerCards(Player* player);
    //发牌动画
    void onDispatchCard(Player* player,int curMovePos);
    //处理玩家抢地主
    void onGrabLordBet(Player* player,int bet,bool flag);
    //处理玩家出牌
    void onDisposePlayHand(Player* player,Cards& cards);
    //处理玩家选牌
    void onCardSelected(Qt::MouseButton button);
    //处理用户玩家出牌
    void onUserPlayHand();
    //用户玩家放弃出牌
    void onUserPass();
    //隐藏玩家打出的牌
    void hidePlayerDropCards(Player* player);
    //显示特效动画
    void showAnimation(AnimationType type , int bet = 0);
    //加载玩家头像
    QPixmap loadRoleImage(Player::Sex sex,Player::Direction direct,Player::Role role);
    //显示玩家最终得分
    void showEndingScorePanel();
    //初始化闹钟倒计时
    void initCountDown();
    //心跳包
    void heartBeat();
    //给服务器发信息
    void sendToServer(QByteArray&data);
    //处理数据咯readJson
    void onReadJson();
    //解析数据
    void onParseJson(QByteArray json);
    //准备完毕给服务器发ready
    void IamReady();
    //从json中转换str为card
    Card strToCard(QString& str);
    //将card转为str
    QString cardToStr(Card& card);
    //处理玩家抢地主，是不是玩家，是不是第一次
    void grabLandLord(QString name,int bet = 0);
    //给服务器发送赌注json
    void onsendPointToServer(int bet);
    //给出牌做准备
    void forPlayHand();
    //出牌处理
    void goToPlayHand(bool timeout);
    //将的出牌和人发给服务器
    void playHandCardsToJson(Cards& cards);
    //游戏继续
    void onSendContinue();
    //全部重置
    void allInitToRestart();
    void onDispatchStep();
protected:
    void paintEvent(QPaintEvent* ev);
    void mouseMoveEvent(QMouseEvent* ev);
private slots:
    void on_pushButton_clicked();

private:
    enum CardAlign{ Horizontal,Vertical};
    struct PlayerContext{
        //1.玩家扑克牌的显示的区域
        QRect cardRect;
        //2.出牌的区域
        QRect playHandRect;
        //3.扑克牌的对齐方式(水平or垂直)
        CardAlign align;
        //4.扑克牌显示正面还是背面
        bool isFrontSide;
        //5.游戏过程中的提示信息,比如：不出
        QLabel* info;
        //6.玩家的头像
        QLabel* roleImg;
        //7.玩家打出的牌
        Cards lastCards;
    };

    Ui::GamePanel *ui;
    QByteArray m_recvBuffer;
    UserPlayer* leftUser = nullptr;
    UserPlayer* rightUser = nullptr;
    UserPlayer* middleUser = nullptr;
    QPixmap m_bkImage;
    GameControl *m_gameCtl;
    QVector<Player*> m_playerList;
    QMap<Card,CardPanel*> m_cardMap;
    QMap<Player*,PlayerContext> m_contextMap;
    QSize m_cardSize;
    QPixmap m_cardBackImg;
    CardPanel* m_baseCard;
    QPoint m_baseCardPos;
    CardPanel* m_moveCard;
    QVector<CardPanel*> m_last3Cards;
    AnimationWindow* m_animation;
    CardPanel* m_curSelCard;
    QSet<CardPanel*> m_selectCards;
    QRect m_cardsRect;
    QHash<CardPanel*,QRect> m_userCards;
    CountDown* m_countDown;
    BGMControl* m_bgm;
    QTcpSocket* m_tcp;
    QString m_name;
    int m_sex;
    Cards m_lordCards;
    QMap<Player*,Cards> m_playerCardsMap;
    QVector<QString> m_nameList;//左右中
    QMap<QString,Player*> m_namePlayerMap;//玩家名字与对象对照表
    QMap<Player*,QString> m_playerNameMap;//玩家对象与名字对照表
    int m_betTimes = 0;
    QTimer* m_heartBeat;
    QTimer* m_dispatchTimer;        // 发牌动画定时器
    int m_dispatchStep;              // 当前步数 (0~100)
    int m_dispatchCardIndex;         // 当前发牌序号 (0~50)
    QVector<Player*> m_dispatchPlayers; // 发牌顺序（循环）
    QString m_nextCallName;
    bool m_isBug = true;

};
#endif // GAMEPANEL_H
