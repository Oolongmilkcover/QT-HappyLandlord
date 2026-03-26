#ifndef PLAYER_H
#define PLAYER_H
#include"cards.h"
#include <QObject>
class Player : public QObject
{
    Q_OBJECT
public:
    enum Role{
        Lord,
        Farmer
    };
    enum Sex{
        Man,
        Woman
    };
    enum Direction{
        Left,
        Right
    };
    explicit Player(QObject *parent = nullptr);
    explicit Player(QString name , QObject *parent = nullptr);

    //ID
    void setName(QString name);
    QString getName();

    //Role
    void setRole(Role role);
    Role getRole();

    //Sex
    void setSex(Sex sex);
    Sex getSex();

    //Direction
    void setDirection (Direction direction);
    Direction getDirection();

    //Score
    void setScore(int score);
    int getScore();

    //isWin
    void setWin(bool flag);
    bool getWin();

    //叫地主/抢地主
    void grabLordBet(int point);

    //储存扑克牌（发牌的时候得到的)
    void storeDispatchCard(const Card& card);
    void storeDispatchCard(const Cards& cards);

    //得到所有的牌
    Cards getCards();

    //清空玩家所有的牌
    void clearCards();

    //出牌
    void playHand(Cards& cards);

    //虚函数
    virtual void prepareCallLord();
    virtual void preparePlayHand();
    virtual void thinkCallLord();  //考虑叫地主
    virtual void thinkPlayHand();

    //设置相对前后玩家
    void setPrevPlayer(Player* player);
    void setNextPlayer(Player* player);
    //设置相对前后玩家
    Player* getPrevPlayer();
    Player* getNextPlayer();

signals:
    //通过已经叫地主下注
    void notifyGrabLordBet(Player* player,int bet,bool flag);
    //通知已经出牌
    void notifyPlayHand(Player* player,Cards& cards);
    //通知已经发牌了
    void notifyPickCards(Player* player,const Cards& cards);


protected:
    QString m_name;
    Role m_role;
    Sex m_sex;
    Direction m_direction;
    int m_score = 0;
    bool m_isWin = false;
    Cards m_cards;

    Player* m_prevPlayer;
    Player* m_nextPlayer;
};

#endif // PLAYER_H
