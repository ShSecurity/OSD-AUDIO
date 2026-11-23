#include "SoundDeckWidget.h"
#include "SoundCardEditor.h"
#include "Icon.h"
#include "../../include/core/HotkeyManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QLabel>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QVariant>

static const int COLS_GRID = 3;
static const int COLS_LIST = 1;

SoundDeckWidget::SoundDeckWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);

    scroll_ = new QScrollArea(this);
    scroll_->setWidgetResizable(true);
    scroll_->setFrameShape(QFrame::NoFrame);
    scroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll_->setStyleSheet("QScrollArea { background: transparent; border: none; }");

    container_ = new QWidget();
    container_->setStyleSheet("background: transparent;");

    grid_ = new QGridLayout(container_);
    grid_->setSpacing(10);
    grid_->setContentsMargins(0, 0, 8, 0);
    grid_->setColumnStretch(0, 1);
    grid_->setColumnStretch(1, 1);
    grid_->setColumnStretch(2, 1);

    scroll_->setWidget(container_);
    outerLayout->addWidget(scroll_);
}

void SoundDeckWidget::setDbConnection(const QString& connectionName) {
    dbConn_ = connectionName;
    loadFromDb();
}

// ──── Cargar todos los sonidos desde SQLite ───────────────────────────────────
void SoundDeckWidget::loadFromDb() {
    QSqlDatabase db = QSqlDatabase::database(dbConn_);
    if (!db.isOpen()) return;

    QSqlQuery q(db);
    q.exec("SELECT id, name, category, file_path, accent_color, volume, shortcut "
           "FROM sounds ORDER BY sort_order ASC, id ASC");

    std::vector<SoundData> entries;
    while (q.next()) {
        SoundData d;
        d.id          = q.value(0).toInt();
        d.name        = q.value(1).toString();
        d.category    = q.value(2).toString();
        d.filePath    = q.value(3).toString();
        d.accentColor = q.value(4).toString();
        d.volume      = q.value(5).toFloat();
        d.shortcut    = q.value(6).toString();
        entries.push_back(d);
    }

    // Limpiar tarjetas existentes
    for (auto* c : cards_) c->deleteLater();
    cards_.clear();

    // Crear nuevas tarjetas
    for (const auto& d : entries) {
        auto* card = new SoundCard(d, container_);
        connect(card, &SoundCard::triggered,    this, &SoundDeckWidget::onCardTriggered);
        connect(card, &SoundCard::editRequested,this, &SoundDeckWidget::onEditRequested);
        cards_.push_back(card);
    }

    rebuildGrid();
    applyHotkeys();
}

