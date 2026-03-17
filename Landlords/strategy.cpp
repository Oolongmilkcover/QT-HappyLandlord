#include "strategy.h"
#include "functional"
#include <QMap>
#include <ctime>


Strategy::Strategy(Player *player, const Cards &cards)
{
    m_player = player;
    m_cards = cards;
}

Cards Strategy::makeStrategy()
{
    //得出出牌玩家对象以及打出的牌
    Player* pendPlayer = m_player->getPendPlayer();
    Cards pendCards = m_player->getPendCards();
    //判断上次出牌的玩家是不是自己
    if(pendPlayer == m_player || pendPlayer == nullptr){
        //直接出牌
        //如果是自己，出牌没限制
        return firstPlay();
    }else{
        //如果不是自己需要找比出牌玩家点数大的牌
        PlayHand type(pendCards);
        Cards beatCards = getGreaterCards(type);
        //找到了点数大的牌考虑是否出牌
        bool shouldBeat = whetherToBeat(beatCards);
        if(shouldBeat){
            return beatCards;
        }else{
            return Cards();
        }
    }
    return Cards();
}

Cards Strategy::firstPlay()
{
    //当前玩家只剩下单一的牌型
    PlayHand hand(m_cards);
    if(hand.getHandType()!=PlayHand::Hand_Unknown){
        return m_cards;
    }
    //不是单一的牌型
    //当前玩家手里有没有顺子
    QVector<Cards> optimalSeq = pickOptimalSeqSingles();
    if(!optimalSeq.isEmpty()){
        //得到单牌的数量
        int baseNum = findCardsByCount(1).size();
        //把得到的顺子的集合从玩家手中删除
        Cards save = m_cards;
        save.remove(optimalSeq);
        int lastNum = Strategy(m_player,save).findCardsByCount(1).size();
        if(baseNum > lastNum){
            return optimalSeq[0];
        }
    }
    bool hasPlane,hasTriple,hasPair;
    hasPlane = hasTriple = hasPair = false;
    Cards backup = m_cards;
    //有没有炸弹
    QVector<Cards> bombArray = findCardType(PlayHand(PlayHand::Hand_Bomb,Card::Card_Begin,0),false);
    backup.remove(bombArray);
    //有没有飞机
    QVector<Cards> PlaneArray = Strategy(m_player,backup).findCardType(PlayHand(PlayHand::Hand_Plane,Card::Card_Begin,0),false);
    if(!PlaneArray.isEmpty()){
        hasTriple = true;
        backup.remove(PlaneArray);
    }
    //有没有三张点相同的牌
    QVector<Cards> TripleArray = Strategy(m_player,backup).findCardType(PlayHand(PlayHand::Hand_Triple,Card::Card_Begin,0),false);
    if(!TripleArray.isEmpty()){
        hasTriple = true;
        backup.remove(TripleArray);
    }
    //有没有连对
    QVector<Cards> seqPairArray = Strategy(m_player,backup).findCardType(PlayHand(PlayHand::Hand_Seq_Pair,Card::Card_Begin,0),false);
    if(!seqPairArray.isEmpty()){
        hasPair = true;
        backup.remove(seqPairArray);
    }
    if(hasPair){
        Cards maxPair;
        for(int i = 0;i<seqPairArray.size();i++){
            if(seqPairArray[i].cardCount()>maxPair.cardCount()){
                maxPair = seqPairArray[i];
            }
        }
        return maxPair;
    }
    if(hasPlane){
        //1.飞机两对
        bool twoPairFond = false;
        QVector<Cards> pairArray;
        for(auto point = Card::Card_3;point<=Card::Card_10;point++){
            Cards pair = Strategy(m_player,backup).findSamePointcards(point,2);
            if(!pair.isEmpty()){
                pairArray.push_back(pair);
                if(pairArray.size()==2){
                    twoPairFond = true;
                    break;
                }
            }
        }
        if(twoPairFond){
            Cards tmp = PlaneArray[0];
            tmp<<pairArray[0]<<pairArray[1];
            return tmp;
        }else{
            //2.飞机带两个单
            bool twoSingleFond = false;
            QVector<Cards> SingleArray;
            for(auto point = Card::Card_3;point<=Card::Card_10;point++){
                if(backup.pointCount(point)==1){
                    Cards Single = Strategy(m_player,backup).findSamePointcards(point,1);
                    if(!Single.isEmpty()){
                        SingleArray.push_back(Single);
                        if(SingleArray.size()==2){
                            twoSingleFond = true;
                            break;
                        }
                    }
                }
            }
            if(twoSingleFond){
                Cards tmp = PlaneArray[0];
                tmp<<SingleArray[0]<<SingleArray[1];
                return tmp;
            }else{
                //3.飞机
                return PlaneArray[0];
            }
        }
    }
    if(hasTriple){
        if(PlayHand(TripleArray[0]).getCardPoint()<Card::Card_A){
            for(auto point = Card::Card_3;point<=Card::Card_A;point++){
                int pointCount = backup.pointCount(point);
                if(pointCount==1){
                    Cards single = Strategy(m_player,backup).findSamePointcards(point,1);
                    Cards tmp = TripleArray[0];
                    tmp.add(single);
                    return tmp;
                }else if(pointCount == 2){
                    Cards pair = Strategy(m_player,backup).findSamePointcards(point,2);
                    Cards tmp = TripleArray[0];
                    tmp.add(pair);
                    return tmp;
                }
            }
        }
        //不带副牌
        return TripleArray[0];
    }
    //单牌或者对牌
    Player* nextPlayer = m_player->getNextPlayer();
    if(nextPlayer->getCards().cardCount()==1&&m_player->getRole()!=nextPlayer->getRole()){
        for(Card::CardPoint point = Card::Card_BJ; point >= Card::Card_3; point = static_cast<Card::CardPoint>(point - 1)){
            int pointCount = backup.pointCount(point);
            if(pointCount==1){
                Cards single = Strategy(m_player,backup).findSamePointcards(point,1);
                return single;
            }else if(pointCount==2){
                Cards pair = Strategy(m_player,backup).findSamePointcards(point,2);
                return pair;
            }
        }
    }else{
        for(auto point = Card::Card_3;point<Card::Card_End;point++){
            int pointCount = backup.pointCount(point);
            if(pointCount==1){
                Cards single = Strategy(m_player,backup).findSamePointcards(point,1);
                return single;
            }else if(pointCount==2){
                Cards pair = Strategy(m_player,backup).findSamePointcards(point,2);
                return pair;
            }
        }
    }
    return  Cards();
}

