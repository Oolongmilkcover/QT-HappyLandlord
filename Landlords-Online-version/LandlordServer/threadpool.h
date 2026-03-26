#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <QObject>

class ThreadPool : public QObject
{
    Q_OBJECT
public:
    explicit ThreadPool(QObject *parent = nullptr);

signals:
};

#endif // THREADPOOL_H