// ──── Re-colocar las tarjetas + botón "Agregar" en el grid ───────────────────
void SoundDeckWidget::rebuildGrid() {
    while (grid_->count()) {
        QLayoutItem* item = grid_->takeAt(0);
        delete item;
    }
    if (addBtn_)    { addBtn_->deleteLater();    addBtn_    = nullptr; }
    if (listWidget_){ listWidget_->deleteLater(); listWidget_ = nullptr; }

    // ── addBtn_ siempre es hijo directo de container_ ──────────────────────
    addBtn_ = new QPushButton(container_);
    addBtn_->setCursor(Qt::PointingHandCursor);
    addBtn_->setText("  +   Agregar efecto");
    connect(addBtn_, &QPushButton::clicked, this, &SoundDeckWidget::onAddClicked);

    if (listMode_) {
        addBtn_->setFixedHeight(44);
        addBtn_->setStyleSheet(R"(
            QPushButton { background: transparent; border: 2px dashed #CFDDF8;
                          border-radius: 8px; font-size: 13px; font-weight: 600; color: #3064E8; }
            QPushButton:hover { background: #EBF1FF; border-color: #3064E8; }
        )");
        grid_->setColumnStretch(0, 1);
        grid_->setColumnStretch(1, 0);
        grid_->setColumnStretch(2, 0);
        rebuildList();                        // llena listWidget_ en fila 0
        grid_->addWidget(addBtn_, 1, 0);
        grid_->setRowStretch(2, 1);
    } else {
        addBtn_->setFixedHeight(82);
        addBtn_->setStyleSheet(R"(
            QPushButton { background: #F7F9FC; border: 2px dashed #CFDDF8;
                          border-radius: 12px; font-size: 13px; font-weight: 600; color: #3064E8; }
            QPushButton:hover { background: #EBF1FF; border-color: #3064E8; }
        )");
        grid_->setColumnStretch(0, 1);
        grid_->setColumnStretch(1, 1);
        grid_->setColumnStretch(2, 1);

        int col = 0, row = 0;
        for (auto* card : cards_) {
            card->setVisible(true);
            grid_->addWidget(card, row, col);
            if (++col >= COLS_GRID) { col = 0; ++row; }
        }
        grid_->addWidget(addBtn_, row, col);
        grid_->setRowStretch(row + 2, 1);
    }
}

// ──── Vista lista: tabla con columnas Nombre / Categoría / Atajo ─────────────
void SoundDeckWidget::rebuildList() {
    listWidget_ = new QWidget(container_);
    listWidget_->setStyleSheet(
        "QWidget#listOuter { background: white; border: 1px solid #E0E7F2; border-radius: 12px; }");
    listWidget_->setObjectName("listOuter");

    auto* listLayout = new QVBoxLayout(listWidget_);
    listLayout->setContentsMargins(0, 0, 0, 0);
    listLayout->setSpacing(0);

    // ── Cabecera de columnas ──────────────────────────────────────────────────
    auto* headerRow = new QWidget(listWidget_);
    headerRow->setStyleSheet("background: transparent;");
    auto* hL = new QHBoxLayout(headerRow);
    hL->setContentsMargins(16, 10, 16, 10);
    hL->setSpacing(0);

    auto hdr = [](const QString& t, QWidget* p = nullptr) {
        auto* l = new QLabel(t, p);
        l->setStyleSheet(
            "font-size: 11px; font-weight: 600; color: #9CA3AF;"
            " background: transparent; border: none;");
        return l;
    };

    hL->addSpacing(44);              // hueco para círculo de play
    hL->addWidget(hdr("Nombre"),   3);
    hL->addWidget(hdr("Categoría"),1);
    hL->addWidget(hdr("Atajo"),    1);
    hL->addSpacing(28);              // hueco para ⋮

    headerRow->setFixedHeight(36);
    listLayout->addWidget(headerRow);

    // separador bajo la cabecera
    auto makeLine = [&](const QString& color) {
        auto* f = new QFrame(listWidget_);
        f->setFrameShape(QFrame::HLine);
        f->setStyleSheet(QString("background: %1; border: none; max-height: 1px;").arg(color));
        return f;
    };
    listLayout->addWidget(makeLine("#E0E7F2"));

    // ── Filas de efectos ──────────────────────────────────────────────────────
    for (auto* card : cards_) {
        if (!card->isVisible()) continue;
        const SoundData& d = card->data();

        auto* row = new QWidget(listWidget_);
        row->setObjectName("listRow");
        row->setStyleSheet(
            "QWidget#listRow { background: white; }"
            "QWidget#listRow:hover { background: #F8FAFF; }");

        auto* rL = new QHBoxLayout(row);
        rL->setContentsMargins(16, 10, 16, 10);
        rL->setSpacing(0);

        // ── Círculo play ──────────────────────────────────────────────────────
        auto* playBtn = new QPushButton(row);
        playBtn->setFixedSize(32, 32);
        playBtn->setCursor(Qt::PointingHandCursor);
        playBtn->setStyleSheet(R"(
            QPushButton { background: #EBF1FF; border-radius: 16px; border: none; }
            QPushButton:hover { background: #DBEAFE; }
        )");
        auto* pi = Icon::label(playBtn, Icon::play(), QColor("#3064E8"), 9);
        pi->setGeometry(0, 0, 32, 32);
        pi->setAttribute(Qt::WA_TransparentForMouseEvents);
        int cid  = d.id;
        QString cfile = d.filePath;
        float   cvol  = d.volume;
        connect(playBtn, &QPushButton::clicked, this, [this, cid, cfile, cvol]() {
            if (!cfile.isEmpty()) emit buttonTriggered(cfile, cvol, cid);
        });
        rL->addWidget(playBtn);
        rL->addSpacing(12);

        // ── Nombre ────────────────────────────────────────────────────────────
        QString name = d.name.trimmed();
        if (name.isEmpty() && !d.filePath.isEmpty())
            name = QFileInfo(d.filePath).baseName();
        if (name.isEmpty()) name = "Sin nombre";

        auto* nameL = new QLabel(name, row);
        nameL->setStyleSheet(
            "font-weight: 600; font-size: 13px; color: #1A2340;"
            " background: transparent; border: none;");
        nameL->setAttribute(Qt::WA_TransparentForMouseEvents);
        rL->addWidget(nameL, 3);

        // ── Categoría ─────────────────────────────────────────────────────────
        auto* catL = new QLabel(d.category.isEmpty() ? "General" : d.category, row);
        catL->setStyleSheet(
            "font-size: 12px; color: #6B7280; background: transparent; border: none;");
        catL->setAttribute(Qt::WA_TransparentForMouseEvents);
        rL->addWidget(catL, 1);

        // ── Atajo ─────────────────────────────────────────────────────────────
        auto* shL = new QLabel(d.shortcut.isEmpty() ? "" : d.shortcut, row);
        shL->setStyleSheet(
            "font-size: 12px; color: #6B7280; background: transparent; border: none;");
        shL->setAttribute(Qt::WA_TransparentForMouseEvents);
        rL->addWidget(shL, 1);

        // ── ⋮ botón ───────────────────────────────────────────────────────────
        auto* more = new QPushButton(row);
        more->setFixedSize(28, 28);
        more->setCursor(Qt::PointingHandCursor);
        QFont mf("Segoe MDL2 Assets", 11);
        more->setFont(mf);
        more->setText(Icon::more());
        more->setStyleSheet(R"(
            QPushButton { background: transparent; border: none;
                          color: #B0BCCC; border-radius: 6px; }
            QPushButton:hover { background: #EBF1FF; color: #3064E8; }
        )");
        int mid = d.id;
        connect(more, &QPushButton::clicked, this, [this, mid]() {
            onEditRequested(mid);
        });
        rL->addWidget(more);

        row->setFixedHeight(54);
        listLayout->addWidget(row);
        listLayout->addWidget(makeLine("#F3F4F6"));
    }

    listLayout->addStretch();
    grid_->addWidget(listWidget_, 0, 0, 1, 3);
}

// ──── Guardar o actualizar un efecto en SQLite ───────────────────────────────
void SoundDeckWidget::saveToDb(const SoundData& data) {
    QSqlDatabase db = QSqlDatabase::database(dbConn_);
    if (!db.isOpen()) return;
    QSqlQuery q(db);
    if (data.id == -1) {
        q.prepare("INSERT INTO sounds (name, category, file_path, accent_color, volume, shortcut, sort_order)"
                  " VALUES (:n, :c, :f, :a, :v, :s, (SELECT COALESCE(MAX(sort_order),0)+1 FROM sounds))");
    } else {
        q.prepare("UPDATE sounds SET name=:n, category=:c, file_path=:f,"
                  " accent_color=:a, volume=:v, shortcut=:s WHERE id=:id");
        q.bindValue(":id", data.id);
    }
    q.bindValue(":n", data.name);
    q.bindValue(":c", data.category);
    q.bindValue(":f", data.filePath);
    q.bindValue(":a", data.accentColor);
    q.bindValue(":v", static_cast<double>(data.volume));
    q.bindValue(":s", data.shortcut);
    q.exec();
}

void SoundDeckWidget::deleteFromDb(int id) {
    QSqlDatabase db = QSqlDatabase::database(dbConn_);
    if (!db.isOpen()) return;
    QSqlQuery q(db);
    q.prepare("DELETE FROM sounds WHERE id = :id");
    q.bindValue(":id", id);
    q.exec();
}

// ──── Slots ───────────────────────────────────────────────────────────────────
void SoundDeckWidget::onCardTriggered(const QString& soundFile, float volume, int id) {
    emit buttonTriggered(soundFile, volume, id);
}

void SoundDeckWidget::onEditRequested(int id) {
    // Buscar el data de la tarjeta
    SoundData current;
    for (auto* c : cards_)
        if (c->cardId() == id) { current = c->data(); break; }

    SoundCardEditor dlg(current, this);
    if (dlg.exec() != QDialog::Accepted) return;

    if (dlg.wantsDelete()) {
        deleteFromDb(id);
        emit fileAssigned(id, QString());
    } else {
        SoundData updated = dlg.result();
        updated.id = id;
        saveToDb(updated);
        emit fileAssigned(id, updated.filePath);
    }
    loadFromDb();
}

void SoundDeckWidget::onAddClicked() {
    SoundData blank;
    SoundCardEditor dlg(blank, this);
    if (dlg.exec() != QDialog::Accepted) return;

    SoundData newData = dlg.result();
    saveToDb(newData);
    loadFromDb();
    emit fileAssigned(-1, newData.filePath);
}

// ──── Filtro de búsqueda ──────────────────────────────────────────────────────
void SoundDeckWidget::filterByName(const QString& query) {
    for (auto* card : cards_) {
        bool visible = query.isEmpty() ||
                       card->cardName().contains(query, Qt::CaseInsensitive);
        card->setVisible(visible);
    }
}

void SoundDeckWidget::setListView(bool list) {
    if (listMode_ == list) return;
    listMode_ = list;
    rebuildGrid();
}

void SoundDeckWidget::registerHotkeys(HotkeyManager* hm) {
    hotkeyMgr_ = hm;
    applyHotkeys();
}

void SoundDeckWidget::setHotkeysEnabled(bool enabled) {
    if (hotkeysEnabled_ == enabled) return;
    hotkeysEnabled_ = enabled;
    applyHotkeys();
}

void SoundDeckWidget::applyHotkeys() {
    if (!hotkeyMgr_) return;
    hotkeyMgr_->clearAll();
    if (!hotkeysEnabled_) return;
    for (auto* card : cards_) {
        const SoundData& d = card->data();
        if (d.shortcut.isEmpty() || d.filePath.isEmpty()) continue;
        QKeySequence seq(d.shortcut);
        if (seq.isEmpty()) continue;
        QString hotkeyId = QString("sound_%1").arg(d.id);
        hotkeyMgr_->registerHotkey(hotkeyId, seq, [this, card]() {
            emit buttonTriggered(card->soundFile(), card->volume(), card->cardId());
        });
    }
}
