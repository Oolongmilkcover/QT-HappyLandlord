#ifndef SETVOLUME_H
#define SETVOLUME_H

#include <QWidget>

namespace Ui {
class setVolume;
}

class setVolume : public QWidget
{
    Q_OBJECT

public:
    explicit setVolume(QWidget *parent = nullptr);
    ~setVolume();

signals:
    void volumeNumber(int action );

private:
    Ui::setVolume *ui;
};

#endif // SETVOLUME_H
