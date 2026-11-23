#pragma once
#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include "SoundCardEditor.h"   // SoundData vive aquí

class SoundCard : public QFrame {
    Q_OBJECT
public:
    explicit SoundCard(const SoundData& data, QWidget* parent = nullptr);

    int       cardId()   const { return data_.id; }
    QString   cardName() const { return data_.name; }
    QString   soundFile()const { return data_.filePath; }
    float     volume()   const { return data_.volume; }
    bool      hasFile()  const { return !data_.filePath.isEmpty(); }
    SoundData data()     const { return data_; }

    void updateData(const SoundData& data);

signals:
    void triggered(const QString& soundFile, float volume, int id);
    void editRequested(int id);

protected:
    void mousePressEvent(QMouseEvent* e) override;
    void enterEvent(QEnterEvent* e) override;
    void leaveEvent(QEvent* e) override;

private:
    void refreshUI();
    void updateStyle(bool hovered);

    SoundData    data_;
    QWidget*     circleWidget_ = nullptr;
    QLabel*      nameLabel_    = nullptr;
    QLabel*      catLabel_     = nullptr;
    QLabel*      statusDot_    = nullptr;
    QLabel*      badgeLabel_   = nullptr;
    QPushButton* moreBtn_      = nullptr;
};
