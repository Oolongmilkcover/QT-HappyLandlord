#include "endingpanel.h"
#include "gamepanel.h"
#include "playhand.h"
#include "qjsondocument.h"
#include "qjsonobject.h"
#include "ui_gamepanel.h"
#include <QDebug>
#include <QHostAddress>
#include <QPainter>
#include <QPropertyAnimation>
#include <QRandomGenerator>
#include <QSlider>
#include<QMessageBox>
#include<QBigEndianStorageType>
#include <QtEndian>
#include <QThread>
GamePanel::GamePanel(LogIn* login,QString ip,unsigned short port , QString name , int sex , QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::GamePanel)
{
    ui->setupUi(this);

    //1.背景图
    int num = QRandomGenerator::global()->bounded(10);
    QString path = QString(":/images/background-%1.png").arg(num+1);
    m_bkImage.load(path);
    //2.窗口的标题的大小
    this->setWindowTitle("欢乐斗地主");
    this->setFixedSize(1000,650);
    m_tcp = new QTcpSocket(this);
    m_name = name;
    m_sex = sex;
    //定时器每三秒发送心跳包
    m_heartBeat = new QTimer(this);
    connect(m_heartBeat,&QTimer::timeout,this,&GamePanel::heartBeat);
    //连接成功
    connect(m_tcp, &QTcpSocket::connected, this, [=]{
        login->close();
        QMessageBox::information(this,"连接成功","等待所有玩家就绪后开始游戏");
        //首次连接 发送基本信息
        QJsonObject obj;
        obj.insert("cmd","connect");
        obj.insert("msg","连接");

        QJsonObject subObj;
        subObj.insert("player_name",name);
        subObj.insert("player_sex",sex);

        obj.insert("data",subObj);
        QJsonDocument doc(obj);
        QByteArray json = doc.toJson();
        int len = qToBigEndian(json.size());
        QByteArray data((char*)(&len),4);
        data.append(json);
        m_tcp->write(data);
        //启动心跳包定时器
        m_heartBeat->start(3000);
    });
    //连接失败
    connect(m_tcp,&QTcpSocket::errorOccurred,this,[=]{
        QMessageBox::information(this,"提示","服务器连接失败");
        close();
    });
    //断开连接
    connect(m_tcp, &QTcpSocket::disconnected, this, [=]{
        m_heartBeat->stop();
        m_heartBeat->deleteLater();
        QMessageBox::information(this,"连接断开","客户端与服务器已断开连接...");

    });
    //读数据
    connect(m_tcp,&QTcpSocket::readyRead,this,&GamePanel::onReadJson);

    //连接服务器
    m_tcp->connectToHost(QHostAddress(ip),port);

    m_dispatchTimer = new QTimer(this);
    connect(m_dispatchTimer, &QTimer::timeout, this, &GamePanel::onDispatchStep);
}
GamePanel::~GamePanel()
{
    delete ui;
}

