#include "MainWindow.h"
#include "SoundDeckWidget.h"
#include "Icon.h"
#include "core/ApplicationDucking.h"
#include <QPixmap>
#include <QIcon>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QSlider>
#include <QListWidget>
#include <QPushButton>
#include <QButtonGroup>
#include <QLineEdit>
#include <QComboBox>
#include <QFrame>
#include <QScrollArea>
#include <QCloseEvent>
#include <QMessageBox>
#include <QListWidgetItem>
#include <QStackedWidget>
#include <QTableWidget>
#include <QHeaderView>
#include <QKeySequenceEdit>
#include <QCheckBox>
#include <QSpinBox>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QTimer>

// ── Paleta ────────────────────────────────────────────────────────────────────
static const char* C_WHITE  = "#FFFFFF";
static const char* C_PANEL  = "#FBFCFF";
static const char* C_ACTIVE = "#EBF1FF";
static const char* C_STATUS = "#EAF0FC";
static const char* C_ACCENT = "#3064E8";
static const char* C_BORDER = "#E3E9F3";
static const char* C_TEXT   = "#1A2340";
static const char* C_MUTED  = "#7B88A0";
// ─────────────────────────────────────────────────────────────────────────────

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , mixer_(std::make_unique<AudioMixer>(this))
    , hotkeyManager_(std::make_unique<HotkeyManager>(this))
    , duckingController_(std::make_unique<DuckingController>(mixer_.get(), this))
    , tiktokBridge_(std::make_unique<TikTokBridge>(this))
    , tts_(std::make_unique<TtsManager>(this))
    , settings_("OSD-AUDIO", "OmniStreamDeck")
    , applicationDucking_(std::make_unique<ApplicationDucking>(settings_))
{
    setWindowTitle("OmniStream Deck");
    setWindowIcon(QIcon(":/icons/icono-32.png"));
    setMinimumSize(1050, 650);
    resize(1200, 740);
    setFont(QFont("Segoe UI", 10));

    setStyleSheet(QString(R"(
        QMainWindow, QWidget { background-color: %1; color: %2;
            font-family: 'Segoe UI', Arial, sans-serif; }
        QScrollBar:vertical { background: transparent; width: 6px; }
        QScrollBar::handle:vertical { background: #D0D8E8; border-radius: 3px; min-height: 24px; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical,
        QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: none; height: 0; }
    )").arg(C_WHITE).arg(C_TEXT));

    initDatabase();
    setupUI();
    setupConnections();
    setupHotkeys();
    loadSettings();
}

MainWindow::~MainWindow() { saveSettings(); }

// ══════════════════════════════════════════════════════════════════════════════
//  BASE DE DATOS SQLITE
// ══════════════════════════════════════════════════════════════════════════════
void MainWindow::initDatabase() {
    QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dataDir);
    db_ = QSqlDatabase::addDatabase("QSQLITE", "osd_audio");
    db_.setDatabaseName(dataDir + "/osd_settings.db");
    if (db_.open()) {
        QSqlQuery q(db_);
        q.exec("CREATE TABLE IF NOT EXISTS settings (key TEXT PRIMARY KEY, value TEXT)");
        q.exec("CREATE TABLE IF NOT EXISTS sounds ("
               "id INTEGER PRIMARY KEY AUTOINCREMENT,"
               "name TEXT NOT NULL,"
               "category TEXT DEFAULT '',"
               "file_path TEXT DEFAULT '',"
               "accent_color TEXT DEFAULT '#EBF1FF',"
               "volume REAL DEFAULT 1.0,"
               "sort_order INTEGER DEFAULT 0,"
               "shortcut TEXT DEFAULT '')");
        // Migración: añadir columna si ya existía la tabla sin ella
        q.exec("ALTER TABLE sounds ADD COLUMN shortcut TEXT DEFAULT ''");
    }
}

void MainWindow::saveDbSetting(const QString& key, const QString& value) {
    if (!db_.isOpen()) return;
    QSqlQuery q(db_);
    q.prepare("INSERT OR REPLACE INTO settings (key, value) VALUES (?, ?)");
    q.addBindValue(key); q.addBindValue(value);
    q.exec();
}

QString MainWindow::loadDbSetting(const QString& key, const QString& defaultVal) {
    if (!db_.isOpen()) return defaultVal;
    QSqlQuery q(db_);
    q.prepare("SELECT value FROM settings WHERE key = ?");
    q.addBindValue(key);
    if (q.exec() && q.next()) return q.value(0).toString();
    return defaultVal;
}

// ══════════════════════════════════════════════════════════════════════════════
//  SETUP
// ══════════════════════════════════════════════════════════════════════════════
void MainWindow::setupUI() {
    auto* central = new QWidget(this);
    setCentralWidget(central);
    auto* root = new QHBoxLayout(central);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* div = new QFrame();
    div->setFrameShape(QFrame::VLine);
    div->setFixedWidth(1);
    div->setStyleSheet(QString("QFrame { color: %1; }").arg(C_BORDER));

    stack_ = new QStackedWidget();
    stack_->addWidget(createLibraryPage());   // 0 – Biblioteca
    stack_->addWidget(createTikTokPage());    // 1 – TikTok Live
    stack_->addWidget(createConfigPage());    // 2 – Configuración
    stack_->setCurrentIndex(0);

    root->addWidget(createSidebar());
    root->addWidget(div);
    root->addWidget(stack_, 1);
}

void MainWindow::setupConnections() {
    connect(soundDeck_, &SoundDeckWidget::buttonTriggered,
            this, &MainWindow::onButtonTriggered);
    connect(soundDeck_, &SoundDeckWidget::fileAssigned,
            this, &MainWindow::onFileAssigned);

    connect(masterVolSlider_, &QSlider::valueChanged,
            this, &MainWindow::onMasterVolumeChanged);
    connect(duckSlider_, &QSlider::valueChanged,
            this, &MainWindow::onDuckValueChanged);
    connect(searchBox_, &QLineEdit::textChanged,
            this, &MainWindow::onSearchChanged);
    connect(stopBtn_,  &QPushButton::clicked, this, &MainWindow::onStopAll);
    connect(connBtn_,  &QPushButton::clicked, this, &MainWindow::onConnectTikTok);

    connect(mixer_.get(), &AudioMixer::playbackStarted,
            this, &MainWindow::onPlaybackStarted);
    connect(mixer_.get(), &AudioMixer::playbackFinished,
            this, &MainWindow::onPlaybackFinished);

    connect(tiktokBridge_.get(), &TikTokBridge::connected,
            this, &MainWindow::onTikTokConnected);
    connect(tiktokBridge_.get(), &TikTokBridge::disconnected,
            this, &MainWindow::onTikTokDisconnected);
    connect(tiktokBridge_.get(), &TikTokBridge::reconnecting,
            this, &MainWindow::onTikTokReconnecting);
    connect(tiktokBridge_.get(), &TikTokBridge::connectionError,
            this, &MainWindow::onTikTokError);
    connect(tiktokBridge_.get(), &TikTokBridge::commentReceived,
            this, &MainWindow::onComment);
    connect(tiktokBridge_.get(), &TikTokBridge::giftReceived,
            this, &MainWindow::onGift);
    connect(tiktokBridge_.get(), &TikTokBridge::followReceived,
            this, &MainWindow::onFollow);
    connect(tiktokBridge_.get(), &TikTokBridge::likeReceived,
            this, &MainWindow::onLike);
}

void MainWindow::setupHotkeys() {
    libraryHotkeysBtn_->setChecked(settings_.value("libraryHotkeysEnabled", true).toBool());
    setLibraryHotkeysEnabled(libraryHotkeysBtn_->isChecked());
    soundDeck_->registerHotkeys(hotkeyManager_.get());
    connect(libraryHotkeysBtn_, &QPushButton::toggled, this, [this](bool enabled) {
        setLibraryHotkeysEnabled(enabled);
        settings_.setValue("libraryHotkeysEnabled", enabled);
    });
}

void MainWindow::setLibraryHotkeysEnabled(bool enabled) {
    soundDeck_->setHotkeysEnabled(enabled);
    libraryHotkeysBtn_->setText(enabled ? "Pausar atajos" : "Activar atajos");
    libraryHotkeysBtn_->setToolTip(enabled
        ? "Atajos de la biblioteca activos. Pulsa para pausarlos."
        : "Atajos de la biblioteca pausados. Pulsa para activarlos.");
}

