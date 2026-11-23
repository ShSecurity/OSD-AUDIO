#pragma once
#include <QMainWindow>
#include <QMap>
#include <QLabel>
#include <QSlider>
#include <QListWidget>
#include <QPushButton>
#include <QLineEdit>
#include <QSettings>
#include <QStackedWidget>
#include <QCheckBox>
#include <QSpinBox>
#include <QSlider>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <memory>
#include "../../include/core/AudioMixer.h"
#include "../../include/core/HotkeyManager.h"
#include "../../include/core/DuckingController.h"
#include "../core/TtsManager.h"
#include "../network/TikTokBridge.h"

class SoundDeckWidget;
class ApplicationDucking;
class QVBoxLayout;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

signals:
    void streamEventReceived(const QString& eventType,
                             const QString& username,
                             double amount);

private slots:
    // Audio
    void onButtonTriggered(const QString& soundFile, float volume, int index);
    void onFileAssigned(int index, const QString& filePath);
    void onMasterVolumeChanged(int value);
    void onDuckValueChanged(int value);
    void onSearchChanged(const QString& text);
    void onStopAll();
    void onDuckManual();

    // TikTok
    void onConnectTikTok();
    void onTikTokConnected(const QString& url);
    void onTikTokDisconnected();
    void onTikTokReconnecting(int attempt, int maxAttempts);
    void onTikTokError(const QString& msg);
    void onComment(const QString& username, const QString& text);
    void onGift(const QString& username, int amount, const QString& giftName);
    void onFollow(const QString& username);
    void onLike(const QString& username, int count);

    // Mixer
    void onPlaybackStarted(const QString& channelId);
    void onPlaybackFinished(const QString& channelId);

    // Navegacion
    void onNavClicked(int page);

protected:
    void closeEvent(QCloseEvent* e) override;

private:
    void setupUI();
    void setupConnections();
    void setupHotkeys();
    void setLibraryHotkeysEnabled(bool enabled);
    void loadSettings();
    void saveSettings();
    void initDatabase();
    void saveDbSetting(const QString& key, const QString& value);
    QString loadDbSetting(const QString& key, const QString& defaultVal = {});

    QWidget* createSidebar();
    // Page 0: Biblioteca
    QWidget* createLibraryPage();
    QWidget* createMixerPanel();
    void refreshDuckingApplications();
    // Page 1: TikTok Live
    QWidget* createTikTokPage();
    // Page 2: Configuracion
    QWidget* createConfigPage();

    void addEventToFeed(const QString& iconChar, const QString& text, const QColor& color);
    void setConnectionStatus(bool connected);
    void setNavActive(int page);

    // Core
    std::unique_ptr<AudioMixer>        mixer_;
    std::unique_ptr<HotkeyManager>     hotkeyManager_;
    std::unique_ptr<DuckingController> duckingController_;
    std::unique_ptr<TikTokBridge>      tiktokBridge_;
    std::unique_ptr<TtsManager>        tts_;
    QSettings    settings_;
    std::unique_ptr<ApplicationDucking> applicationDucking_;
    QSqlDatabase db_;

    // Layout principal
    QStackedWidget* stack_      = nullptr;

    // Sidebar nav buttons (para cambiar estilo activo)
    QPushButton* navBiblio_     = nullptr;
    QPushButton* navTikTok_     = nullptr;
    QPushButton* navConfig_     = nullptr;

    // ── Página 0: Biblioteca ────────────────────────────────────────────────
    SoundDeckWidget* soundDeck_       = nullptr;
    QLineEdit*       searchBox_       = nullptr;
    QPushButton*     togGrid_         = nullptr;
    QPushButton*     togList_         = nullptr;
    QLabel*          nowPlayingLabel_ = nullptr;
    QLabel*          statusMsgLabel_  = nullptr;
    QPushButton*     stopBtn_         = nullptr;
    QPushButton*     libraryHotkeysBtn_ = nullptr;
    QSlider*         masterVolSlider_ = nullptr;
    QLabel*          masterVolLabel_  = nullptr;
    QSlider*         duckSlider_      = nullptr;
    QLabel*          duckLabel_       = nullptr;
    QVBoxLayout*     duckAppsLayout_  = nullptr;
    QMap<QString, QString> duckApps_;

    // ── Página 1: TikTok Live ───────────────────────────────────────────────
    QLineEdit*   serverUrlEdit_ = nullptr;
    QListWidget* tiktokFeed_   = nullptr;
    QLabel*      connDot_      = nullptr;
    QLabel*      connStatus_   = nullptr;
    QPushButton* connBtn_      = nullptr;
    // Contadores de eventos
    QLabel* countComments_ = nullptr;
    QLabel* countGifts_    = nullptr;
    QLabel* countFollows_  = nullptr;
    QLabel* countLikes_    = nullptr;
    int     nComments_ = 0, nGifts_ = 0, nFollows_ = 0, nLikes_ = 0;

    // Mini badge de estado TikTok visible desde la página Biblioteca
    QLabel* miniTikTokDot_    = nullptr;
    QLabel* miniTikTokStatus_ = nullptr;

    // Toggles TTS: activar lectura + incluir nombre
    QCheckBox* ttsReadComments_  = nullptr;
    QCheckBox* ttsNameComments_  = nullptr;
    QCheckBox* ttsReadGifts_     = nullptr;
    QCheckBox* ttsNameGifts_     = nullptr;
    QCheckBox* ttsReadFollows_   = nullptr;
    QCheckBox* ttsNameFollows_   = nullptr;
    QCheckBox* ttsReadLikes_     = nullptr;
    QCheckBox* ttsNameLikes_     = nullptr;
    QCheckBox* ttsReadEmojis_    = nullptr;
    QSpinBox*  ttsMaxQueueSpin_  = nullptr;
    QSlider*   ttsRateSlider_    = nullptr;
    QLabel*    ttsRateLabel_     = nullptr;
};