Cards Strategy::getGreaterCards(PlayHand type)
{
    //1.出牌玩家和当前玩家不是一伙的
    Player* pendPlayer = m_player->getPendPlayer();
    if(pendPlayer->getRole()!=m_player->getRole()&&pendPlayer->getCards().cardCount()<=3){
        QVector<Cards> bombs = findCardsByCount(4);
        for(int i = 0;i<bombs.size();i++){
            if(PlayHand(bombs[i]).canBeat(type)){
                return bombs[i];
            }
        }
        //搜索有没有王炸
        Cards sj = findSamePointcards(Card::Card_SJ,1);
        Cards bj = findSamePointcards(Card::Card_BJ,1);
        if(!sj.isEmpty()&&!bj.isEmpty()){
            Cards jokers;
            jokers<<sj<<bj;
            return jokers;
        }
    }
    //2.当前玩家和下一个玩家不是一伙的
    Player* nextPlayer =  m_player->getNextPlayer();
    //将玩家手中的顺子剔除
    Cards remain = m_cards;
    remain.remove(Strategy(m_player,remain).pickOptimalSeqSingles());

    auto beatcard = std::bind([=](Cards& cards){
        QVector<Cards> beatCardsArray = Strategy(m_player,cards).findCardType(type,true);
        if(!beatCardsArray.isEmpty()){
            if(m_player->getRole()!=nextPlayer->getRole() && nextPlayer->getCards().cardCount()<=2){
                return beatCardsArray.back();
            }else{
                return beatCardsArray.front();
            }
        }
        return Cards();
    },std::placeholders::_1);

    Cards cs;
    if(!(cs = beatcard(remain)).isEmpty()){
        return cs;
    }else{
        if(!(cs = beatcard(m_cards)).isEmpty()) return cs;
    }
    return Cards();
}