void GamePanel::onParseJson(QByteArray json)
{
    QJsonDocument doc = QJsonDocument::fromJson(json);
    if(doc.isObject()){
        QJsonObject obj = doc.object();
        QString status = obj["cmd"].toString();
        //
        if(status == "read"){//读取传输过来的数据进行初始化
            QString leftName,rightName;
            int leftSex,rightSex;
            QJsonObject playerObj = obj["data"].toObject();
            leftName = playerObj["player_name1"].toString();
            m_nameList.push_back(leftName);
            leftSex = playerObj["player_sex1"].toInt();
            rightName = playerObj["player_name2"].toString();
            m_nameList.push_back(rightName);
            rightSex = playerObj["player_sex2"].toInt();
            m_nameList.push_back(m_name);
            //实例化gamecontrol对象
            GameControlInit(m_name,(Player::Sex)m_sex,leftName,(Player::Sex)leftSex,rightName,(Player::Sex)rightSex);
            //5.切割扑克牌图片
            initCardMap();
            //6.初始化游戏的按钮组
            initButtonGroup();
            //7.初始化玩家在窗口的上下文环境
            initPlayContext();
            //8.扑克牌场景初始化
            initGameScene();
            //倒计时窗口初始化
            initCountDown();

            //出牌动画
            m_animation = new AnimationWindow(this);
            //bgm实例化和音量调节
            m_bgm = new BGMControl(this);
            QSlider *volumeSlider = ui->setvolume->findChild<QSlider*>("verticalSlider");
            connect(volumeSlider, &QSlider::valueChanged, this, [=](int value) {
                m_bgm->setVolume(value);
            });

        }else if(status == "deal_cards"){
            m_bgm->startBGM();
            //清空上次的数据
            m_gameCtl->getBetRecord().reset();
            m_playerCardsMap.clear();
            m_gameCtl->getMiddleUser()->setWin(false);
            m_gameCtl->getRightUser()->setWin(false);
            m_gameCtl->getLeftUser()->setWin(false);
            QJsonObject dataObj = obj["data"].toObject();
            //玩家牌
            QJsonArray playersArr = dataObj["players"].toArray();
            for(int i = 0 ; i<playersArr.size();i++){
                QJsonObject playerObj = playersArr[i].toObject();
                QString name = playerObj["player_id"].toString();
                QJsonArray cardsArr = playerObj["cards"].toArray();
                Cards cards ;
                for(int j=0;j<cardsArr.size();j++){
                    QString cardStr = cardsArr[j].toString();
                    Card card = strToCard(cardStr);
                    cards<<card;
                }
                Player* player = m_namePlayerMap[name];
                m_playerCardsMap[player] = cards;
            }
            //地主牌
            Cards lordCards ;
            QJsonArray lordCardsArr = dataObj["landlord_cards"].toArray();
            for(int i = 0;i<lordCardsArr.size();i++){
                QString cardStr = lordCardsArr[i].toString();
                Card card = strToCard(cardStr);
                lordCards<<card;
            }
            m_lordCards = lordCards;
            m_nextCallName = dataObj["nextcall"].toString();

            //取出底牌数据
            CardList last3Card = m_lordCards.toCardList();
            //给底牌设置图片
            for(int i = 0;i<last3Card.size();i++){
                QPixmap front = m_cardMap[last3Card.at(i)]->getImage();
                m_last3Cards[i]->setImage(front,m_cardBackImg);
                m_last3Cards[i]->hide();
            }

            //发牌给对象
            startDispatchCard();

        }else if(status == "call_lord"){
            QJsonObject dataObj = obj["data"].toObject();
            QString name = dataObj["player_id"].toString();
            int bet =  dataObj["point"].toInt();
            QString nextName = dataObj["nextcall"].toString();
            qDebug()<<"call_lord"<<name<<":"<<bet;
            grabLandLord(name,bet);//先刷新玩家叫地主
            if(nextName==m_name){
                ui->btnGroup->selectPanel(ButtonGroup::CallLord, m_gameCtl->getBetRecord().bet);
            }
        }else if(status == "become_lord"){
            QJsonObject dataObj = obj["data"].toObject();
            QString name = dataObj["player_id"].toString();
            int bet =  dataObj["point"].toInt();
            qDebug()<<"become_lord"<<name<<":"<<bet;
            if(name==m_name){
                m_namePlayerMap[name]->setRole(Player::Lord);
            }
            //显示动画
            m_gameCtl->onGrabBet(m_namePlayerMap[name],bet,true);
            //成为地主
            m_gameCtl->becomeLord(m_namePlayerMap[name],m_lordCards);
            //给出牌做准备
            updatePlayerCards(m_namePlayerMap[name]);
            forPlayHand();
            //出牌处理
            if(m_name == name){
                goToPlayHand(false);
            }
        }else if(status == "play_hand"){
            QJsonObject dataObj = obj["data"].toObject();
            QString name = dataObj["player_id"].toString();//这个人出的牌
            if (name != m_name) {
                ui->btnGroup->selectPanel(ButtonGroup::Empty);
            }
            //出的牌
            Cards playHandCards;
            QJsonArray cardsArr = dataObj["cards"].toArray();
            for(int i = 0;i<cardsArr.size();i++){
                QString cardStr = cardsArr[i].toString();
                Card card = strToCard(cardStr);
                playHandCards<<card;
            }
            bool gameover = dataObj["gameover"].toBool();
            QString nextName = dataObj["nextplayhand"].toString();
            m_gameCtl->setCurrentPlayer(m_namePlayerMap[nextName]);
            bool timeout = dataObj["timeout"].toBool();
            //处理别的玩家出牌
            Player* currplayer = m_namePlayerMap[name];
            hidePlayerDropCards(currplayer);
            currplayer->playHand(playHandCards);
            //还有没有下一回合
            if(gameover){
                //本地自动会结束游戏
            }else{
                //看自己是不是要出牌
                if(nextName==m_name){
                    goToPlayHand(timeout);
                }
            }

        }else if(status == "game_over"){
            QJsonObject dataObj = obj["data"].toObject();
            QVector<QString> nameList;
            QVector<int> scoreList;
            for(int i = 1;i<4;i++){
                QString nameStr = QString("name_%1").arg(i);
                QString scoreStr = QString("score_%1").arg(i);
                nameList<<dataObj[nameStr].toString();
                scoreList<<dataObj[scoreStr].toInt();
            }
            for(int i = 0;i<nameList.size();i++){
                Player* player = m_namePlayerMap[nameList[i]];
                player->setScore(scoreList[i]);
            }
            updatePlayerScore();
            //显示最后的分数框
            showEndingScorePanel();
        }
    }
}

void GamePanel::IamReady()
{
    QJsonObject obj;
    obj.insert("cmd","ready");
    QJsonDocument doc(obj);
    QByteArray json = doc.toJson();
    qint32 len = qToBigEndian(json.size());
    QByteArray data((char*)(&len),sizeof(qint32));
    data.append(json);
    sendToServer(data);
}

Card GamePanel::strToCard(QString& str)
{
    if(str.isEmpty()){
        return Card();
    }
    // 按下划线分割成两部分
    QStringList parts = str.split('_');
    Card::CardSuit suit = (Card::CardSuit)parts[0].toInt();
    Card::CardPoint point = (Card::CardPoint)parts[1].toInt();
    return Card(point, suit);
}

QString GamePanel::cardToStr(Card &card)
{
    QString ret;
    ret.append(QString::number((int)card.suit()));
    ret.append("_");
    ret.append(QString::number((int)card.point()));
    return ret;
}


void GamePanel::grabLandLord(QString name,int bet)
{
    if(name ==m_name){
        ui->btnGroup->selectPanel(ButtonGroup::CallLord,m_gameCtl->getBetRecord().bet);
    }else{
        m_gameCtl->onGrabBet(m_namePlayerMap[name],bet);
    }
}

