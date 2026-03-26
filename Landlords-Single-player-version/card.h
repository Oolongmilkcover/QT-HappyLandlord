#ifndef CARD_H
#define CARD_H

#include <QVector>


class Card
{
public:
    //花色
    enum CardSuit{
        Suit_Begin,
        Diamond, //方块
        Club,    //梅花
        Heart,   //红桃
        Spade,    //黑桃
        Suit_End
    };
    //点数
    enum CardPoint{
        Card_Begin,
        Card_3,
        Card_4,
        Card_5,
        Card_6,
        Card_7,
        Card_8,
        Card_9,
        Card_10,
        Card_j,
        Card_Q,
        Card_K,
        Card_A,
        Card_2,
        Card_SJ,  //小王
        Card_BJ, //大王
        Card_End
    };
    Card();
    Card(CardPoint point,CardSuit suit);
    void setPoint(CardPoint point);
    void setSuit(CardSuit suit);
    CardPoint point() const ;
    CardSuit suit() const ;

private:
    CardPoint m_point;
    CardSuit m_suit;
};
//定义别名
using CardList = QVector<Card>;

//对象比较
bool lessSort(const Card& c1 , const Card& c2);
bool greaterSort(const Card& c1 , const Card& c2);

//重载QSet（==）
bool operator==(const Card& left , const Card& right);

//重写全局函数 qHash
uint qHash(const Card& card);

inline Card::CardPoint& operator++(Card::CardPoint& p) {
    // 先转 int 自增，再转回枚举（确保类型安全）
    p = static_cast<Card::CardPoint>(static_cast<int>(p) + 1);
    return p;
}
// 后置 p++
inline Card::CardPoint operator++(Card::CardPoint& p, int) {
    Card::CardPoint temp = p;
    ++p;
    return temp;
}

// CardSuit 同理
inline Card::CardSuit& operator++(Card::CardSuit& s) {
    s = static_cast<Card::CardSuit>(static_cast<int>(s) + 1);
    return s;
}

inline Card::CardSuit operator++(Card::CardSuit& s, int) {
    Card::CardSuit temp = s;
    ++s;
    return temp;
}

inline bool operator<(const Card& lhs, const Card& rhs) {
    // 先比较点数，点数相同再比较花色
    if (lhs.point() != rhs.point()) {
        return lhs.point() < rhs.point();
    }
    return lhs.suit() < rhs.suit();
}


#endif // CARD_H