bool Strategy::whetherToBeat(Cards &cs)
{
    //豆脑的算法
    if(cs.isEmpty()) return false;

    // ========== 1. 基础上下文获取 ==========
    Player* pendPlayer = m_player->getPendPlayer();
    if (!pendPlayer) return false; // 防御性检查

    Player::Role myRole = m_player->getRole();
    Player::Role pendRole = pendPlayer->getRole();
    int myCardCount = m_cards.cardCount();
    int pendCardCount = pendPlayer->getCards().cardCount();
    Cards leftCards = m_cards;
    leftCards.remove(cs);
    int leftCount = leftCards.cardCount();
    PlayHand csHand(cs);
    PlayHand leftHand(leftCards);
    Card::CardPoint csPoint = csHand.getCardPoint();

    // ========== 2. 绝对不出牌的场景（优先级最高） ==========
    // 2.1 拟出牌是2/大小王（核心牌，非关键局不轻易出）
    if (csPoint == Card::Card_2 || csPoint == Card::Card_SJ || csPoint == Card::Card_BJ) {
        // 例外：自己只剩1-2张牌时，用核心牌冲刺
        if (myCardCount <= 2) return true;
        return false;
    }

    // 2.2 三个2带牌/对2（保留核心牌型）
    if ((csHand.getHandType() == PlayHand::Hand_Triple_Single ||
         csHand.getHandType() == PlayHand::Hand_Triple_Pair) &&
        csPoint == Card::Card_2) {
        return false;
    }
    if (csHand.getHandType() == PlayHand::Hand_Pair &&
        csPoint == Card::Card_2 &&
        pendCardCount >= 10 &&
        myCardCount >= 5) {
        return false;
    }

    // 2.3 出牌后剩余牌型极差（无完整牌型且牌数多）
    if (leftHand.getHandType() == PlayHand::Hand_Unknown && leftCount > 8) {
        return false;
    }

    // ========== 3. 必须出牌的场景 ==========
    // 3.1 对手是敌人且只剩≤3张牌（防止对手走牌）
    if (myRole != pendRole && pendCardCount <= 3) {
        return true;
    }

    // 3.2 自己出牌后只剩1张牌（冲刺走牌）
    if (leftCount == 1) {
        return true;
    }

    // 3.3 队友出牌且只剩≤2张牌（不抢队友出牌权）
    if (myRole == pendRole && pendCardCount <= 2) {
        return false;
    }
    // ========== 3.4 新增：地主清场/农民压制逻辑 ==========
    int teammateCount = 0, enemyCount = 0;
    calcTeammateAndEnemyCount(teammateCount, enemyCount);

    // 地主：敌人总牌数≤5，必出
    if (myRole == Player::Lord && enemyCount <= 5) {
        return true;
    }

    // 农民：地主牌数>5，且自己是唯一能压制的，必出
    if (myRole == Player::Farmer && pendRole == Player::Lord &&
        isOnlyCanBeat(cs) && pendCardCount > 5) {
        return true;
    }
    // ========== 4. 灵活决策 ==========
    // 4.1 自己牌数少（≤4张）：优先出牌
    if (myCardCount <= 4) {
        return true;
    }

    // 4.2 拟出牌是小牌（3-5）：优先消耗
    if (csPoint <= Card::Card_5) {
        return true;
    }

    // 4.3 出牌后剩余牌型完整：可以出
    if (leftHand.getHandType() != PlayHand::Hand_Unknown) {
        return true;
    }

    // 4.4 兜底：动态概率随机（更像人类决策）
    // 核心逻辑：牌多→保守（少出牌），牌少→激进（多出牌），中间状态折中
    int prob;
    if (myCardCount > 8) {
        prob = 20; // 手牌多（>8张）：保守，20%概率出牌
    } else if (myCardCount > 4) {
        prob = 50; // 手牌中等（5-8张）：折中，50%概率出牌
    } else {
        prob = 80; // 手牌少（≤4张）：激进，80%概率出牌
    }
    // 额外优化：农民打地主，地主牌少则更激进
    if (myRole == Player::Farmer && pendRole == Player::Lord && pendCardCount <= 5) {
        prob = qMin(prob + 20, 90); // 最多90%，避免100%太机械
    }
    srand(time(nullptr));
    return (rand() % 100) < prob;
}