void GamePanel::onsendPointToServer(int bet)
{
    QJsonObject obj;
    obj.insert("cmd","call_lord");
    QJsonObject dataObj;
    dataObj.insert("player_id",m_name);
    dataObj.insert("point",bet);
    obj.insert("data",dataObj);
    QJsonDocument doc(obj);
    QByteArray json = doc.toJson();
    qint32 len = qToBigEndian(json.size());
    QByteArray data((char*)(&len),sizeof(qint32));
    data.append(json);
    sendToServer(data);
}

void GamePanel::forPlayHand()
{
    //隐藏发牌区的底牌和移动的牌
    m_baseCard->hide();
    m_moveCard->hide();
    //显示留给地主的三张底牌
    for(int i = 0;i<m_last3Cards.size();i++){
        m_last3Cards.at(i)->show();
    }

    for(int i = 0;i<m_playerList.size();i++){
        PlayerContext &context = m_contextMap[m_playerList.at(i)];
        //隐藏各个玩家抢地主过程中的提示信息
        context.info->hide();
        //显示各个玩家的头像
        Player* player = m_playerList.at(i);
        QPixmap pixmap = loadRoleImage(player->getSex(),player->getDirection(),player->getRole());
        context.roleImg->setPixmap(pixmap);
        context.roleImg->show();
    }
}

void GamePanel::goToPlayHand(bool timeout)
{
    hidePlayerDropCards(middleUser);
    // 清空之前可能选中的牌
    for (auto it = m_selectCards.begin(); it != m_selectCards.end(); ++it) {
        (*it)->setSeclected(false);
    }
    m_selectCards.clear();
    if(!timeout){
        ui->btnGroup->selectPanel(ButtonGroup::PlayCard);
    }else{
        ui->btnGroup->selectPanel(ButtonGroup::PassOrPlay);
        middleUser->preparePlayHand();
    }
}

void GamePanel::playHandCardsToJson(Cards &cards)
{
    QJsonObject obj;
    obj.insert("cmd","play_hand");
    QJsonObject dataObj;
    dataObj.insert("player_id",m_name);
    QJsonArray cardsArr ;
    CardList cardlist = cards.toCardList();
    for(int i = 0;i<cardlist.size();i++){
        QString CardStr = cardToStr(cardlist[i]);
        cardsArr.append(CardStr);
    }
    dataObj.insert("cards",cardsArr);
    dataObj.insert("isWin",middleUser->getWin());
    obj.insert("data",dataObj);
    QJsonDocument doc(obj);
    QByteArray json = doc.toJson();
    qint32 len = qToBigEndian(json.size());
    QByteArray data((char*)(&len),sizeof(qint32));
    data.append(json);
    sendToServer(data);
}

void GamePanel::onSendContinue()
{
    QJsonObject obj;
    obj.insert("cmd","continue");
    QJsonObject dataObj;
    dataObj.insert("player_id",m_name);
    obj.insert("data",dataObj);
    QJsonDocument doc(obj);
    QByteArray json = doc.toJson();
    qint32 len = qToBigEndian(json.size());
    QByteArray data((char*)(&len),sizeof(qint32));
    data.append(json);
    sendToServer(data);
}

void GamePanel::allInitToRestart()
{
    leftUser->clearCards();
    rightUser->clearCards();
    middleUser->clearCards();

}

void GamePanel::onDispatchStep()
{
    if (m_dispatchCardIndex >= m_dispatchPlayers.size()) {
        // 发牌完成
        m_dispatchTimer->stop();
        m_moveCard->hide();
        m_baseCard->hide();
        m_bgm->stopAssistMusic();
        if(m_nextCallName==m_name){
            grabLandLord(m_nextCallName);
        }
        return;
    }

    Player* player = m_dispatchPlayers[m_dispatchCardIndex];
    QRect cardRect = m_contextMap[player].cardRect;

    // 计算移动方向与步长（核心修复：左侧玩家动画）
    int unitX = 0, unitY = 0;
    const int totalStep = 100; // 总步数
    if (player == m_gameCtl->getLeftUser()) {
        // 左侧玩家：从中间 → 左方（修复方向计算）
        unitX = (cardRect.right() - m_baseCardPos.x()) / totalStep;
    } else if (player == m_gameCtl->getRightUser()) {
        // 右侧玩家：从中间 → 右方
        unitX = (cardRect.left() - m_baseCardPos.x()) / totalStep;
    } else if (player == m_gameCtl->getMiddleUser()) {
        // 下方玩家：从中间 → 下方
        unitY = (cardRect.top() - m_baseCardPos.y()) / totalStep;
    }

    // 实时移动卡牌
    int curX = m_baseCardPos.x() + m_dispatchStep * unitX;
    int curY = m_baseCardPos.y() + m_dispatchStep * unitY;
    m_moveCard->move(curX, curY);

    m_dispatchStep += 10;
    if (m_dispatchStep >= totalStep) {
        // 到达目标位置，发牌
        Card card = m_playerCardsMap[player].takeOneCard();
        player->storeDispatchCard(card);
        m_dispatchStep = 0;
        ++m_dispatchCardIndex;
    }
}

