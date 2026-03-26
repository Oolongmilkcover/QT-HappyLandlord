#ifndef COUNTDOWN_H
#define COUNTDOWN_H

#include <QTimer>
#include <QWidget>

class CountDown : public QWidget
{
    Q_OBJECT
public:
    explicit CountDown(QWidget *parent = nullptr);
    void showCountDown();
    void stopCountDown();
    void shutdownTimer();
signals:
    void notMuchTime();
    void timeout();
    void stopTimer();
protected:
    void paintEvent(QPaintEvent *ev);

private:
    QPixmap m_clock;
    QPixmap m_number;
    QTimer* m_timer;
    int m_count;
};

#endif // COUNTDOWN_H