Cards Strategy::findSamePointcards(Card::CardPoint point, int count)
{
    if(count < 1 || count > 4|| point == Card::Card_Begin || point == Card::Card_End ){
        return Cards();
    }
    //大小王
    if(point ==Card::Card_SJ||point == Card::Card_BJ){
        if(count < 1){
            return Cards();
        }
        Card card;
        card.setPoint(point);
        card.setSuit(Card::Suit_Begin);
        if(m_cards.contains(card)){
            Cards cards;
            cards.add(card);
            return cards;
        }
        return Cards();
    }

    //不是大小王
    int findCount = 0;
    Cards findCards;
    for(Card::CardSuit s = Card::Diamond;s!=Card::Suit_End;s++){
        Card card;
        card.setPoint(point);
        card.setSuit(s);
        if(m_cards.contains(card)){
            findCount++;
            findCards.add(card);
            if(findCount == count){
                return findCards;
            }
        }
    }
    return Cards();
}

QVector<Cards> Strategy::findCardsByCount(int count)
{
    if(count < 1 || count > 4){
        return {};
    }

    QVector<Cards> cardsArray;
    for(Card::CardPoint p  = Card::Card_3;p < Card::Card_End;p++){
        if(m_cards.pointCount(p) == count){
            Cards cs ;
            cs<<findSamePointcards(p,count);
            cardsArray<<cs;
        }
    }
    return cardsArray;
}

Cards Strategy::getRangeCards(Card::CardPoint begin, Card::CardPoint end)
{
    Cards rangeCards;
    for(Card::CardPoint p = begin ; p < end;p++){
        int count = m_cards.pointCount(p);
        Cards cs = findSamePointcards(p,count);
        rangeCards << cs;
    }
    return rangeCards;
}

QVector<Cards> Strategy::findCardType(PlayHand hand, bool beat)
{
    PlayHand::HandType type = hand.getHandType();
    Card::CardPoint point = hand.getCardPoint();
    int extra = hand.getExtra();

    //确定起始点数
    Card::CardPoint beginPoint = beat?Card::CardPoint(point+1):Card::Card_3;
    switch(type){
    case PlayHand::Hand_Single:
        return getCards(beginPoint,1);
    case PlayHand::Hand_Pair:
        return getCards(beginPoint,2);
    case PlayHand::Hand_Triple:
        return getCards(beginPoint,3);
    case PlayHand::Hand_Triple_Single:
        return getTripleSingleOrPair(beginPoint,PlayHand::Hand_Single);
    case PlayHand::Hand_Triple_Pair:
        return getTripleSingleOrPair(beginPoint,PlayHand::Hand_Pair);
    case PlayHand::Hand_Plane:
        return getPlane(beginPoint);
    case PlayHand::Hand_Plane_Two_Single:
        return getPlane2SingleOr2Pair(beginPoint,PlayHand::Hand_Single);
    case PlayHand::Hand_Plane_Two_Pair:
        return getPlane2SingleOr2Pair(beginPoint,PlayHand::Hand_Pair);
    case PlayHand::Hand_Seq_Pair:
    {
        CardInfo info;
        info.begin = beginPoint;
        info.end = Card::Card_Q;
        info.number = 2;
        info.base = 3;
        info.extra = extra;
        info.beat = beat;
        info.getSeq = &Strategy::getBaseSeqPair;
        return getSepPairOrSeqSingle(info);
    }
    case PlayHand::Hand_Seq_Single:
    {
        CardInfo info;
        info.begin = beginPoint;
        info.end = Card::Card_10;
        info.number = 1;
        info.base = 5;
        info.extra = extra;
        info.beat = beat;
        info.getSeq = &Strategy::getBaseSeqSingle;
        return getSepPairOrSeqSingle(info);
    }
    case PlayHand::Hand_Bomb:
        return getBomb(beginPoint);
    default:
        return {};
    }
}