void GamePanel::GameControlInit(QString midName,Player::Sex midSex,
                                QString lefName,Player::Sex lefSex,
                                QString rigName,Player::Sex rigSex)
{
    m_gameCtl = new GameControl(this);
    m_gameCtl->playerInit(midName,midSex,lefName,lefSex,rigName,rigSex);
    //得到三个玩家的实例
    leftUser = m_gameCtl->getLeftUser();
    rightUser = m_gameCtl->getRightUser();
    middleUser = m_gameCtl->getMiddleUser();
    //存储的顺序：左右中
    m_playerList<<leftUser<<rightUser<<middleUser;
    m_namePlayerMap.insert(lefName,leftUser);
    m_playerNameMap.insert(leftUser,lefName);
    m_namePlayerMap.insert(rigName,rightUser);
    m_playerNameMap.insert(rightUser,rigName);
    m_namePlayerMap.insert(midName,middleUser);
    m_playerNameMap.insert(middleUser,midName);

    ui->scorePanel->setmiddleName(midName);
    ui->scorePanel->setLeftName(lefName);
    ui->scorePanel->setRightName(rigName);

    ui->scorePanel->setScores(0,0,0);

    connect(m_gameCtl,&GameControl::notifyGrabLordBet,this,&GamePanel::onGrabLordBet);
    connect(m_gameCtl,&GameControl::notifyPlayHand,this,&GamePanel::onDisposePlayHand);
    connect(m_gameCtl,&GameControl::sendPointToServer,this,&GamePanel::onsendPointToServer);


    connect(leftUser,&Player::notifyPickCards,this,&GamePanel::disposCard);
    connect(rightUser,&Player::notifyPickCards,this,&GamePanel::disposCard);
    connect(middleUser,&Player::notifyPickCards,this,&GamePanel::disposCard);
    connect(m_gameCtl, &GameControl::lordChanged, this, [=](Player* p){
        updatePlayerCards(p);
    });
}


void GamePanel::updatePlayerScore()
{
    ui->scorePanel->setScores(m_playerList[0]->getScore(),
                              m_playerList[1]->getScore(),
                              m_playerList[2]->getScore());
}

void GamePanel::initCardMap()
{
    //1.加载大图
    QPixmap pixmap(":/images/card.png");
    //2.计算每张图片大小
    m_cardSize.setWidth(pixmap.width()/13);
    m_cardSize.setHeight(pixmap.height()/5);

    //3.背景图
    m_cardBackImg = pixmap.copy(2*m_cardSize.width(),4*m_cardSize.height(),m_cardSize.width(),m_cardSize.height());
    //正常花色
    int i = 0,j = 0;
    for(Card::CardSuit suit = Card::Diamond;suit<Card::Suit_End;suit++){
        j = 0;
        for(Card::CardPoint pt=Card::Card_3;pt<Card::Card_SJ;pt++){
            Card card(pt,suit);
            //裁剪图片
            cropImage(pixmap,j*m_cardSize.width(),i*m_cardSize.height(),card);
            j++;
        }
        i++;
    }
    //大小王
    Card c;
    c.setPoint(Card::Card_SJ);
    c.setSuit(Card::Suit_Begin);
    cropImage(pixmap,0,4*m_cardSize.height(),c);

    c.setPoint(Card::Card_BJ);
    cropImage(pixmap,m_cardSize.width(),4*m_cardSize.height(),c);

}

void GamePanel::cropImage(QPixmap &pix, int x, int y,Card& c)
{
    QPixmap sub = pix.copy(x,y,m_cardSize.width(),m_cardSize.height());
    CardPanel* panel = new CardPanel(this);
    panel->setImage(sub,m_cardBackImg);
    panel->setCard(c);
    panel->hide();
    m_cardMap.insert(c,panel);
    connect(panel,&CardPanel::cardSelected,this,&GamePanel::onCardSelected);
}

void GamePanel::initButtonGroup()
{
    ui->btnGroup->initButtons();
    ui->btnGroup->selectPanel(ButtonGroup::Start);
    connect(ui->btnGroup,&ButtonGroup::startGame,this,[=](){
        //界面的初始化 隐藏按钮组
        ui->btnGroup->selectPanel(ButtonGroup::Empty);
        //按下按钮后就是发送准备完毕
        IamReady();
    });
    connect(ui->btnGroup,&ButtonGroup::playHand,this,&GamePanel::onUserPlayHand);
    connect(ui->btnGroup,&ButtonGroup::pass,this,&GamePanel::onUserPass);
    connect(ui->btnGroup,&ButtonGroup::betPoint,this,[=](int bet){
        m_gameCtl->getMiddleUser()->grabLordBet(bet);
        ui->btnGroup->selectPanel(ButtonGroup::Empty);
    });
}

