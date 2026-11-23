#pragma once
#include <QWidget>
#include <QGridLayout>
#include <QScrollArea>
#include <QPushButton>
#include <vector>
#include "SoundCard.h"

class HotkeyManager;

class SoundDeckWidget : public QWidget {
    Q_OBJECT
public:
    explicit SoundDeckWidget(QWidget* parent = nullptr);

    // Nombre de la conexión SQLite (debe estar abierta antes de llamar)
    void setDbConnection(const QString& connectionName);

    void filterByName(const QString& query);
    void registerHotkeys(HotkeyManager* hm);
    void setHotkeysEnabled(bool enabled);
    void setListView(bool list);

signals:
    void buttonTriggered(const QString& soundFile, float volume, int id);
    void fileAssigned(int id, const QString& filePath);  // notificación a MainWindow

private slots:
    void onCardTriggered(const QString& soundFile, float volume, int id);
    void onEditRequested(int id);
    void onAddClicked();

private:
    void loadFromDb();
    void rebuildGrid();
    void saveToDb(const SoundData& data);
    void deleteFromDb(int id);

    void applyHotkeys();
    void rebuildList();

    bool          listMode_   = false;
    bool          hotkeysEnabled_ = true;
    QString       dbConn_;
    HotkeyManager* hotkeyMgr_ = nullptr;
    QGridLayout*  grid_       = nullptr;
    QWidget*      container_  = nullptr;
    QScrollArea*  scroll_     = nullptr;
    QPushButton*  addBtn_     = nullptr;
    QWidget*      listWidget_ = nullptr;
    std::vector<SoundCard*> cards_;
};