void Strategy::pickSeqSingles(QVector<QVector<Cards>> &allSeqRecord,QVector<Cards>&seqSingle,const Cards &cards)
{
    //1.得到所有顺子的组合
    QVector<Cards> allSeq = Strategy(m_player,cards).findCardType(PlayHand(PlayHand::Hand_Seq_Single,Card::Card_Begin,0),false);
    if(allSeq.isEmpty()){
        //结束递归，将满足条件的顺子传给调用者
        allSeqRecord<<seqSingle;
        return;
    }else{ //2.对顺子进行筛选
        //遍历得到的所有顺子
        Cards saveCards = cards;
        for(int i =0;i<allSeq.size();i++){
            //将顺子取出
            Cards aScheme = allSeq.at(i);
            //将顺子从用户手中删除
            Cards temp = saveCards;
            temp.remove(aScheme);
            QVector<Cards> seqArray = seqSingle;
            seqArray<<aScheme;
            //检测还有没有其他顺子
            pickSeqSingles(allSeqRecord,seqArray,temp);

        }
    }
    //2.对顺子进行筛选

}

QVector<Cards> Strategy::pickOptimalSeqSingles()
{
    QVector<QVector<Cards>> seqRecord;
    QVector<Cards> seqSingles;
    Cards save = m_cards;
    save.remove(findCardsByCount(4));
    save.remove(findCardsByCount(3));
    pickSeqSingles(seqRecord,seqSingles,save);
    if(seqRecord.isEmpty()){
        return QVector<Cards>();
    }
    //遍历容器
    QMap<int,int> seqMarks;
    for(int i = 0;i<seqRecord.size();i++){
        Cards backupCards = m_cards;
        QVector<Cards> seqArray = seqRecord[i];
        backupCards.remove(seqArray);

        //判断剩下的单牌数量，数量越少,顺子的组合就越合理
        QVector<Cards> singleArray = Strategy(m_player,backupCards).findCardsByCount(1);
        CardList cardlist ;
        for(int j = 0;j<singleArray.size();j++){
            cardlist<<singleArray[j].toCardList();
        }
        //找点数相对大的顺子
        int mark = 0;
        for(int k = 0;k<cardlist.size();k++){
            mark += cardlist[k].point() + 15;
        }
        seqMarks.insert(i,mark);
    }
    //遍历map
    int comMark = 1000;
    int value = 0;
    auto it = seqMarks.constBegin();
    for(;it!=seqMarks.constEnd();it++){
        if(it.value()<comMark){
            comMark = it.value();
            value = it.key();
        }
    }
    return seqRecord[value];
}
//找到指定数量的牌 返回cards数组
QVector<Cards> Strategy::getCards(Card::CardPoint point, int number)
{
    QVector<Cards> findCardsArray;

    // 入参合法性校验：数量必须是1-4，点数必须有效
    if (number < 1 || number > 4 || point <= Card::Card_Begin || point >= Card::Card_End) {
        return findCardsArray;
    }

    // 遍历所有点数（从指定起点到最大点数）
    for(Card::CardPoint pt = point; pt < Card::Card_End; pt++){
        // 获取当前点数的总牌数
        int totalCount = m_cards.pointCount(pt);

        // 核心防护逻辑：
        // 1. 炸弹（4张同点）：仅当明确要取4张时才使用，否则跳过（不拆炸弹）
        // 2. 非炸弹牌：只要数量≥需要的数量，就可以取（支持三带一/飞机等牌型拆分）
        if (totalCount == 4 && number != 4) {
            continue; // 跳过炸弹，不拆分
        }
        if (totalCount >= number) {
            // 取指定数量的牌（不会超过总数量）
            Cards cs = findSamePointcards(pt, number);
            if (!cs.isEmpty()) {
                findCardsArray << cs;
            }
        }
    }

    return findCardsArray;
}