void GamePanel::initPlayContext()
{
    //1.放置玩家扑克牌的区域
    QRect cardsRect[] = {
        //x,y,width,heigth
        QRect(90,130,100,height()-200),                                   //左机器人
        QRect(rect().right()-190,130,100,height()-200),                   //右机器人
        QRect(250,rect().bottom()-120,width()-500,100)                    //当前玩家
    };
    //2.玩家出牌的区域
    const QRect playHandRect[] = {
        //x,y,width,heigth
        QRect(260,150,100,100),                                           //左机器人
        QRect(rect().right()-360,150,100,100),                            //右机器人
        QRect(150,rect().bottom()-290,width()-300,100)                    //当前玩家
    };
    //3.玩家头像显示的位置
    QPoint roleImgRect[] = {
        //x,y,width,heigth
        QPoint(cardsRect[0].left()-80,cardsRect[0].height()/2+20),         //左机器人
        QPoint(cardsRect[1].right()+10,cardsRect[1].height()/2+20),        //右机器人
        QPoint(cardsRect[2].right()-10,cardsRect[2].top()-10),             //当前玩家
    };

    //循环
    int index = m_playerList.indexOf(m_gameCtl->getMiddleUser());
    for(int i=0;i<m_playerList.size();i++){
        PlayerContext context;
        context.align = i==index?Horizontal:Vertical;
        context.isFrontSide = i==index?true:false;
        context.cardRect = cardsRect[i];
        context.playHandRect = playHandRect[i];
        //提示信息
        context.info = new QLabel(this);
        context.info->resize(160,98);
        context.info->hide();
        //显示到出牌区域的中心位置
        QRect rect = playHandRect[i];
        QPoint pt(rect.left()+(rect.width()-context.info->width())/2,
                  rect.top()+(rect.height()-context.info->height())/2);
        context.info->move(pt);
        //玩家的头像
        context.roleImg = new QLabel(this);
        context.roleImg->resize(84,120);
        context.roleImg->hide();
        context.roleImg->move(roleImgRect[i]);

        m_contextMap.insert(m_playerList[i],context);

    }
}

void GamePanel::initGameScene()
{
    //1.发牌区的扑克牌
    m_baseCard = new CardPanel(this);
    m_baseCard->setImage(m_cardBackImg,m_cardBackImg);
    //2.发牌过程中移动的扑克牌
    m_moveCard = new CardPanel(this);
    m_moveCard->setImage(m_cardBackImg,m_cardBackImg);
    //3.最后的三张底牌(用于窗口的显示)
    for(int i = 0;i<3;i++){
        CardPanel* panel = new CardPanel(this);
        panel->setImage(m_cardBackImg,m_cardBackImg);
        m_last3Cards.push_back(panel);
        panel->hide();
    }
    //扑克牌的位置
    m_baseCardPos = QPoint((width()-m_cardSize.width())/2,
                            height()/2-100);
    m_baseCard->move(m_baseCardPos);
    m_moveCard->move(m_baseCardPos);
    int base = (width()-3*m_cardSize.width()-2*10)/2;
    for(int i=0;i<3;i++){
        m_last3Cards[i]->move(base+(m_cardSize.width()+10)*i,20);
    }
}


void GamePanel::startDispatchCard()
{
    //重置每张卡牌的属性
    for(auto it = m_cardMap.begin();it!=m_cardMap.end();it++){
        it.value()->setSeclected(false);
        it.value()->setFrontSide(true);
        it.value()->hide();
    }
    //隐藏三张底牌
    for(int i = 0;i<m_last3Cards.size();i++){
        m_last3Cards[i]->hide();
    }
    //重置玩家的窗口双下文信息
    int index = m_playerList.indexOf(m_gameCtl->getMiddleUser());
    for(int i = 0;i<m_playerList.size();i++){
        m_contextMap[m_playerList[i]].lastCards.clear();
        m_contextMap[m_playerList[i]].info->hide();
        m_contextMap[m_playerList[i]].roleImg->hide();
        m_contextMap[m_playerList[i]].isFrontSide = i==index?true:false;
    }
    //重置所有玩家的卡牌数据
    m_gameCtl->resetCardData();
    //显示中央底牌
    m_baseCard->show();
    //隐藏按钮面板
    ui->btnGroup->selectPanel(ButtonGroup::Empty);
    //播放背景音乐
    m_bgm->playAssistMusic(BGMControl::Dispatch);

    // 构造发牌顺序：三个玩家循环 17 次
    m_dispatchPlayers.clear();
    for (int i = 0; i < 17; ++i) {
        m_dispatchPlayers << m_playerList[0] << m_playerList[1] << m_playerList[2];
    }
    m_dispatchCardIndex = 0;
    m_dispatchStep = 0;
    m_moveCard->show();
    m_dispatchTimer->start(10);
}

void GamePanel::cardMoveStep(Player* player,int curPos)
{
    //得到每个玩家扑克牌展示区域
    QRect cardRect = m_contextMap[player].cardRect;
    //每个玩家的单元步长
    const int unit[] = {
        (m_baseCardPos.x()-cardRect.right())/100,
        (cardRect.left()-m_baseCardPos.x())/100,
        (cardRect.top()-m_baseCardPos.y())/100
    };
    //每次窗口移动的时候每个玩家对应的牌的实时位置
    QPoint pos[]={
        QPoint(m_baseCardPos.x()-curPos * unit[0],m_baseCardPos.y()),
        QPoint(m_baseCardPos.x()+curPos * unit[1],m_baseCardPos.y()),
        QPoint(m_baseCardPos.x(),m_baseCardPos.y()+curPos*unit[2]),
    };
    //移动扑克牌窗口
    int index = m_playerList.indexOf(player);
    m_moveCard->move(pos[index]);
    //临界
    if(curPos==0){
        m_moveCard->show();
    }
    if(curPos>=100){
        m_moveCard->hide();
    }

}