void MainWindow::loadSettings() {
    // Conectar SoundDeck a SQLite (carga los efectos guardados automáticamente)
    soundDeck_->setDbConnection("osd_audio");

    masterVolSlider_->setValue(settings_.value("masterVolume", 80).toInt());
    duckSlider_->setValue(settings_.value("duckPercent", 20).toInt());
    serverUrlEdit_->setText(settings_.value("tiktokUser", "").toString());

    // Cargar toggles TTS desde SQLite
    if (ttsReadComments_) ttsReadComments_->setChecked(loadDbSetting("tts_comments",      "1") == "1");
    if (ttsNameComments_) ttsNameComments_->setChecked(loadDbSetting("tts_name_comments", "1") == "1");
    if (ttsReadGifts_)    ttsReadGifts_->setChecked(loadDbSetting("tts_gifts",            "1") == "1");
    if (ttsNameGifts_)    ttsNameGifts_->setChecked(loadDbSetting("tts_name_gifts",       "1") == "1");
    if (ttsReadFollows_)  ttsReadFollows_->setChecked(loadDbSetting("tts_follows",        "0") == "1");
    if (ttsNameFollows_)  ttsNameFollows_->setChecked(loadDbSetting("tts_name_follows",   "1") == "1");
    if (ttsReadLikes_)    ttsReadLikes_->setChecked(loadDbSetting("tts_likes",            "0") == "1");
    if (ttsNameLikes_)    ttsNameLikes_->setChecked(loadDbSetting("tts_name_likes",       "1") == "1");
    if (ttsReadEmojis_) {
        bool emojiOn = loadDbSetting("tts_emojis", "0") == "1";
        ttsReadEmojis_->setChecked(emojiOn);
        tts_->setReadEmojis(emojiOn);
    }
    if (ttsMaxQueueSpin_) {
        int maxQ = loadDbSetting("tts_max_queue", "3").toInt();
        ttsMaxQueueSpin_->setValue(maxQ);
        tts_->setMaxQueue(maxQ);
    }
    if (ttsRateSlider_) {
        int rate = loadDbSetting("tts_rate", "25").toInt();
        ttsRateSlider_->setValue(rate);
        tts_->setRate(rate);
        ttsRateLabel_->setText((rate >= 0 ? "+" : "") + QString::number(rate) + "%");
    }
}

void MainWindow::saveSettings() {
    // Los efectos de sonido se guardan en SQLite directamente desde SoundDeckWidget
    settings_.setValue("masterVolume", masterVolSlider_->value());
    settings_.setValue("duckPercent",  duckSlider_->value());
    settings_.setValue("tiktokUser",   serverUrlEdit_->text());
}