QVector<Cards> Strategy::getTripleSingleOrPair(Card::CardPoint begin, PlayHand::HandType type)
{
    //找到点数相同的三张牌
    QVector<Cards> findCardArray = getCards(begin,3);
    if(!findCardArray.isEmpty()){
        //将找到的牌从拷贝的宗派删除再搜索
        Cards remainCards = m_cards;
        remainCards.remove(findCardArray);
        //搜索牌型
        //搜索单牌或成对的牌
        Strategy st(m_player,remainCards);
        QVector<Cards> cardsArray = st.findCardType(PlayHand(type,Card::Card_Begin,0),false);
        if(!cardsArray.isEmpty()){
            //将找的牌和三张点数相同的牌进行组合
            for(int i = 0;i<findCardArray.size();i++){
                findCardArray[i].add(cardsArray.at(0));
            }
        }else{
            findCardArray.clear();
        }
    }

    //返回数组

    return findCardArray;
}

QVector<Cards> Strategy::getPlane(Card::CardPoint begin)
{
    QVector<Cards> findCardArray ;
    for(auto point = begin;point<= Card::Card_K;point++){
        //根据点数和数量进行搜索
        Cards prevCards = findSamePointcards(point,3);
        Cards nextCards = findSamePointcards((Card::CardPoint)(point+1),3);
        if(!prevCards.isEmpty() && !nextCards.isEmpty()){
            Cards tmp ;
            tmp<< prevCards << nextCards;
            findCardArray << tmp;
        }
    }
    return findCardArray;
}

QVector<Cards> Strategy::getPlane2SingleOr2Pair(Card::CardPoint begin, PlayHand::HandType type)
{
    //找飞机
    QVector<Cards> findCardArray = getPlane(begin);
    if(!findCardArray.isEmpty()){
        //将找到的牌从拷贝的宗派删除再搜索
        Cards remainCards = m_cards;
        remainCards.remove(findCardArray);
        //搜索牌型
        //搜索单牌或成对的牌
        Strategy st(m_player,remainCards);
        QVector<Cards> cardsArray = st.findCardType(PlayHand(type,Card::Card_Begin,0),false);
        if(cardsArray.size() >= 2){
            //将找的牌和三张点数相同的牌进行组合
            for(int i = 0;i<findCardArray.size();i++){
                Cards tmp;
                tmp<<cardsArray[0]<<cardsArray[1];
                findCardArray[i].add(tmp);
            }
        }else{
            findCardArray.clear();
        }
    }
    return findCardArray;
}

QVector<Cards> Strategy::getSepPairOrSeqSingle(CardInfo &info)
{
    QVector<Cards> findCardsArray;
    if(info.beat){
        for(auto point = info.begin;point <=info.end;point++){
            bool found = true;
            Cards seqCards;
            for(int i = 0;i<info.extra;i++){
                //基于点数和数量进行搜索
                Cards cards = findSamePointcards((Card::CardPoint)(point+i),info.number);
                if(cards.isEmpty()||(point+info.extra >= Card::Card_2)){
                    found =false;
                    seqCards.clear();
                    break;
                }else{
                    seqCards<<cards;
                }

            }
            if(found){
                findCardsArray<<seqCards;
                return findCardsArray;
            }
        }
    }else{
        for(auto point = info.begin;point <=info.end;point++){

            Cards baseSep = (this->*info.getSeq)(point);
            if(baseSep.isEmpty())continue;
            //连对储存到容器
            findCardsArray << baseSep;

            int followed = info.base;

            Cards alreadyFollowedCards; //储存后续找到的满足条件的连对

            while(true){
                //新的起点
                Card::CardPoint followedPoint = Card::CardPoint(point+followed);
                //判断是否超出上线
                if(followedPoint>=Card::Card_2) break;

                Cards follwedCards = findSamePointcards(followedPoint,info.number);

                if(follwedCards.isEmpty()) break;

                alreadyFollowedCards<<follwedCards;
                Cards newSeq = baseSep;
                newSeq<<alreadyFollowedCards;
                findCardsArray<<newSeq;
                followed++;
            }

        }
    }
    return findCardsArray;
}