void GamePanel::disposCard(Player* player,const Cards& cards)
{
    Cards& myCard = const_cast<Cards&>(cards);
    CardList list = myCard.toCardList();
    for(int i= 0;i<list.size();i++){
        CardPanel* panel = m_cardMap[list[i]];
        panel->setOwner(player);
    }
    //更新扑克牌
    updatePlayerCards(player);
}

void GamePanel::updatePlayerCards(Player *player)
{
    if (!player|| !m_contextMap.contains(player)) return;
    Cards cards = player->getCards();
    CardList list = cards.toCardList();

    m_cardsRect = QRect();
    m_userCards.clear();
    //取出展示扑克牌的区域
    int cardSpace = 20;
    QRect cardsRect = m_contextMap[player].cardRect;
    for(int i = 0;i<list.size();i++){
        CardPanel* panel = m_cardMap[list.at(i)];
        panel->show();
        panel->raise();
        panel->setFrontSide(m_contextMap[player].isFrontSide);
        //水平or垂直
        if(m_contextMap[player].align == Horizontal){
            int leftX = cardsRect.left()+(cardsRect.width()-(list.size()-1)*cardSpace-panel->width())/2;
            int topY = cardsRect.top()+(cardsRect.height()-m_cardSize.height())/2;
            if(panel->isSelected()){
                topY -=10;
            }
            panel->move(leftX+cardSpace*i,topY);
            m_cardsRect = QRect(leftX,topY,cardSpace*i+m_cardSize.width(),m_cardSize.height());
            int curWidth = 0;
            if(list.size()-1 == i){
                curWidth = m_cardSize.width();
            }else{
                curWidth = cardSpace;
            }
            QRect cardRect(leftX+cardSpace*i,topY,curWidth,m_cardSize.height());
            m_userCards.insert(panel,cardRect);
        }else{
            int leftX = cardsRect.left()+(cardsRect.width()-m_cardSize.width())/2;
            int topY = cardsRect.top()+(cardsRect.height()-(list.size()-1)*cardSpace-panel->height())/2;
            panel->move(leftX,topY+i*cardSpace);
        }
    }
    //显示玩家打出的牌
    //得到当前玩家的出牌区域以及本轮打出的牌
    QRect playCardRect = m_contextMap[player].playHandRect;
    Cards lastCards = m_contextMap[player].lastCards;
    if(!lastCards.isEmpty()){
        int  playSpacing =24;
        CardList lastCardList = lastCards.toCardList();
        auto it = lastCardList.constBegin();
        for(int i = 0;it!=lastCardList.constEnd();it++,i++){
            CardPanel* panel = m_cardMap[*it] ;
            panel->setFrontSide(true);
            panel->raise();
            //将打出的牌移动到出牌区域
            if(m_contextMap[player].align == Horizontal){
                int leftBase = playCardRect.left()+(playCardRect.width()-
                    (lastCardList.size()-1)*playSpacing -panel->width())/2;
                int top = playCardRect.top() +(playCardRect.height()-panel->height())/2;
                panel->move(leftBase+i*playSpacing,top);
            }else{
                int left = playCardRect.left()+(playCardRect.width()-panel->width())/2;
                int top = playCardRect.top();
                panel->move(left,top+i*playSpacing);
            }
            panel->show();
        }
    }
}

void GamePanel::onDispatchCard(Player* player,int curMovePos)
{
    if (curMovePos >= 100) {
        // 给玩家发一张牌
        Card card = m_playerCardsMap[player].takeOneCard();
        player->storeDispatchCard(card);
        return;
    }

    // 移动扑克牌
    cardMoveStep(player, curMovePos);
    int nextPos = qMin(curMovePos + 15, 100);

    QThread::msleep(50);
    onDispatchCard(player,nextPos);

}

void GamePanel::onGrabLordBet(Player *player, int bet, bool flag)
{
    // if(player!=middleUser && m_isBug){
    //     m_isBug = false;
    //     return;
    // }
    // m_isBug = false;
    //显示抢地主的信息提示
    PlayerContext context = m_contextMap[player];
    if(bet==0){
        context.info->setPixmap(QPixmap(":/images/buqinag.png"));
    }else{
        if(flag){
            context.info->setPixmap(QPixmap(":/images/jiaodizhu.png"));
        }else{
            context.info->setPixmap(QPixmap(":/images/qiangdizhu.png"));
        }
        //显示叫地主的分数
        showAnimation(Bet,bet);
    }
    context.info->show();

    m_bgm->playerRobLordMusic(bet,(BGMControl::RoleSex)player->getSex(),flag);
}