// ══════════════════════════════════════════════════════════════════════════════
//  SIDEBAR
// ══════════════════════════════════════════════════════════════════════════════
QWidget* MainWindow::createSidebar() {
    auto* sidebar = new QWidget();
    sidebar->setFixedWidth(210);
    sidebar->setStyleSheet(QString("QWidget { background-color: %1; }").arg(C_WHITE));

    auto* layout = new QVBoxLayout(sidebar);
    layout->setContentsMargins(20, 26, 20, 20);
    layout->setSpacing(0);

    // Logo
    auto* logoRow = new QWidget();
    auto* logoLayout = new QHBoxLayout(logoRow);
    logoLayout->setContentsMargins(0, 0, 0, 28);
    logoLayout->setSpacing(9);

    auto* logoBox = new QLabel();
    logoBox->setFixedSize(36, 36);
    logoBox->setStyleSheet("background: transparent; border: none;");
    logoBox->setPixmap(QPixmap(":/icons/icono-48.png")
                           .scaled(36, 36, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    logoBox->setAlignment(Qt::AlignCenter);

    auto* appTitle = new QLabel("OmniStream\nDeck");
    appTitle->setStyleSheet(QString("font-size: 13px; font-weight: 700; color: %1;").arg(C_TEXT));
    logoLayout->addWidget(logoBox);
    logoLayout->addWidget(appTitle, 1);
    layout->addWidget(logoRow);

    auto* subtitle = new QLabel("Tu espacio de sonido");
    subtitle->setStyleSheet(QString("color: %1; font-size: 11.5px; margin-bottom: 4px;").arg(C_MUTED));
    layout->addWidget(subtitle);
    layout->addSpacing(14);

    // Función helper para botón de navegación
    auto makeNav = [&](const QString& iconChar, const QString& text, bool active) -> QPushButton* {
        auto* btn = new QPushButton();
        btn->setFixedHeight(38);
        btn->setIcon(Icon::make(iconChar, active ? QColor(C_ACCENT) : QColor(C_TEXT), 18));
        btn->setIconSize(QSize(18, 18));
        btn->setText("  " + text);
        btn->setCheckable(true);
        btn->setChecked(active);
        auto styleActive = QString(R"(
            QPushButton { background: %1; border-radius: 9px; color: %2;
                font-weight: 600; font-size: 13px; text-align: left; padding: 0 10px; border: none; }
        )").arg(C_ACTIVE).arg(C_ACCENT);
        auto styleInactive = QString(R"(
            QPushButton { background: transparent; border-radius: 9px; color: %1;
                font-size: 13px; text-align: left; padding: 0 10px; border: none; }
            QPushButton:hover { background: #F4F7FC; }
        )").arg(C_TEXT);
        btn->setProperty("activeStyle",   styleActive);
        btn->setProperty("inactiveStyle", styleInactive);
        btn->setStyleSheet(active ? styleActive : styleInactive);
        return btn;
    };

    navBiblio_ = makeNav(Icon::library(), "Biblioteca",    true);
    navTikTok_ = makeNav(Icon::live(),    "TikTok Live",   false);
    navConfig_ = makeNav(Icon::settings(),"Configuración", false);

    connect(navBiblio_, &QPushButton::clicked, this, [this]{ onNavClicked(0); });
    connect(navTikTok_, &QPushButton::clicked, this, [this]{ onNavClicked(1); });
    connect(navConfig_, &QPushButton::clicked, this, [this]{ onNavClicked(2); });

    layout->addWidget(navBiblio_);
    layout->addSpacing(5);
    layout->addWidget(navTikTok_);
    layout->addSpacing(5);
    layout->addWidget(navConfig_);
    layout->addStretch();

    // Mini badge estado TikTok
    auto* tiktokStatusCard = new QFrame();
    tiktokStatusCard->setStyleSheet("QFrame { background: #F4F7FC; border-radius: 10px; border: none; }");
    auto* tsLayout = new QHBoxLayout(tiktokStatusCard);
    tsLayout->setContentsMargins(12, 10, 12, 10);
    tsLayout->setSpacing(7);
    miniTikTokDot_ = new QLabel("●");
    miniTikTokDot_->setStyleSheet("color: #D1D5DB; font-size: 10px; background: transparent; border: none;");
    miniTikTokStatus_ = new QLabel("TikTok: sin conectar");
    miniTikTokStatus_->setStyleSheet(QString("font-size: 11px; color: %1; background: transparent; border: none;").arg(C_MUTED));
    miniTikTokStatus_->setWordWrap(true);
    tsLayout->addWidget(miniTikTokDot_);
    tsLayout->addWidget(miniTikTokStatus_, 1);
    layout->addWidget(tiktokStatusCard);
    layout->addSpacing(10);

    auto* verLabel = new QLabel("OSD-AUDIO  v1.0.0");
    verLabel->setStyleSheet("color: #94A0B4; font-size: 10.5px;");
    layout->addWidget(verLabel);

    return sidebar;
}

// ══════════════════════════════════════════════════════════════════════════════
//  PÁGINA 0 — BIBLIOTECA
// ══════════════════════════════════════════════════════════════════════════════
QWidget* MainWindow::createLibraryPage() {
    auto* page   = new QWidget();
    auto* hbox   = new QHBoxLayout(page);
    hbox->setContentsMargins(0,0,0,0);
    hbox->setSpacing(0);

    // ── Panel central: biblioteca de sonidos ──────────────────────────────
    auto* libPanel = new QWidget();
    auto* libLayout = new QVBoxLayout(libPanel);
    libLayout->setContentsMargins(28, 28, 24, 0);
    libLayout->setSpacing(0);

    // Header
    auto* hRow = new QWidget();
    auto* hRowL = new QHBoxLayout(hRow);
    hRowL->setContentsMargins(0,0,0,0);
    auto* titleBlock = new QWidget();
    auto* titleL = new QVBoxLayout(titleBlock);
    titleL->setContentsMargins(0,0,0,0); titleL->setSpacing(3);
    auto* mainTitle = new QLabel("Tu biblioteca de sonidos");
    mainTitle->setStyleSheet(QString("font-size: 23px; font-weight: 700; color: %1;").arg(C_TEXT));
    auto* mainSub = new QLabel("Clic en una tarjeta para reproducir · Click derecho para asignar audio");
    mainSub->setStyleSheet(QString("font-size: 12px; color: %1;").arg(C_MUTED));
    titleL->addWidget(mainTitle); titleL->addWidget(mainSub);
    hRowL->addWidget(titleBlock, 1);
    libLayout->addWidget(hRow);
    libLayout->addSpacing(20);

    // Toolbar: búsqueda + filtro + toggle
    auto* toolbar = new QWidget();
    auto* toolbarL = new QHBoxLayout(toolbar);
    toolbarL->setContentsMargins(0,0,0,0); toolbarL->setSpacing(10);

    searchBox_ = new QLineEdit();
    searchBox_->setPlaceholderText("Buscar sonidos...");
    searchBox_->addAction(Icon::make(Icon::search(), QColor(C_MUTED), 14), QLineEdit::LeadingPosition);
    searchBox_->setFixedHeight(36);
    searchBox_->setStyleSheet(QString(R"(
        QLineEdit { background: white; border: 1px solid %1; border-radius: 9px;
            padding: 0 10px; font-size: 13px; color: %2; }
        QLineEdit:focus { border-color: %3; }
    )").arg(C_BORDER).arg(C_TEXT).arg(C_ACCENT));

    auto* catFilter = new QComboBox();
    catFilter->addItems({"Todas las categorías","Efectos","Música","Reacciones","Voz","Alertas"});
    catFilter->setFixedSize(170, 36);
    catFilter->setStyleSheet(QString(R"(
        QComboBox { background: white; border: 1px solid %1; border-radius: 9px;
            padding: 0 10px; font-size: 13px; color: %2; }
        QComboBox::drop-down { border: none; width: 24px; }
        QComboBox QAbstractItemView { background: white; border: 1px solid %1;
            selection-background-color: %3; }
    )").arg(C_BORDER).arg(C_TEXT).arg(C_ACTIVE));

    auto* toggleFrame = new QFrame();
    toggleFrame->setStyleSheet(QString("QFrame { background: white; border: 1px solid %1; border-radius: 9px; }").arg(C_BORDER));
    auto* toggleL = new QHBoxLayout(toggleFrame);
    toggleL->setContentsMargins(4,4,4,4); toggleL->setSpacing(2);
    toggleFrame->setFixedHeight(38);
    auto mkTog = [&](const QString& ic, const QString& txt, bool active) {
        auto* b = new QPushButton();
        b->setIcon(Icon::make(ic, active ? QColor("white") : QColor(C_MUTED), 14));
        b->setIconSize(QSize(14,14)); b->setText(" "+txt); b->setFixedHeight(28);
        b->setStyleSheet(active ? QString("QPushButton { background: %1; color: white; border-radius: 7px; border: none; font-size: 12px; padding: 0 8px; }").arg(C_ACCENT)
            : "QPushButton { background: transparent; color: #7B88A0; border: none; font-size: 12px; padding: 0 8px; } QPushButton:hover { color: #3064E8; }");
        return b;
    };
    togGrid_ = mkTog(Icon::grid(), "Tarjetas", true);
    togList_ = mkTog(Icon::list(),  "Lista",    false);

    static const QString SS_ACTIVE =
        "QPushButton { background: #3064E8; color: white; border-radius: 7px;"
        " border: none; font-size: 12px; padding: 0 8px; }";
    static const QString SS_INACTIVE =
        "QPushButton { background: transparent; color: #7B88A0; border: none;"
        " font-size: 12px; padding: 0 8px; }"
        "QPushButton:hover { color: #3064E8; }";

    connect(togGrid_, &QPushButton::clicked, this, [this](){
        togGrid_->setStyleSheet(SS_ACTIVE);
        togGrid_->setIcon(Icon::make(Icon::grid(), QColor("white"), 14));
        togList_->setStyleSheet(SS_INACTIVE);
        togList_->setIcon(Icon::make(Icon::list(), QColor("#7B88A0"), 14));
        soundDeck_->setListView(false);
    });
    connect(togList_, &QPushButton::clicked, this, [this](){
        togList_->setStyleSheet(SS_ACTIVE);
        togList_->setIcon(Icon::make(Icon::list(), QColor("white"), 14));
        togGrid_->setStyleSheet(SS_INACTIVE);
        togGrid_->setIcon(Icon::make(Icon::grid(), QColor("#7B88A0"), 14));
        soundDeck_->setListView(true);
    });

    toggleL->addWidget(togGrid_);
    toggleL->addWidget(togList_);

    toolbarL->addWidget(searchBox_, 1);
    toolbarL->addWidget(catFilter);
    toolbarL->addWidget(toggleFrame);
    libLayout->addWidget(toolbar);
    libLayout->addSpacing(16);

    soundDeck_ = new SoundDeckWidget(this);
    libLayout->addWidget(soundDeck_, 1);

    // Barra inferior de estado
    auto* statusBar = new QFrame();
    statusBar->setStyleSheet(QString("QFrame { background: %1; border-radius: 12px; border: none; }").arg(C_STATUS));
    auto* statusL = new QHBoxLayout(statusBar);
    statusL->setContentsMargins(16,12,16,12);

    auto* stBlock = new QWidget(); stBlock->setStyleSheet("background: transparent;");
    auto* stBlockL = new QVBoxLayout(stBlock); stBlockL->setContentsMargins(0,0,0,0); stBlockL->setSpacing(2);
    nowPlayingLabel_ = new QLabel("Listo cuando tú quieras");
    nowPlayingLabel_->setStyleSheet(QString("font-weight: 600; font-size: 13px; color: %1; background: transparent;").arg(C_TEXT));
    statusMsgLabel_ = new QLabel("Asigna archivos a las tarjetas (click derecho) para comenzar.");
    statusMsgLabel_->setStyleSheet(QString("font-size: 11px; color: %1; background: transparent;").arg(C_MUTED));
    statusMsgLabel_->setWordWrap(true);
    stBlockL->addWidget(nowPlayingLabel_); stBlockL->addWidget(statusMsgLabel_);

    // Volumen master
    auto* volRow = new QWidget(); volRow->setStyleSheet("background: transparent;");
    auto* volL = new QHBoxLayout(volRow); volL->setContentsMargins(0,0,0,0); volL->setSpacing(7);
    volL->addWidget(Icon::label(volRow, Icon::volume(), QColor(C_MUTED), 13));
    masterVolSlider_ = new QSlider(Qt::Horizontal);
    masterVolSlider_->setRange(0,100); masterVolSlider_->setValue(80); masterVolSlider_->setFixedWidth(100);
    masterVolSlider_->setStyleSheet(R"(QSlider::groove:horizontal{height:4px;background:#D0D8E8;border-radius:2px}QSlider::sub-page:horizontal{background:#3064E8;border-radius:2px}QSlider::handle:horizontal{background:#3064E8;width:14px;height:14px;margin:-5px 0;border-radius:7px})");
    masterVolLabel_ = new QLabel("80%");
    masterVolLabel_->setFixedWidth(36);
    masterVolLabel_->setStyleSheet(QString("font-size: 12px; color: %1; background: transparent;").arg(C_TEXT));
    volL->addWidget(masterVolSlider_); volL->addWidget(masterVolLabel_);

    stopBtn_ = new QPushButton();
    stopBtn_->setIcon(Icon::make(Icon::stop(), QColor("#9CA3AF"), 14));
    stopBtn_->setIconSize(QSize(14,14)); stopBtn_->setText(" Detener");
    stopBtn_->setFixedHeight(34); stopBtn_->setEnabled(false);
    stopBtn_->setStyleSheet(R"(QPushButton{background:white;border:1px solid #E3E9F3;border-radius:8px;padding:0 14px;font-size:12px;color:#9CA3AF}QPushButton:enabled{color:#E03030;border-color:#FECDD3}QPushButton:enabled:hover{background:#FFF1F2})");

    libraryHotkeysBtn_ = new QPushButton();
    libraryHotkeysBtn_->setCheckable(true);
    libraryHotkeysBtn_->setFixedHeight(34);
    libraryHotkeysBtn_->setCursor(Qt::PointingHandCursor);
    libraryHotkeysBtn_->setStyleSheet(R"(
        QPushButton { background: #B91C1C; color: white; border-radius: 8px;
                      padding: 0 12px; font-size: 12px; font-weight: 600; }
        QPushButton:hover { background: #991B1B; }
        QPushButton:checked { background: #15803D; }
        QPushButton:checked:hover { background: #166534; }
    )");

    statusL->addWidget(stBlock, 1);
    statusL->addWidget(libraryHotkeysBtn_);
    statusL->addWidget(volRow);
    statusL->addSpacing(8);
    statusL->addWidget(stopBtn_);
    libLayout->addSpacing(12); libLayout->addWidget(statusBar); libLayout->addSpacing(16);

    // ── Panel derecho: Mezclador ──────────────────────────────────────────
    auto* div = new QFrame(); div->setFrameShape(QFrame::VLine); div->setFixedWidth(1);
    div->setStyleSheet(QString("QFrame { color: %1; }").arg(C_BORDER));

    hbox->addWidget(libPanel, 1);
    hbox->addWidget(div);
    hbox->addWidget(createMixerPanel());
    return page;
}

QWidget* MainWindow::createMixerPanel() {
    auto* panel = new QWidget(); panel->setFixedWidth(280);
    panel->setStyleSheet(QString("QWidget { background: %1; }").arg(C_PANEL));
    auto* scroll = new QScrollArea(); scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet("QScrollArea { background: transparent; border: none; }");
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto* content = new QWidget(); content->setStyleSheet(QString("background: %1;").arg(C_PANEL));
    auto* layout  = new QVBoxLayout(content);
    layout->setContentsMargins(20,22,20,20); layout->setSpacing(0);

    auto* row1 = new QWidget(); row1->setStyleSheet("background:transparent;");
    auto* row1L = new QHBoxLayout(row1); row1L->setContentsMargins(0,0,0,0);
    row1L->addWidget(Icon::label(row1, Icon::duck(), QColor(C_ACCENT), 15));
    auto* mixTitle = new QLabel("Mezclador");
    mixTitle->setStyleSheet(QString("font-size: 16px; font-weight: 700; color: %1; background: transparent;").arg(C_TEXT));
    row1L->addWidget(mixTitle,1);
    layout->addWidget(row1);
    auto* mixSub = new QLabel("Duck automático al reproducir efectos.");
    mixSub->setStyleSheet(QString("font-size: 11px; color: %1; margin-top:3px;").arg(C_MUTED));
    layout->addWidget(mixSub); layout->addSpacing(18);

    // Duck card
    auto* duckCard = new QFrame();
    duckCard->setStyleSheet("QFrame { background: white; border: 1px solid #E0E7F2; border-radius: 12px; }");
    auto* duckL = new QVBoxLayout(duckCard); duckL->setContentsMargins(16,16,16,16); duckL->setSpacing(0);
    auto* duckTitleL = new QLabel("Volumen durante el efecto");
    duckTitleL->setStyleSheet("font-weight: 600; font-size: 13px; background: transparent; border: none;");
    auto* duckRow = new QWidget(); duckRow->setStyleSheet("background:transparent;");
    auto* duckRowL = new QHBoxLayout(duckRow); duckRowL->setContentsMargins(0,12,0,8);
    auto* duckConserv = new QLabel("Conservar global");
    duckConserv->setStyleSheet(QString("color: %1; font-size: 12px; background: transparent; border: none;").arg(C_MUTED));
    duckLabel_ = new QLabel("20 %");
    duckLabel_->setStyleSheet(QString("font-size: 22px; font-weight: 700; color: %1; background: transparent; border: none;").arg(C_ACCENT));
    duckRowL->addWidget(duckConserv,1); duckRowL->addWidget(duckLabel_);
    duckSlider_ = new QSlider(Qt::Horizontal); duckSlider_->setRange(0,100); duckSlider_->setValue(20);
    duckSlider_->setStyleSheet(R"(QSlider::groove:horizontal{height:4px;background:#E0E7F2;border-radius:2px}QSlider::sub-page:horizontal{background:#3064E8;border-radius:2px}QSlider::handle:horizontal{background:#3064E8;width:16px;height:16px;margin:-6px 0;border-radius:8px})");

    auto* duckManualBtn = new QPushButton();
    duckManualBtn->setIcon(Icon::make(Icon::duck(), QColor(C_ACCENT), 14));
    duckManualBtn->setIconSize(QSize(14,14)); duckManualBtn->setText(" Duck manual ahora");
    duckManualBtn->setStyleSheet(QString("QPushButton { background: %1; border: none; border-radius: 8px; color: %2; font-size: 12px; padding: 7px 12px; margin-top: 10px; font-weight: 600; } QPushButton:hover { background: #D4E2FC; }").arg(C_ACTIVE).arg(C_ACCENT));
    connect(duckManualBtn, &QPushButton::clicked, this, &MainWindow::onDuckManual);
    auto* duckNote = new QLabel("El ducking se activa automáticamente al reproducir un efecto.");
    duckNote->setWordWrap(true);
    duckNote->setStyleSheet(QString("font-size: 11px; color: %1; margin-top: 10px; background: transparent; border: none;").arg(C_MUTED));
    duckL->addWidget(duckTitleL); duckL->addWidget(duckRow); duckL->addWidget(duckSlider_);
    duckL->addWidget(duckManualBtn); duckL->addWidget(duckNote);
    layout->addWidget(duckCard);

    auto* appTitle = new QLabel("Volumen por aplicación");
    appTitle->setStyleSheet(QString("font-size: 13px; font-weight: 700; color: %1;").arg(C_TEXT));
    auto* appHint = new QLabel("Ajusta cuánto volumen conserva cada aplicación durante un efecto.");
    appHint->setWordWrap(true);
    appHint->setStyleSheet(QString("font-size: 11px; color: %1;").arg(C_MUTED));
    layout->addSpacing(16);
    layout->addWidget(appTitle);
    layout->addSpacing(4);
    layout->addWidget(appHint);
    layout->addSpacing(8);
    duckAppsLayout_ = new QVBoxLayout();
    duckAppsLayout_->setSpacing(6);
    layout->addLayout(duckAppsLayout_);
    layout->addStretch();

    auto* audioAppsTimer = new QTimer(panel);
    connect(audioAppsTimer, &QTimer::timeout, this, &MainWindow::refreshDuckingApplications);
    audioAppsTimer->start(500);
    refreshDuckingApplications();

    scroll->setWidget(content);
    auto* panelL = new QVBoxLayout(panel); panelL->setContentsMargins(0,0,0,0); panelL->addWidget(scroll);
    return panel;
}

void MainWindow::refreshDuckingApplications() {
    const auto apps = applicationDucking_->refresh();
    if (apps == duckApps_ && duckAppsLayout_->count() > 0) return;
    duckApps_ = apps;
    while (auto* item = duckAppsLayout_->takeAt(0)) {
        item->widget()->hide();
        item->widget()->deleteLater();
        delete item;
    }

    if (apps.isEmpty()) {
        auto* empty = new QLabel("Aún no hay otras aplicaciones reproduciendo audio.");
        empty->setWordWrap(true);
        empty->setStyleSheet(QString("font-size: 11px; color: %1;").arg(C_MUTED));
        duckAppsLayout_->addWidget(empty);
        return;
    }

    for (auto app = apps.cbegin(); app != apps.cend(); ++app) {
        auto* row = new QFrame();
        row->setObjectName("applicationVolumeCard");
        row->setStyleSheet(R"(
            QFrame#applicationVolumeCard { background: #FFFFFF; border: 1px solid #E0E7F2; border-radius: 10px; }
            QFrame#applicationVolumeCard QLabel { background: transparent; }
        )");
        auto* rowLayout = new QVBoxLayout(row);
        rowLayout->setContentsMargins(10, 9, 10, 9);
        rowLayout->setSpacing(7);
        auto* label = new QLabel(app.value());
        label->setTextFormat(Qt::PlainText);
        label->setWordWrap(true);
        label->setStyleSheet("font-size: 12px; font-weight: 700; color: #1A2340;");
        rowLayout->addWidget(label);

        auto* selector = new QWidget();
        selector->setObjectName("applicationVolumeMode");
        selector->setStyleSheet(R"(
            QWidget#applicationVolumeMode { background: #EEF3FB; border-radius: 8px; }
            QWidget#applicationVolumeMode QPushButton {
                background: transparent; border: 1px solid transparent; border-radius: 6px;
                color: #475875; font-size: 11px; font-weight: 600; padding: 4px 5px;
            }
            QWidget#applicationVolumeMode QPushButton:hover { background: #E0EAFC; }
            QWidget#applicationVolumeMode QPushButton:checked { background: #3064E8; color: white; }
            QWidget#applicationVolumeMode QPushButton:focus { border-color: #1A2340; }
        )");
        auto* modeLayout = new QHBoxLayout(selector);
        modeLayout->setContentsMargins(3, 3, 3, 3);
        modeLayout->setSpacing(3);
        auto* globalButton = new QPushButton("Global");
        auto* customButton = new QPushButton("Personalizado");
        globalButton->setCheckable(true);
        customButton->setCheckable(true);
        auto* modes = new QButtonGroup(row);
        modes->addButton(globalButton);
        modes->addButton(customButton);
        modeLayout->addWidget(globalButton, 1);
        modeLayout->addWidget(customButton, 1);
        rowLayout->addWidget(selector);

        auto* valueRow = new QHBoxLayout();
        auto* valueLabel = new QLabel("Conservar");
        valueLabel->setStyleSheet("font-size: 11px; color: #52617C;");
        valueRow->addWidget(valueLabel, 1);
        auto* inherited = new QLabel();
        inherited->setStyleSheet("font-size: 12px; font-weight: 700; color: #3064E8;");
        const auto showGlobal = [inherited](int value) { inherited->setText(QString("%1 %").arg(value)); };
        showGlobal(duckSlider_->value());
        connect(duckSlider_, &QSlider::valueChanged, inherited, showGlobal);
        valueRow->addWidget(inherited);

        const QString key = app.key();
        const int savedPercent = applicationDucking_->applicationPercent(key);
        const bool useCustom = savedPercent >= 0;
        auto* percent = new QSpinBox();
        percent->setRange(0, 100);
        percent->setSuffix(" %");
        percent->setKeyboardTracking(false);
        percent->setButtonSymbols(QAbstractSpinBox::NoButtons);
        percent->setAlignment(Qt::AlignRight);
        percent->setAccessibleName("Volumen durante el efecto de " + app.value());
        percent->setToolTip("Escribe un porcentaje entre 0 y 100.");
        percent->setValue(useCustom ? savedPercent : duckSlider_->value());
        percent->setFixedWidth(58);
        percent->setStyleSheet(R"(
            QSpinBox { background: #FFFFFF; color: #1A2340; border: 1px solid #B9CCEF; border-radius: 6px; padding: 3px 5px; }
            QSpinBox:focus { border-color: #3064E8; }
        )");
        valueRow->addWidget(percent);
        rowLayout->addLayout(valueRow);

        globalButton->setChecked(!useCustom);
        customButton->setChecked(useCustom);
        inherited->setVisible(!useCustom);
        percent->setVisible(useCustom);
        connect(customButton, &QPushButton::toggled, row, [this, key, inherited, percent](bool custom) {
            inherited->setVisible(!custom);
            percent->setVisible(custom);
            applicationDucking_->setApplicationPercent(key, custom ? percent->value() : -1);
        });
        connect(customButton, &QPushButton::clicked, percent, [percent] {
            percent->setFocus();
            percent->selectAll();
        });
        connect(percent, &QSpinBox::valueChanged, row, [this, key, customButton](int value) {
            if (customButton->isChecked()) applicationDucking_->setApplicationPercent(key, value);
        });
        duckAppsLayout_->addWidget(row);
    }
}

// ══════════════════════════════════════════════════════════════════════════════
//  PÁGINA 1 — TIKTOK LIVE
// ══════════════════════════════════════════════════════════════════════════════
QWidget* MainWindow::createTikTokPage() {
    auto* page  = new QWidget();
    page->setStyleSheet(QString("QWidget { background: %1; }").arg(C_WHITE));
    auto* hbox  = new QHBoxLayout(page);
    hbox->setContentsMargins(0,0,0,0); hbox->setSpacing(0);

    // ── Panel izquierdo: conexión + feed ─────────────────────────────────
    auto* leftPanel = new QWidget();
    auto* leftL = new QVBoxLayout(leftPanel);
    leftL->setContentsMargins(32,28,24,24); leftL->setSpacing(0);

    // Header
    auto* ttHeader = new QWidget(); ttHeader->setStyleSheet("background:transparent;");
    auto* ttHeaderL = new QHBoxLayout(ttHeader); ttHeaderL->setContentsMargins(0,0,0,0); ttHeaderL->setSpacing(10);
    ttHeaderL->addWidget(Icon::label(ttHeader, Icon::live(), QColor(C_ACCENT), 22));
    auto* ttTitle = new QLabel("TikTok Live");
    ttTitle->setStyleSheet(QString("font-size: 24px; font-weight: 700; color: %1;").arg(C_TEXT));
    ttHeaderL->addWidget(ttTitle,1);
    leftL->addWidget(ttHeader);
    auto* ttSub = new QLabel("Eventos en tiempo real. Escribe tu usuario y conecta — sin servidores.");
    ttSub->setStyleSheet(QString("font-size: 12px; color: %1; margin-top:4px; margin-bottom:18px;").arg(C_MUTED));
    leftL->addWidget(ttSub);

    // Card de conexión
    auto* connCard = new QFrame();
    connCard->setStyleSheet("QFrame { background: white; border: 1px solid #E0E7F2; border-radius: 14px; }");
    auto* connCardL = new QVBoxLayout(connCard); connCardL->setContentsMargins(20,18,20,18); connCardL->setSpacing(10);
    auto* connCardTitle = new QLabel("Conectar a tu live");
    connCardTitle->setStyleSheet("font-weight: 700; font-size: 14px; background: transparent; border: none;");
    serverUrlEdit_ = new QLineEdit();
    serverUrlEdit_->setPlaceholderText("@tu_usuario_tiktok");
    serverUrlEdit_->setFixedHeight(38);
    serverUrlEdit_->setStyleSheet(QString(R"(
        QLineEdit { background: #F7F9FE; border: 1px solid %1; border-radius: 10px;
            padding: 0 14px; font-size: 14px; color: %2; }
        QLineEdit:focus { border-color: %3; background: white; }
    )").arg(C_BORDER).arg(C_TEXT).arg(C_ACCENT));

    // Fila estado + botón
    auto* connRow = new QWidget(); connRow->setStyleSheet("background:transparent;");
    auto* connRowL = new QHBoxLayout(connRow); connRowL->setContentsMargins(0,0,0,0); connRowL->setSpacing(10);
    connDot_ = new QLabel("●");
    connDot_->setStyleSheet("color: #E03030; font-size: 12px; background: transparent; border: none;");
    connStatus_ = new QLabel("Sin conectar");
    connStatus_->setStyleSheet(QString("font-size: 13px; color: %1; background: transparent; border: none;").arg(C_MUTED));
    connBtn_ = new QPushButton();
    connBtn_->setIcon(Icon::make(Icon::connect(), QColor("white"), 14));
    connBtn_->setIconSize(QSize(14,14)); connBtn_->setText(" Conectar");
    connBtn_->setFixedHeight(38);
    connBtn_->setStyleSheet(QString(R"(
        QPushButton { background: %1; color: white; border: none;
            border-radius: 10px; padding: 0 20px; font-size: 13px; font-weight: 700; }
        QPushButton:hover { background: #2550C8; }
    )").arg(C_ACCENT));
    connRowL->addWidget(connDot_); connRowL->addWidget(connStatus_,1); connRowL->addWidget(connBtn_);

    connCardL->addWidget(connCardTitle);
    connCardL->addWidget(serverUrlEdit_);
    connCardL->addWidget(connRow);
    leftL->addWidget(connCard); leftL->addSpacing(22);

    // Feed de eventos
    auto* feedTitle = new QLabel("Feed de eventos en vivo");
    feedTitle->setStyleSheet(QString("font-size: 14px; font-weight: 700; color: %1; margin-bottom: 8px;").arg(C_TEXT));
    leftL->addWidget(feedTitle);
    tiktokFeed_ = new QListWidget();
    tiktokFeed_->setStyleSheet(R"(
        QListWidget { background: white; border: 1px solid #E0E7F2; border-radius: 12px; font-size: 13px; }
        QListWidget::item { padding: 8px 14px; border-bottom: 1px solid #F0F4FA; }
        QListWidget::item:hover { background: #F3F7FF; }
    )");
    auto* placeholder = new QListWidgetItem(tiktokFeed_);
    placeholder->setText("Esperando conexión...");
    placeholder->setForeground(QColor(C_MUTED));
    leftL->addWidget(tiktokFeed_, 1); leftL->addSpacing(16);

    // ── Divisor ──────────────────────────────────────────────────────────
    auto* div = new QFrame(); div->setFrameShape(QFrame::VLine); div->setFixedWidth(1);
    div->setStyleSheet(QString("QFrame { color: %1; }").arg(C_BORDER));

    // ── Panel derecho: estadísticas ───────────────────────────────────────
    auto* rightPanel = new QWidget(); rightPanel->setFixedWidth(260);
    rightPanel->setStyleSheet(QString("QWidget { background: %1; }").arg(C_PANEL));
    auto* rightScroll = new QScrollArea(); rightScroll->setWidgetResizable(true);
    rightScroll->setFrameShape(QFrame::NoFrame);
    rightScroll->setStyleSheet("QScrollArea { background: transparent; border: none; }");
    auto* rightContent = new QWidget(); rightContent->setStyleSheet(QString("background: %1;").arg(C_PANEL));
    auto* rightL = new QVBoxLayout(rightContent); rightL->setContentsMargins(20,22,20,20); rightL->setSpacing(12);

    auto* statsTitle = new QLabel("Estadísticas");
    statsTitle->setStyleSheet(QString("font-size: 15px; font-weight: 700; color: %1;").arg(C_TEXT));
    rightL->addWidget(statsTitle);

    // Tarjetas de stats: comentarios, regalos, seguidores, likes
    struct StatDef { QString icon; QString label; QLabel** ptr; QColor color; };
    StatDef stats[] = {
        { Icon::comment(), "Comentarios", &countComments_, QColor("#3064E8") },
        { Icon::gift(),    "Regalos",     &countGifts_,    QColor("#F59E0B") },
        { Icon::accept(),  "Seguidores",  &countFollows_,  QColor("#22C55E") },
        { Icon::heart(),   "Likes",       &countLikes_,    QColor("#E879F9") },
    };
    for (auto& s : stats) {
        auto* card = new QFrame();
        card->setStyleSheet("QFrame { background: white; border: 1px solid #E0E7F2; border-radius: 12px; }");
        auto* cardL = new QHBoxLayout(card); cardL->setContentsMargins(14,14,14,14); cardL->setSpacing(10);
        auto* iconLbl = Icon::label(card, s.icon, s.color, 18);
        auto* textBlock = new QWidget(); textBlock->setStyleSheet("background:transparent;");
        auto* textBlockL = new QVBoxLayout(textBlock); textBlockL->setContentsMargins(0,0,0,0); textBlockL->setSpacing(1);
        *s.ptr = new QLabel("0");
        (*s.ptr)->setStyleSheet(QString("font-size: 22px; font-weight: 700; color: %1; background: transparent; border: none;").arg(s.color.name()));
        auto* nameLbl = new QLabel(s.label);
        nameLbl->setStyleSheet(QString("font-size: 11px; color: %1; background: transparent; border: none;").arg(C_MUTED));
        textBlockL->addWidget(*s.ptr); textBlockL->addWidget(nameLbl);
        cardL->addWidget(iconLbl); cardL->addWidget(textBlock,1);
        rightL->addWidget(card);
    }

    // Panel TTS toggles
    rightL->addSpacing(8);
    auto* ttsCard = new QFrame();
    ttsCard->setStyleSheet("QFrame { background: white; border: 1px solid #E0E7F2; border-radius: 12px; }");
    auto* ttsCardL = new QVBoxLayout(ttsCard); ttsCardL->setContentsMargins(14,14,14,14); ttsCardL->setSpacing(6);

    auto* ttsTitleRow = new QWidget(); ttsTitleRow->setStyleSheet("background:transparent;");
    auto* ttsTitleL = new QHBoxLayout(ttsTitleRow); ttsTitleL->setContentsMargins(0,0,0,0); ttsTitleL->setSpacing(6);
    ttsTitleL->addWidget(Icon::label(ttsTitleRow, Icon::volume(), QColor(C_ACCENT), 14));
    auto* ttsTitle = new QLabel("Lectura por voz");
    ttsTitle->setStyleSheet(QString("font-weight: 700; font-size: 13px; color: %1; background:transparent; border:none;").arg(C_TEXT));
    ttsTitleL->addWidget(ttsTitle, 1);
    ttsCardL->addWidget(ttsTitleRow);

    // Toggle global: leer emojis
    ttsReadEmojis_ = new QCheckBox("Leer emojis");
    ttsReadEmojis_->setStyleSheet(QString(R"(
        QCheckBox { font-size: 11px; color: %1; background:transparent; spacing:6px; }
        QCheckBox::indicator { width:13px; height:13px; border-radius:3px;
            border:1.5px solid #D0D8E8; background:white; }
        QCheckBox::indicator:checked { background:#94A3B8; border-color:#94A3B8; }
    )").arg(C_MUTED));
    connect(ttsReadEmojis_, &QCheckBox::toggled, this, [this](bool v) {
        tts_->setReadEmojis(v);
        saveDbSetting("tts_emojis", v ? "1" : "0");
    });
    ttsCardL->addWidget(ttsReadEmojis_);

    // Separador antes de cabeceras
    auto* sep0 = new QFrame(); sep0->setFrameShape(QFrame::HLine);
    sep0->setStyleSheet("QFrame { color: #EEF2FA; }");
    ttsCardL->addWidget(sep0);

    // Cabeceras de columnas
    auto* hdrRow = new QWidget(); hdrRow->setStyleSheet("background:transparent;");
    auto* hdrL = new QHBoxLayout(hdrRow); hdrL->setContentsMargins(0,2,0,2); hdrL->setSpacing(0);
    auto* hdrLeer = new QLabel("Leer");
    auto* hdrNombre = new QLabel("Con nombre");
    for (auto* h : {hdrLeer, hdrNombre}) {
        h->setStyleSheet(QString("font-size: 10px; font-weight: 700; color: %1; background:transparent; border:none;").arg(C_MUTED));
        h->setAlignment(Qt::AlignCenter);
    }
    hdrL->addWidget(hdrLeer, 1);
    hdrL->addWidget(hdrNombre, 1);
    ttsCardL->addWidget(hdrRow);

    // Separador
    auto* sep = new QFrame(); sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet("QFrame { color: #EEF2FA; }");
    ttsCardL->addWidget(sep);

    // Estilos comunes — ambos usan azul al estar marcados para ser claramente visibles
    auto chkMain = QString(R"(
        QCheckBox { font-size: 12px; color: %1; background:transparent; spacing:6px; }
        QCheckBox::indicator { width:15px; height:15px; border-radius:4px;
            border:1.5px solid #D0D8E8; background:white; }
        QCheckBox::indicator:checked { background:%2; border-color:%2; }
    )").arg(C_TEXT).arg(C_ACCENT);
    auto chkSub = QString(R"(
        QCheckBox { font-size: 11px; color: %1; background:transparent; spacing:6px; }
        QCheckBox::indicator { width:14px; height:14px; border-radius:3px;
            border:1.5px solid #D0D8E8; background:white; }
        QCheckBox::indicator:checked { background:%2; border-color:%2; }
    )").arg(C_MUTED).arg(C_ACCENT);

    // Helper: crea una fila con dos checkboxes
    struct TtsRow { QCheckBox* read; QCheckBox* name; };
    auto makeTtsRow = [&](const QString& label) -> TtsRow {
        auto* row = new QWidget(); row->setStyleSheet("background:transparent;");
        auto* rowL = new QHBoxLayout(row); rowL->setContentsMargins(0,1,0,1); rowL->setSpacing(0);
        auto* chkRead = new QCheckBox(label);
        auto* chkName = new QCheckBox("con nombre");
        chkRead->setStyleSheet(chkMain);
        chkName->setStyleSheet(chkSub);
        rowL->addWidget(chkRead, 1, Qt::AlignLeft);
        rowL->addWidget(chkName, 1, Qt::AlignLeft);
        ttsCardL->addWidget(row);
        return {chkRead, chkName};
    };

    auto rComm = makeTtsRow("Comentarios");
    auto rGift = makeTtsRow("Regalos");
    auto rFoll = makeTtsRow("Seguidores");
    auto rLike = makeTtsRow("Likes");

    ttsReadComments_ = rComm.read; ttsNameComments_ = rComm.name;
    ttsReadGifts_    = rGift.read; ttsNameGifts_    = rGift.name;
    ttsReadFollows_  = rFoll.read; ttsNameFollows_  = rFoll.name;
    ttsReadLikes_    = rLike.read; ttsNameLikes_    = rLike.name;

    connect(ttsReadComments_, &QCheckBox::toggled, this, [this](bool v){ saveDbSetting("tts_comments",      v?"1":"0"); });
    connect(ttsNameComments_, &QCheckBox::toggled, this, [this](bool v){ saveDbSetting("tts_name_comments", v?"1":"0"); });
    connect(ttsReadGifts_,    &QCheckBox::toggled, this, [this](bool v){ saveDbSetting("tts_gifts",         v?"1":"0"); });
    connect(ttsNameGifts_,    &QCheckBox::toggled, this, [this](bool v){ saveDbSetting("tts_name_gifts",    v?"1":"0"); });
    connect(ttsReadFollows_,  &QCheckBox::toggled, this, [this](bool v){ saveDbSetting("tts_follows",       v?"1":"0"); });
    connect(ttsNameFollows_,  &QCheckBox::toggled, this, [this](bool v){ saveDbSetting("tts_name_follows",  v?"1":"0"); });
    connect(ttsReadLikes_,    &QCheckBox::toggled, this, [this](bool v){ saveDbSetting("tts_likes",         v?"1":"0"); });
    connect(ttsNameLikes_,    &QCheckBox::toggled, this, [this](bool v){ saveDbSetting("tts_name_likes",    v?"1":"0"); });

    // Control: máximo de comentarios en cola
    auto* sep2 = new QFrame(); sep2->setFrameShape(QFrame::HLine);
    sep2->setStyleSheet("QFrame { color: #EEF2FA; }");
    ttsCardL->addWidget(sep2);

    auto* queueRow = new QWidget(); queueRow->setStyleSheet("background:transparent;");
    auto* queueRowL = new QHBoxLayout(queueRow); queueRowL->setContentsMargins(0,2,0,0); queueRowL->setSpacing(8);
    auto* queueLbl = new QLabel("Máx. en cola:");
    queueLbl->setStyleSheet(QString("font-size: 11px; color: %1; background:transparent; border:none;").arg(C_TEXT));
    ttsMaxQueueSpin_ = new QSpinBox();
    ttsMaxQueueSpin_->setRange(1, 10); ttsMaxQueueSpin_->setValue(3);
    ttsMaxQueueSpin_->setFixedWidth(56); ttsMaxQueueSpin_->setFixedHeight(26);
    ttsMaxQueueSpin_->setStyleSheet(QString(R"(
        QSpinBox { background:white; border:1px solid %1; border-radius:6px;
            padding:0 4px; font-size:12px; color:%2; }
        QSpinBox::up-button, QSpinBox::down-button { width:18px; border:none; }
    )").arg(C_BORDER).arg(C_TEXT));
    auto* queueNote = new QLabel("comentarios");
    queueNote->setStyleSheet(QString("font-size: 11px; color: %1; background:transparent; border:none;").arg(C_MUTED));
    connect(ttsMaxQueueSpin_, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int v){
        tts_->setMaxQueue(v);
        saveDbSetting("tts_max_queue", QString::number(v));
    });
    queueRowL->addWidget(queueLbl); queueRowL->addWidget(ttsMaxQueueSpin_); queueRowL->addWidget(queueNote,1);
    ttsCardL->addWidget(queueRow);

    // ── Velocidad de lectura ────────────────────────────────────────────────
    auto* rateRow = new QWidget(); rateRow->setStyleSheet("background:transparent;");
    auto* rateRowL = new QHBoxLayout(rateRow); rateRowL->setContentsMargins(0,4,0,0); rateRowL->setSpacing(8);
    auto* rateLbl = new QLabel("Velocidad:");
    rateLbl->setStyleSheet(QString("font-size: 11px; color: %1; background:transparent; border:none;").arg(C_TEXT));
    ttsRateSlider_ = new QSlider(Qt::Horizontal);
    ttsRateSlider_->setRange(-50, 100);
    ttsRateSlider_->setValue(25);
    ttsRateSlider_->setFixedHeight(20);
    ttsRateSlider_->setStyleSheet(QString(R"(
        QSlider::groove:horizontal { height:4px; background:%1; border-radius:2px; }
        QSlider::handle:horizontal { width:14px; height:14px; margin:-5px 0;
            background:%2; border-radius:7px; }
        QSlider::sub-page:horizontal { background:%2; border-radius:2px; }
    )").arg(C_BORDER).arg(C_ACCENT));
    ttsRateLabel_ = new QLabel("+25%");
    ttsRateLabel_->setFixedWidth(42);
    ttsRateLabel_->setStyleSheet(QString("font-size: 11px; color: %1; background:transparent; border:none;").arg(C_MUTED));
    connect(ttsRateSlider_, &QSlider::valueChanged, this, [this](int v){
        tts_->setRate(v);
        ttsRateLabel_->setText((v >= 0 ? "+" : "") + QString::number(v) + "%");
        saveDbSetting("tts_rate", QString::number(v));
    });
    rateRowL->addWidget(rateLbl);
    rateRowL->addWidget(ttsRateSlider_, 1);
    rateRowL->addWidget(ttsRateLabel_);
    ttsCardL->addWidget(rateRow);

    rightL->addWidget(ttsCard); rightL->addStretch();

    rightScroll->setWidget(rightContent);
    auto* rightPanelL = new QVBoxLayout(rightPanel); rightPanelL->setContentsMargins(0,0,0,0); rightPanelL->addWidget(rightScroll);

    hbox->addWidget(leftPanel,1); hbox->addWidget(div); hbox->addWidget(rightPanel);
    return page;
}

// ══════════════════════════════════════════════════════════════════════════════
//  PÁGINA 2 — CONFIGURACIÓN
// ══════════════════════════════════════════════════════════════════════════════
QWidget* MainWindow::createConfigPage() {
    auto* page = new QWidget();
    page->setStyleSheet(QString("QWidget { background: %1; }").arg(C_WHITE));
    auto* scroll = new QScrollArea(); scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setStyleSheet("QScrollArea { background: transparent; border: none; }");

    auto* content = new QWidget();
    auto* layout  = new QVBoxLayout(content); layout->setContentsMargins(32,28,32,28); layout->setSpacing(24);

    // Header
    auto* hdr = new QWidget(); hdr->setStyleSheet("background:transparent;");
    auto* hdrL = new QHBoxLayout(hdr); hdrL->setContentsMargins(0,0,0,0); hdrL->setSpacing(10);
    hdrL->addWidget(Icon::label(hdr, Icon::settings(), QColor(C_ACCENT), 22));
    auto* hdrTitle = new QLabel("Configuración");
    hdrTitle->setStyleSheet(QString("font-size: 24px; font-weight: 700; color: %1;").arg(C_TEXT));
    hdrL->addWidget(hdrTitle,1);
    layout->addWidget(hdr);

    // ── Sección: Atajos de teclado ────────────────────────────────────────
    auto makeSection = [&](const QString& iconCh, const QString& title, const QString& sub) {
        auto* sec = new QWidget(); sec->setStyleSheet("background:transparent;");
        auto* secL = new QVBoxLayout(sec); secL->setContentsMargins(0,0,0,8); secL->setSpacing(3);
        auto* row = new QWidget(); row->setStyleSheet("background:transparent;");
        auto* rowL = new QHBoxLayout(row); rowL->setContentsMargins(0,0,0,0); rowL->setSpacing(8);
        rowL->addWidget(Icon::label(row, iconCh, QColor(C_ACCENT), 16));
        auto* t = new QLabel(title);
        t->setStyleSheet(QString("font-size: 16px; font-weight: 700; color: %1;").arg(C_TEXT));
        rowL->addWidget(t,1); secL->addWidget(row);
        if (!sub.isEmpty()) {
            auto* s = new QLabel(sub);
            s->setStyleSheet(QString("font-size: 12px; color: %1;").arg(C_MUTED));
            secL->addWidget(s);
        }
        layout->addWidget(sec);
    };

    // ── Sección: Audio ───────────────────────────────────────────────────
    makeSection(Icon::volume(), "Audio",
                "Configuración de volumen y comportamiento de reproducción.");

    auto* audioCard = new QFrame();
    audioCard->setStyleSheet("QFrame { background: white; border: 1px solid #E0E7F2; border-radius: 12px; }");
    auto* audioL = new QVBoxLayout(audioCard); audioL->setContentsMargins(20,16,20,16); audioL->setSpacing(12);
    auto* pyRow = new QWidget(); pyRow->setStyleSheet("background:transparent;");
    auto* pyRowL = new QHBoxLayout(pyRow); pyRowL->setContentsMargins(0,0,0,0); pyRowL->setSpacing(10);
    auto* pyLabel = new QLabel("Ruta de Python:");
    pyLabel->setStyleSheet(QString("font-size: 13px; color: %1;").arg(C_TEXT));
    pyLabel->setFixedWidth(120);
    auto* pyEdit = new QLineEdit("python");
    pyEdit->setFixedHeight(32);
    pyEdit->setStyleSheet(QString(R"(QLineEdit{background:#F7F9FE;border:1px solid %1;border-radius:8px;padding:0 10px;font-size:12px;color:%2}QLineEdit:focus{border-color:%3;background:white})").arg(C_BORDER).arg(C_TEXT).arg(C_ACCENT));
    pyRowL->addWidget(pyLabel); pyRowL->addWidget(pyEdit,1);
    audioL->addWidget(pyRow);
    auto* pyNote = new QLabel("Usa 'python' si está en el PATH, o escribe la ruta completa (ej: C:\\Python311\\python.exe).");
    pyNote->setWordWrap(true);
    pyNote->setStyleSheet(QString("font-size: 11px; color: %1;").arg(C_MUTED));
    audioL->addWidget(pyNote);
    layout->addWidget(audioCard);

    // ── Sección: Acerca de ───────────────────────────────────────────────
    makeSection(Icon::record(), "Acerca de", "");
    auto* aboutCard = new QFrame();
    aboutCard->setStyleSheet("QFrame { background: white; border: 1px solid #E0E7F2; border-radius: 12px; }");
    auto* aboutL = new QVBoxLayout(aboutCard); aboutL->setContentsMargins(20,16,20,16); aboutL->setSpacing(6);
    auto addInfo = [&](const QString& key, const QString& val) {
        auto* r = new QWidget(); r->setStyleSheet("background:transparent;");
        auto* rl = new QHBoxLayout(r); rl->setContentsMargins(0,0,0,0);
        auto* k = new QLabel(key+":"); k->setFixedWidth(140);
        k->setStyleSheet(QString("font-size:12px;color:%1;font-weight:600;").arg(C_MUTED));
        auto* v = new QLabel(val);
        v->setStyleSheet(QString("font-size:12px;color:%1;").arg(C_TEXT));
        rl->addWidget(k); rl->addWidget(v,1);
        aboutL->addWidget(r);
    };
    addInfo("Nombre",    "OmniStream Deck (OSD-AUDIO)");
    addInfo("Versión",   "1.0.0");
    addInfo("Framework", "C++17 / Qt 6.12.0");
    addInfo("Repositorio","github.com/Suarezsh/OSD-AUDIO");
    addInfo("Proyecto",  "Tecnología de Objetos 2026-B — UNSA");
    layout->addWidget(aboutCard);
    layout->addStretch();

    scroll->setWidget(content);
    auto* pageL = new QVBoxLayout(page); pageL->setContentsMargins(0,0,0,0); pageL->addWidget(scroll);
    return page;
}

// ══════════════════════════════════════════════════════════════════════════════
//  NAVEGACIÓN
// ══════════════════════════════════════════════════════════════════════════════
void MainWindow::setNavActive(int page) {
    QPushButton* btns[] = { navBiblio_, navTikTok_, navConfig_ };
    const QString iconChars[] = { Icon::library(), Icon::live(), Icon::settings() };
    for (int i = 0; i < 3; ++i) {
        bool active = (i == page);
        btns[i]->setStyleSheet(active
            ? btns[i]->property("activeStyle").toString()
            : btns[i]->property("inactiveStyle").toString());
        btns[i]->setIcon(Icon::make(iconChars[i], active ? QColor(C_ACCENT) : QColor(C_TEXT), 18));
    }
}

void MainWindow::onNavClicked(int page) {
    stack_->setCurrentIndex(page);
    setNavActive(page);
}

// ══════════════════════════════════════════════════════════════════════════════
//  SLOTS
// ══════════════════════════════════════════════════════════════════════════════
void MainWindow::onButtonTriggered(const QString& soundFile, float volume, int index) {
    if (soundFile.isEmpty()) {
        nowPlayingLabel_->setText("Sin archivo asignado en esta tarjeta");
        statusMsgLabel_->setText("Click derecho en la tarjeta para asignar un audio.");
        return;
    }
    AudioRequest req;
    req.channelId = "priority_1";
    req.filePath  = soundFile.toStdString();
    req.volume    = volume;
    mixer_->enqueuePlay(req);
    mixer_->activateDucking(duckSlider_->value() / 100.0f);
}

void MainWindow::onFileAssigned(int /*id*/, const QString& filePath) {
    statusMsgLabel_->setText(filePath.isEmpty()
        ? "Efecto eliminado."
        : "Efecto guardado correctamente.");
    // SQLite ya fue actualizado por SoundDeckWidget
}

void MainWindow::onMasterVolumeChanged(int value) {
    masterVolLabel_->setText(QString("%1%").arg(value));
    mixer_->setMasterVolume(value / 100.0f);
}

void MainWindow::onDuckValueChanged(int value) {
    duckLabel_->setText(QString("%1 %").arg(value));
    applicationDucking_->setGlobalPercent(value);
}

void MainWindow::onSearchChanged(const QString& text) {
    soundDeck_->filterByName(text);
}

void MainWindow::onStopAll() {
    applicationDucking_->setActive(false);
    mixer_->stopAll();
    mixer_->deactivateDucking();
    tts_->stopAll();   // para la voz y limpia la cola
    nowPlayingLabel_->setText("Listo cuando tú quieras");
    statusMsgLabel_->setText("Audio detenido.");
    stopBtn_->setEnabled(false);
    addEventToFeed(Icon::stop(), "Audio detenido manualmente", QColor(C_MUTED));
}

void MainWindow::onDuckManual() {
    mixer_->activateDucking(duckSlider_->value() / 100.0f);
    addEventToFeed(Icon::duck(), "Duck manual activado", QColor(C_ACCENT));
}

void MainWindow::onConnectTikTok() {
    if (tiktokBridge_->isConnected()) {
        tiktokBridge_->disconnectFromServer();
    } else {
        QString user = serverUrlEdit_->text().trimmed();
        if (user.isEmpty()) {
            connStatus_->setText("Escribe tu usuario primero");
            return;
        }
        tiktokBridge_->connectToServer(user);
        connDot_->setStyleSheet("color: #F59E0B; font-size: 12px; background: transparent; border: none;");
        connStatus_->setText("Conectando...");
        connBtn_->setText(" Cancelar");
    }
}

void MainWindow::onTikTokConnected(const QString& username) {
    setConnectionStatus(true);
    miniTikTokDot_->setStyleSheet("color: #22C55E; font-size: 10px; background: transparent; border: none;");
    miniTikTokStatus_->setText("TikTok: " + username);
    addEventToFeed(Icon::connect(), "Conectado como " + username, QColor("#22C55E"));
}

void MainWindow::onTikTokDisconnected() {
    // Solo mostrar "sin conectar" si fue el usuario quien desconectó
    connDot_->setStyleSheet("color: #94A3B8; font-size: 12px; background: transparent; border: none;");
    connStatus_->setText("Sin conectar");
    connBtn_->setIcon(Icon::make(Icon::connect(), QColor("white"), 14));
    connBtn_->setText(" Conectar");
    connBtn_->setStyleSheet(QString("QPushButton { background: %1; color: white; border: none; border-radius: 10px; padding: 0 20px; font-size: 13px; font-weight: 700; } QPushButton:hover { background: #2550C8; }").arg(C_ACCENT));
    miniTikTokDot_->setStyleSheet("color: #94A3B8; font-size: 10px; background: transparent; border: none;");
    miniTikTokStatus_->setText("TikTok: desconectado");
    addEventToFeed(Icon::disconnect_(), "Desconectado del live", QColor("#94A3B8"));
}

void MainWindow::onTikTokReconnecting(int attempt, int maxAttempts) {
    QString msg = QString("Reconectando... (%1/%2)").arg(attempt).arg(maxAttempts);
    // Estado visual ambar — tranquilo, no alarmante
    connDot_->setStyleSheet("color: #F59E0B; font-size: 12px; background: transparent; border: none;");
    connStatus_->setText(msg);
    connBtn_->setIcon(Icon::make(Icon::cancel(), QColor("white"), 12));
    connBtn_->setText(" Cancelar");
    connBtn_->setStyleSheet("QPushButton { background: #64748B; color: white; border: none; border-radius: 10px; padding: 0 20px; font-size: 13px; font-weight: 700; } QPushButton:hover { background: #475569; }");
    miniTikTokDot_->setStyleSheet("color: #F59E0B; font-size: 10px; background: transparent; border: none;");
    miniTikTokStatus_->setText("TikTok: reconectando...");
    addEventToFeed(Icon::signal_(), msg, QColor("#F59E0B"));
}

void MainWindow::onTikTokError(const QString& msg) {
    connDot_->setStyleSheet("color: #94A3B8; font-size: 12px; background: transparent; border: none;");
    // Detectar si es "live no activo" para un mensaje más amable
    bool notLive = msg.contains("no está en vivo", Qt::CaseInsensitive)
                || msg.contains("not live",        Qt::CaseInsensitive);
    connStatus_->setText(notLive ? "No está en vivo ahora" : "No se pudo conectar");
    connBtn_->setIcon(Icon::make(Icon::connect(), QColor("white"), 14));
    connBtn_->setText(" Conectar");
    connBtn_->setStyleSheet(QString("QPushButton { background: %1; color: white; border: none; border-radius: 10px; padding: 0 20px; font-size: 13px; font-weight: 700; } QPushButton:hover { background: #2550C8; }").arg(C_ACCENT));
    miniTikTokDot_->setStyleSheet("color: #94A3B8; font-size: 10px; background: transparent; border: none;");
    miniTikTokStatus_->setText(notLive ? "TikTok: sin live activo" : "TikTok: sin conectar");
    QColor feedColor = notLive ? QColor("#F59E0B") : QColor("#94A3B8");
    addEventToFeed(notLive ? Icon::signal_() : Icon::cancel(), msg, feedColor);
}

void MainWindow::onComment(const QString& username, const QString& text) {
    addEventToFeed(Icon::comment(), username + ": " + text, QColor(C_TEXT));
    countComments_->setText(QString::number(++nComments_));
    if (ttsReadComments_ && ttsReadComments_->isChecked()) {
        bool withName = ttsNameComments_ && ttsNameComments_->isChecked();
        // Con nombre: "Laura dice: hola" / Sin nombre: solo el texto
        tts_->speak(withName ? username + " dice: " + text : text);
    }
    emit streamEventReceived("comment", username, 0);
}

void MainWindow::onGift(const QString& username, int amount, const QString& giftName) {
    addEventToFeed(Icon::gift(), QString("%1 envió %2x %3").arg(username).arg(amount).arg(giftName), QColor("#F59E0B"));
    countGifts_->setText(QString::number(++nGifts_));
    if (ttsReadGifts_ && ttsReadGifts_->isChecked()) {
        bool withName = ttsNameGifts_ && ttsNameGifts_->isChecked();
        // Con nombre: "Laura envió 5 Rose" / Sin nombre: "envió 5 Rose"
        tts_->speak((withName ? username + " " : QString()) + "envió " + QString::number(amount) + " " + giftName);
    }
    emit streamEventReceived("donation", username, amount);
}

void MainWindow::onFollow(const QString& username) {
    addEventToFeed(Icon::accept(), username + " te siguió", QColor("#22C55E"));
    countFollows_->setText(QString::number(++nFollows_));
    if (ttsReadFollows_ && ttsReadFollows_->isChecked()) {
        bool withName = ttsNameFollows_ && ttsNameFollows_->isChecked();
        tts_->speak(withName ? username + " te acaba de seguir" : "nuevo seguidor");
    }
    emit streamEventReceived("follow", username, 0);
}

void MainWindow::onLike(const QString& username, int count) {
    addEventToFeed(Icon::heart(), QString("%1 dio %2 likes").arg(username).arg(count), QColor("#E879F9"));
    countLikes_->setText(QString::number(++nLikes_));
    if (ttsReadLikes_ && ttsReadLikes_->isChecked()) {
        bool withName = ttsNameLikes_ && ttsNameLikes_->isChecked();
        // Con nombre: "Laura dio 11 likes" / Sin nombre: "11 likes"
        tts_->speak((withName ? username + " dio " : QString()) + QString::number(count) + " likes");
    }
    emit streamEventReceived("like", username, count);
}

void MainWindow::onPlaybackStarted(const QString&) {
    applicationDucking_->setActive(true);
    nowPlayingLabel_->setText("Reproduciendo...");
    stopBtn_->setEnabled(true);
}

void MainWindow::onPlaybackFinished(const QString&) {
    applicationDucking_->setActive(false);
    mixer_->deactivateDucking();
    nowPlayingLabel_->setText("Listo cuando tú quieras");
    stopBtn_->setEnabled(false);
}

// ── Helpers ───────────────────────────────────────────────────────────────────
void MainWindow::addEventToFeed(const QString& iconChar, const QString& text, const QColor& color) {
    if (!tiktokFeed_) return;
    // Eliminar placeholder si está
    if (tiktokFeed_->count() == 1 &&
        tiktokFeed_->item(0)->text() == "Esperando conexión...")
        delete tiktokFeed_->takeItem(0);

    auto* item = new QListWidgetItem(iconChar + "  " + text, tiktokFeed_);
    item->setForeground(color);
    if (tiktokFeed_->count() > 200) delete tiktokFeed_->takeItem(0);
    tiktokFeed_->scrollToBottom();
}

void MainWindow::setConnectionStatus(bool connected) {
    if (connected) {
        connDot_->setStyleSheet("color: #22C55E; font-size: 12px; background: transparent; border: none;");
        connStatus_->setText("Conectado");
        connBtn_->setIcon(Icon::make(Icon::disconnect_(), QColor("white"), 14));
        connBtn_->setText(" Desconectar");
        connBtn_->setStyleSheet("QPushButton { background: #DC2626; color: white; border: none; border-radius: 10px; padding: 0 20px; font-size: 13px; font-weight: 700; } QPushButton:hover { background: #B91C1C; }");
    } else {
        connDot_->setStyleSheet("color: #E03030; font-size: 12px; background: transparent; border: none;");
        connStatus_->setText("Sin conectar");
        connBtn_->setIcon(Icon::make(Icon::connect(), QColor("white"), 14));
        connBtn_->setText(" Conectar");
        connBtn_->setStyleSheet(QString("QPushButton { background: %1; color: white; border: none; border-radius: 10px; padding: 0 20px; font-size: 13px; font-weight: 700; } QPushButton:hover { background: #2550C8; }").arg(C_ACCENT));
    }
}

void MainWindow::closeEvent(QCloseEvent* e) {
    QMessageBox confirmation(QMessageBox::Warning, "Cerrar OmniStream Deck",
        "Al salir, OmniStream Deck se desconectará de TikTok LIVE, "
        "detendrá los sonidos y la lectura en voz alta, y liberará los atajos. "
        "La aplicación se cerrará por completo.",
        QMessageBox::NoButton, this);
    auto* exitButton = confirmation.addButton("Cerrar aplicación", QMessageBox::AcceptRole);
    auto* cancelButton = confirmation.addButton("Seguir usando", QMessageBox::RejectRole);
    confirmation.setDefaultButton(cancelButton);
    confirmation.setEscapeButton(cancelButton);

    confirmation.exec();
    if (confirmation.clickedButton() != exitButton) {
        e->ignore();
        return;
    }

    soundDeck_->setHotkeysEnabled(false);
    applicationDucking_->setActive(false);
    tiktokBridge_->disconnectFromServer();
    tts_->stopAll();
    mixer_->stopAll();
    saveSettings();
    e->accept();
}
