#pragma once
#include <QDialog>
#include <QLineEdit>
#include <QKeySequenceEdit>
#include <QSlider>
#include <QLabel>
#include <QPushButton>

struct SoundData {
    int     id          = -1;   // -1 = nuevo (sin guardar)
    QString name;
    QString category;
    QString filePath;
    QString accentColor = "#EBF1FF";
    float   volume      = 1.0f;
    QString shortcut;
};

// Ventana flotante para crear o editar un efecto de sonido
class SoundCardEditor : public QDialog {
    Q_OBJECT
public:
    explicit SoundCardEditor(const SoundData& data, QWidget* parent = nullptr);

    SoundData result()      const { return data_; }
    bool      wantsDelete() const { return deleteRequested_; }

private:
    void pickFile();
    void selectColor(int index);

    SoundData        data_;
    bool             deleteRequested_ = false;
    int              colorIndex_      = 0;

    QLineEdit*       nameEdit_     = nullptr;
    QLineEdit*       catEdit_      = nullptr;
    QLineEdit*       fileEdit_     = nullptr;
    QSlider*         volSlider_    = nullptr;
    QLabel*          volLabel_     = nullptr;
    QKeySequenceEdit* shortcutEdit_ = nullptr;
    QPushButton*     colorBtns_[8] = {};
};