Cards Strategy::getBaseSeqPair(Card::CardPoint point)
{
    Cards cards0 = findSamePointcards(point,2);
    Cards cards1 = findSamePointcards((Card::CardPoint)(point+1),2);
    Cards cards2 = findSamePointcards((Card::CardPoint)(point+2),2);
    Cards baseSeq;
    if(!cards0.isEmpty()&& !cards1.isEmpty() && !cards2.isEmpty()){
        baseSeq << cards0 << cards1 << cards2;
    }
    return baseSeq;
}

Cards Strategy::getBaseSeqSingle(Card::CardPoint point)
{
    Cards cards0 = findSamePointcards(point,1);
    Cards cards1 = findSamePointcards((Card::CardPoint)(point+1),1);
    Cards cards2 = findSamePointcards((Card::CardPoint)(point+2),1);
    Cards cards3 = findSamePointcards((Card::CardPoint)(point+3),1);
    Cards cards4 = findSamePointcards((Card::CardPoint)(point+4),1);

    Cards baseSeq;
    if(!cards0.isEmpty()&& !cards1.isEmpty() && !cards2.isEmpty()&& !cards3.isEmpty()&& !cards4.isEmpty()){
        baseSeq << cards0 << cards1 << cards2 << cards3 <<cards4;
    }
    return baseSeq;
}

QVector<Cards> Strategy::getBomb(Card::CardPoint begin)
{
    QVector<Cards> findcardsArray;
    for(auto point = begin;point < Card::Card_End;point++){
        Cards cs = findSamePointcards(point,4);
        if(!cs.isEmpty()){
            findcardsArray<< cs;
        }
    }
    return findcardsArray;
}

void Strategy::calcTeammateAndEnemyCount(int &teammateCount, int &enemyCount)
{
    teammateCount = 0;
    enemyCount = 0;
    if (!m_player) return;

    // 遍历上下家（斗地主3人：自己+左+右）
    Player* prev = m_player->getPrevPlayer();
    Player* next = m_player->getNextPlayer();
    Player::Role myRole = m_player->getRole();

    if (prev) {
        if (prev->getRole() == myRole) {
            teammateCount += prev->getCards().cardCount();
        } else {
            enemyCount += prev->getCards().cardCount();
        }
    }
    if (next) {
        if (next->getRole() == myRole) {
            teammateCount += next->getCards().cardCount();
        } else {
            enemyCount += next->getCards().cardCount();
        }
    }
}

bool Strategy::isOnlyCanBeat(Cards &cs)
{
    // 简化版：农民阵营，检查队友是否有更大的牌
    if (m_player->getRole() != Player::Farmer) return false;

    Player* teammate = nullptr;
    Player* prev = m_player->getPrevPlayer();
    Player* next = m_player->getNextPlayer();
    if (prev && prev->getRole() == Player::Farmer && prev != m_player) {
        teammate = prev;
    } else if (next && next->getRole() == Player::Farmer && next != m_player) {
        teammate = next;
    }

    if (!teammate) return true;

    // 检查队友是否有比cs大的牌（复用你的PlayHand逻辑）
    PlayHand csHand(cs);
    Cards teammateCards = teammate->getCards();
    Strategy tmpStrategy(teammate, teammateCards);
    QVector<Cards> biggerCards = tmpStrategy.findCardType(csHand, true);
    return biggerCards.isEmpty();
}