void GamePanel::onDisposePlayHand(Player *player,Cards &cards)
{
    //储存玩家打出的牌
    auto it = m_contextMap.find(player);
    it->lastCards = cards;
    //2.根据牌型播放游戏特效
    PlayHand hand(cards);
    PlayHand::HandType type = hand.getHandType();
    if(type == PlayHand::Hand_Plane||type == PlayHand::Hand_Plane_Two_Pair
        ||type == PlayHand::Hand_Plane_Two_Single)
    {
        showAnimation(Plane);
    }else if(type == PlayHand::Hand_Seq_Pair){
        showAnimation(LianDui);
    }else if(type == PlayHand::Hand_Seq_Single){
        showAnimation(Shunzi);
    }else if(type == PlayHand::Hand_Bomb){
        showAnimation(Bomb);
    }else if(type == PlayHand::Hand_Bomb_Jokers){
        showAnimation(JokerBomb);
    }
    //如果玩家打出的是空（不要）,显示提示信息
    if(cards.isEmpty()){
        it->info->setPixmap(QPixmap(":/images/pass.png"));
        it->info->show();
        m_bgm->playPassMusic((BGMControl::RoleSex)player->getSex());
    }else{
        if(m_gameCtl->getPendPlayer() == player||m_gameCtl->getPendPlayer() == nullptr){
            m_bgm->playCardMusic(cards,true,(BGMControl::RoleSex)player->getSex());
        }else{
            m_bgm->playCardMusic(cards,false,(BGMControl::RoleSex)player->getSex());
        }
    }
    //3.更新玩家剩余的牌
    updatePlayerCards(player);
    //4.播放提示音乐
    if(player->getCards().cardCount() == 2){
        m_bgm->playerLastMusic(BGMControl::Last2,(BGMControl::RoleSex)player->getSex());
    }else if(player->getCards().cardCount() == 1){
        m_bgm->playerLastMusic(BGMControl::Last1,(BGMControl::RoleSex)player->getSex());
    }
}

void GamePanel::onCardSelected(Qt::MouseButton button)
{
    if(button == Qt::RightButton){
        return;
    }
    //2.判断是不是自己的牌
    CardPanel* panel = static_cast<CardPanel*>(sender());
    if(panel->getOwner()!=m_gameCtl->getMiddleUser()){
        return;
    }
    //3.保存当前被选中的牌的窗口对象
    m_curSelCard =panel;
    //4.判断参数是左键还是右键
    //设置扑克牌的选中状态
    panel->setSeclected(!panel->isSelected());
    //更新扑克牌在窗口的显示
    updatePlayerCards(panel->getOwner());
    //保存或删除扑克牌窗口对象
    auto it = m_selectCards.find(panel);
    if(it == m_selectCards.end()){
        m_selectCards.insert(panel);
    }else{
        m_selectCards.erase(it);
    }
    m_bgm->playAssistMusic(BGMControl::SelectCard);
}

void GamePanel::onUserPlayHand()
{
    //判断要出的牌是否为空
    if(m_selectCards.isEmpty()){
        return;
    }
    //得到要打出的牌的牌型
    Cards cs;
    for(auto it = m_selectCards.begin();it!=m_selectCards.end();it++){
        Card card = (*it)->getCard();
        cs.add(card);
    }
    PlayHand hand(cs);
    PlayHand::HandType type = hand.getHandType();
    if(type == PlayHand::Hand_Unknown){
        return;
    }
    //判断当前玩家的牌能不能压住上一家
    if(m_gameCtl->getPendPlayer() != m_gameCtl->getMiddleUser()){
        Cards cards = m_gameCtl->getPendCards();
        if(!hand.canBeat(PlayHand(cards))){
            return;
        }
    }
    m_countDown->stopCountDown();
    //通过玩家对象出牌
    m_gameCtl->getMiddleUser()->playHand(cs);
    //出牌成功，准备给服务器发json
    playHandCardsToJson(cs);
    //清空容器
    m_selectCards.clear();
    ui->btnGroup->selectPanel(ButtonGroup::Empty);
}

void GamePanel::onUserPass()
{
    m_countDown->stopCountDown();
    Player* curPlayer = m_gameCtl->getCurrentPlayer();
    Player* userPlayer = m_gameCtl->getMiddleUser();
    if(curPlayer != userPlayer){
        return ;
    }
    Player* pendPlayer = m_gameCtl->getPendPlayer();
    if(pendPlayer == userPlayer || pendPlayer == nullptr){
        return;
    }
    Cards empty;
    userPlayer->playHand(empty);
    for(auto it = m_selectCards.begin();it!=m_selectCards.end();it++){
        (*it)->setSeclected(false);
    }
    m_selectCards.clear();
    updatePlayerCards(userPlayer);
    playHandCardsToJson(empty);
    ui->btnGroup->selectPanel(ButtonGroup::Empty);
}

void GamePanel::hidePlayerDropCards(Player *player)
{
    if (!player || !m_contextMap.contains(player)) {
        return;
    }

    auto& it = m_contextMap[player];
    if (it.lastCards.isEmpty()) {
        it.info->hide();
        return;
    }

    // 安全隐藏卡牌
    CardList list = it.lastCards.toCardList();
    for (auto& card : list) {
        if (m_cardMap.contains(card)) {
            m_cardMap[card]->hide();
        }
    }


    it.lastCards.clear();
}

void GamePanel::showAnimation(AnimationType type, int bet)
{
    switch(type){
    case AnimationType::Shunzi:
    case AnimationType::LianDui:
        m_animation->setFixedSize(250,150);
        m_animation->move((width()-m_animation->width())/2,200);
        m_animation->showSequence((AnimationWindow::Type)type);
        break;
    case AnimationType::Plane:
        m_animation->setFixedSize(800,75);
        m_animation->move((width()-m_animation->width())/2,200);
        m_animation->showPlane();
        break;
    case AnimationType::Bomb:
        m_animation->setFixedSize(180,200);
        m_animation->move((width()-m_animation->width())/2,(height()-m_animation->height())/2-70);
        m_animation->showBomb();
        break;
    case AnimationType::JokerBomb:
        m_animation->setFixedSize(250,200);
        m_animation->move((width()-m_animation->width())/2,(height()-m_animation->height())/2-70);
        m_animation->showJokerBomb();
        break;
    case AnimationType::Bet:
        m_animation->setFixedSize(160,98);
        m_animation->move((width()-m_animation->width())/2,(height()-m_animation->height())/2-140);
        m_animation->showBetScore(bet);
        break;
    }
    m_animation->show();
}

QPixmap GamePanel::loadRoleImage(Player::Sex sex, Player::Direction direct, Player::Role role)
{
    //找图片
    QVector<QString> lordMan;
    QVector<QString> lordWoman;
    QVector<QString> farmerMan;
    QVector<QString> farmerWoman;
    lordMan<<":/images/lord_man_1.png"<<":/images/lord_man_2.png";
    lordWoman<<":/images/lord_woman_1.png"<<":/images/lord_woman_2.png";
    farmerMan<<":/images/farmer_man_1.png"<<":/images/farmer_man_2.png";
    farmerWoman<<":/images/farmer_woman_1.png"<<":/images/farmer_woman_2.png";
    //加载图片 QImage(提供水平镜像函数
    QImage image;
    int random = QRandomGenerator::global()->bounded(2);
    if(sex == Player::Man&&role == Player::Lord){
        image.load(lordMan[random]);
    }else if(sex == Player::Man&&role == Player::Farmer){
        image.load(farmerMan[random]);
    }else if(sex == Player::Woman&&role == Player::Lord){
        image.load(lordWoman[random]);
    }else if(sex == Player::Woman&&role == Player::Farmer){
        image.load(farmerWoman[random]);
    }

    QPixmap pixmap;
    if(direct == Player::Left){
        pixmap = QPixmap::fromImage(image);
    }else{
        pixmap = QPixmap::fromImage(image.mirrored(true,false));
    }
    return pixmap;

}

void GamePanel::showEndingScorePanel()
{
    bool isLoard =  m_gameCtl->getMiddleUser()->getRole() == Player::Lord ? true:false;
    bool isWin = m_gameCtl->getMiddleUser()->getWin();
    if(isWin){
        m_bgm->playEndingMusic(true);
    }else{
        m_bgm->playEndingMusic(false);
    }
    EndingPanel* panel = new EndingPanel(isLoard,isWin,this);
    panel->show();
    panel->move((width()-panel->width())/2,-panel->height());
    panel->setPlayerScore(m_gameCtl->getLeftUser()->getScore(),
                          m_gameCtl->getRightUser()->getScore(),
                          m_gameCtl->getMiddleUser()->getScore());
    QPropertyAnimation *animation =new  QPropertyAnimation(panel,"geometry",this);
    animation->setDuration(1500);
    animation->setStartValue(QRect(panel->x(),panel->y(),panel->width(),panel->height()));
    animation->setEndValue(QRect((width()-panel->width())/2,(height()-panel->height())/2
                                   ,panel->width(),panel->height()));

    animation->setEasingCurve(QEasingCurve(QEasingCurve::OutBounce));
    animation->start();

    connect(panel,&EndingPanel::continueGame,this,[=](){
        panel->close();
        panel->deleteLater();
        animation->deleteLater();
        ui->btnGroup->selectPanel(ButtonGroup::Empty);
        //游戏继续
        onSendContinue();
        allInitToRestart();
    });

}

void GamePanel::initCountDown()
{
    m_countDown = new CountDown(this);
    m_countDown->move((width()-m_countDown->width())/2,(height()-m_countDown->height())/2+30);
    m_countDown->hide(); // 初始隐藏

    connect(m_countDown,&CountDown::notMuchTime,this,[=](){
        m_bgm->playAssistMusic(BGMControl::Alert);
    });
    // 超时自动Pass
    connect(m_countDown,&CountDown::timeout,this,&GamePanel::onUserPass);
    connect(middleUser,&UserPlayer::startCountDown,m_countDown,&CountDown::showCountDown);
}

void GamePanel::heartBeat()
{
    //心跳包
    QJsonObject obj;
    obj.insert("cmd","heartbeat");
    obj.insert("msg",m_name);
    QJsonDocument doc(obj);
    QByteArray json = doc.toJson();
    qint32 len = qToBigEndian(json.size());
    QByteArray data((char*)(&len),4);
    data.append(json);
    sendToServer(data);
}

void GamePanel::sendToServer(QByteArray &data)
{
    if(data.isEmpty()){
        return;
    }
    m_tcp->write(data);
}

void GamePanel::onReadJson()
{
    // 1. 把新数据读到缓存（处理粘包/拆包）
    m_recvBuffer += m_tcp ->readAll();

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
        onParseJson(jsonData);

    }

}


void GamePanel::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.drawPixmap(rect(),m_bkImage);
}

void GamePanel::mouseMoveEvent(QMouseEvent *ev)
{
    if(ev->buttons() & Qt::LeftButton){
        QPoint pt = ev->pos();
        if(!m_cardsRect.contains(pt)){
            m_curSelCard = nullptr;
        }else{
            QList<CardPanel*> list = m_userCards.keys();
            for(int i = 0;i<list.size();i++){
                CardPanel* panel = list.at(i);
                if(m_userCards[panel].contains(pt) && m_curSelCard != panel){
                    panel->clicked();
                    m_curSelCard = panel;
                }
            }
        }
    }
}

void GamePanel::on_pushButton_clicked()
{

}
