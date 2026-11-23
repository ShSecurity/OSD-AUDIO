#include "SoundCardEditor.h"
#include "Icon.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFileDialog>
#include <QFileInfo>
#include <QDir>
#include <QFrame>

static const char* COLORS[8] = {
    "#EBF1FF", "#FFF0F4", "#F0FFF4", "#FFF8E1",
    "#F5F0FF", "#E3F2FD", "#FCE4EC", "#E8F5E9"
};
static const int   COLOR_COUNT = 8;

static auto sectionLabel(const QString& text, QWidget* parent = nullptr) {
    auto* lbl = new QLabel(text, parent);
    lbl->setStyleSheet("font-size: 11px; font-weight: 600; color: #7C8AA3;"
                       " background: transparent; border: none; letter-spacing: 0.5px;");
    return lbl;
}

SoundCardEditor::SoundCardEditor(const SoundData& data, QWidget* parent)
    : QDialog(parent), data_(data)
{
    bool isNew = (data_.id == -1);
    setWindowTitle(isNew ? "Nuevo efecto" : "Editar efecto");
    setModal(true);
    setFixedWidth(420);
    setStyleSheet(R"(
        QDialog  { background: #FFFFFF; }
        QLineEdit {
            border: 1.5px solid #E0E7F2; border-radius: 8px;
            padding: 8px 12px; font-size: 13px; color: #1A2340;
            background: #F7F9FC;
        }
        QLineEdit:focus { border-color: #3064E8; background: white; }
        QLineEdit:read-only { color: #7C8AA3; }
    )");

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 22, 24, 20);
    root->setSpacing(0);

    // ── Título ────────────────────────────────────────────────────────────────
    auto* titleRow = new QHBoxLayout();
    titleRow->setContentsMargins(0,0,0,18);
    auto* titleLbl = new QLabel(isNew ? "Nuevo efecto" : "Editar efecto");
    titleLbl->setStyleSheet("font-size: 15px; font-weight: 700; color: #1A2340;"
                            " background: transparent; border: none;");
    auto* closeBtn = new QPushButton();
    closeBtn->setFixedSize(28, 28);
    closeBtn->setStyleSheet(R"(
        QPushButton { background: transparent; border: none; color: #7C8AA3;
                      border-radius: 6px; font-size: 14px; }
        QPushButton:hover { background: #FEE2E2; color: #E03030; }
    )");
    QFont mf2("Segoe MDL2 Assets", 10);
    closeBtn->setFont(mf2);
    closeBtn->setText(Icon::cancel());
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::reject);
    titleRow->addWidget(titleLbl, 1);
    titleRow->addWidget(closeBtn);
    root->addLayout(titleRow);

    // ── Nombre ────────────────────────────────────────────────────────────────
    root->addWidget(sectionLabel("NOMBRE"));
    root->addSpacing(5);
    nameEdit_ = new QLineEdit(data_.name);
    nameEdit_->setPlaceholderText("Nombre del efecto");
    nameEdit_->setFixedHeight(38);
    root->addWidget(nameEdit_);
    root->addSpacing(14);

    // ── Categoría ─────────────────────────────────────────────────────────────
    root->addWidget(sectionLabel("CATEGORÍA"));
    root->addSpacing(5);
    catEdit_ = new QLineEdit(data_.category);
    catEdit_->setPlaceholderText("Alertas, Música, Efectos...");
    catEdit_->setFixedHeight(38);
    root->addWidget(catEdit_);
    root->addSpacing(14);

    // ── Archivo de audio ─────────────────────────────────────────────────────
    root->addWidget(sectionLabel("ARCHIVO DE AUDIO"));
    root->addSpacing(5);
    auto* fileRow = new QWidget();
    fileRow->setStyleSheet("background: transparent;");
    auto* fileRowL = new QHBoxLayout(fileRow);
    fileRowL->setContentsMargins(0,0,0,0); fileRowL->setSpacing(8);
    fileEdit_ = new QLineEdit();
    fileEdit_->setFixedHeight(38);
    fileEdit_->setReadOnly(true);
    fileEdit_->setPlaceholderText("Sin archivo asignado");
    if (!data_.filePath.isEmpty())
        fileEdit_->setText(QFileInfo(data_.filePath).fileName());
    auto* browseBtn = new QPushButton("Seleccionar");
    browseBtn->setFixedSize(100, 38);
    browseBtn->setStyleSheet(R"(
        QPushButton { background: #3064E8; color: white; border: none;
                      border-radius: 8px; font-size: 12px; font-weight: 600; }
        QPushButton:hover { background: #2555D6; }
    )");
    connect(browseBtn, &QPushButton::clicked, this, &SoundCardEditor::pickFile);
    fileRowL->addWidget(fileEdit_, 1);
    fileRowL->addWidget(browseBtn);
    root->addWidget(fileRow);
    root->addSpacing(14);

    // ── Volumen ───────────────────────────────────────────────────────────────
    root->addWidget(sectionLabel("VOLUMEN"));
    root->addSpacing(5);
    auto* volRow = new QWidget();
    volRow->setStyleSheet("background: transparent;");
    auto* volRowL = new QHBoxLayout(volRow);
    volRowL->setContentsMargins(0,0,0,0); volRowL->setSpacing(10);
    volSlider_ = new QSlider(Qt::Horizontal);
    volSlider_->setRange(10, 100);
    volSlider_->setValue(qRound(data_.volume * 100.0f));
    volSlider_->setFixedHeight(20);
    volSlider_->setStyleSheet(R"(
        QSlider::groove:horizontal { height:4px; background:#E0E7F2; border-radius:2px; }
        QSlider::handle:horizontal { width:14px; height:14px; margin:-5px 0;
            background:#3064E8; border-radius:7px; }
        QSlider::sub-page:horizontal { background:#3064E8; border-radius:2px; }
    )");
    volLabel_ = new QLabel(QString::number(volSlider_->value()) + "%");
    volLabel_->setFixedWidth(38);
    volLabel_->setStyleSheet("font-size: 12px; color: #1A2340; background: transparent; border: none;");
    connect(volSlider_, &QSlider::valueChanged, this, [this](int v){
        volLabel_->setText(QString::number(v) + "%");
    });
    volRowL->addWidget(volSlider_, 1);
    volRowL->addWidget(volLabel_);
    root->addWidget(volRow);
    root->addSpacing(14);

    // ── Atajo de teclado ─────────────────────────────────────────────────────
    root->addWidget(sectionLabel("ATAJO DE TECLADO"));
    root->addSpacing(5);
    shortcutEdit_ = new QKeySequenceEdit(QKeySequence(data_.shortcut), this);
    shortcutEdit_->setFixedHeight(38);
    // Limitar a 1 sola combinación (no permite Tecla1, Tecla2, ...)
    connect(shortcutEdit_, &QKeySequenceEdit::keySequenceChanged,
            shortcutEdit_,  [this](const QKeySequence& seq) {
        if (seq.count() > 1)
            shortcutEdit_->setKeySequence(QKeySequence(seq[0]));
    });
    shortcutEdit_->setStyleSheet(R"(
        QKeySequenceEdit {
            border: 1.5px solid #E0E7F2; border-radius: 8px;
            padding: 4px 12px; font-size: 13px; color: #1A2340;
            background: #F7F9FC;
        }
        QKeySequenceEdit:focus { border-color: #3064E8; background: white; }
    )");
    root->addWidget(shortcutEdit_);
    root->addSpacing(14);

    // ── Color de acento ───────────────────────────────────────────────────────
    root->addWidget(sectionLabel("COLOR"));
    root->addSpacing(5);
    auto* colorRow = new QWidget();
    colorRow->setStyleSheet("background: transparent;");
    auto* colorRowL = new QHBoxLayout(colorRow);
    colorRowL->setContentsMargins(0,0,0,0); colorRowL->setSpacing(8);

    // Find current color index
    colorIndex_ = 0;
    for (int i = 0; i < COLOR_COUNT; ++i)
        if (data_.accentColor == COLORS[i]) { colorIndex_ = i; break; }

    for (int i = 0; i < COLOR_COUNT; ++i) {
        colorBtns_[i] = new QPushButton();
        colorBtns_[i]->setFixedSize(28, 28);
        colorBtns_[i]->setStyleSheet(QString(R"(
            QPushButton { background: %1; border: 2.5px solid transparent;
                          border-radius: 14px; }
            QPushButton:hover { border-color: #94A3B8; }
        )").arg(COLORS[i]));
        connect(colorBtns_[i], &QPushButton::clicked, this, [this, i](){ selectColor(i); });
        colorRowL->addWidget(colorBtns_[i]);
    }
    colorRowL->addStretch();
    root->addWidget(colorRow);

    // Mark initial selection
    colorBtns_[colorIndex_]->setStyleSheet(QString(R"(
        QPushButton { background: %1; border: 2.5px solid #3064E8; border-radius: 14px; }
    )").arg(COLORS[colorIndex_]));

    // ── Separador ─────────────────────────────────────────────────────────────
    root->addSpacing(18);
    auto* sep = new QFrame();
    sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet("color: #E0E7F2; background: #E0E7F2; border: none; max-height: 1px;");
    root->addWidget(sep);
    root->addSpacing(14);

    // ── Botones de acción ─────────────────────────────────────────────────────
    auto* btnRow = new QHBoxLayout();
    btnRow->setSpacing(8);

    if (!isNew) {
        auto* delBtn = new QPushButton("Eliminar");
        delBtn->setFixedHeight(38);
        delBtn->setStyleSheet(R"(
            QPushButton { background: #FEE2E2; color: #E03030; border: none;
                          border-radius: 8px; font-size: 13px; font-weight: 600;
                          padding: 0 16px; }
            QPushButton:hover { background: #FCA5A5; }
        )");
        connect(delBtn, &QPushButton::clicked, this, [this](){
            deleteRequested_ = true;
            accept();
        });
        btnRow->addWidget(delBtn);
    }

    btnRow->addStretch();

    auto* cancelBtn = new QPushButton("Cancelar");
    cancelBtn->setFixedHeight(38);
    cancelBtn->setStyleSheet(R"(
        QPushButton { background: #F4F7FC; color: #586A87; border: none;
                      border-radius: 8px; font-size: 13px; font-weight: 600;
                      padding: 0 16px; }
        QPushButton:hover { background: #E0E7F2; }
    )");
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);

    auto* saveBtn = new QPushButton("Guardar");
    saveBtn->setFixedHeight(38);
    saveBtn->setStyleSheet(R"(
        QPushButton { background: #3064E8; color: white; border: none;
                      border-radius: 8px; font-size: 13px; font-weight: 600;
                      padding: 0 20px; }
        QPushButton:hover { background: #2555D6; }
    )");
    connect(saveBtn, &QPushButton::clicked, this, [this](){
        data_.name        = nameEdit_->text().trimmed();
        data_.category    = catEdit_->text().trimmed();
        data_.accentColor = COLORS[colorIndex_];
        data_.volume      = volSlider_->value() / 100.0f;
        data_.shortcut    = shortcutEdit_->keySequence().toString();
        if (data_.name.isEmpty()) data_.name = "Efecto";
        accept();
    });

    btnRow->addWidget(cancelBtn);
    btnRow->addWidget(saveBtn);
    root->addLayout(btnRow);
}

void SoundCardEditor::pickFile() {
    QString file = QFileDialog::getOpenFileName(
        this,
        "Seleccionar audio",
        data_.filePath.isEmpty() ? QDir::homePath() : QFileInfo(data_.filePath).absolutePath(),
        "Archivos de audio (*.wav *.mp3 *.ogg *.flac *.aac *.m4a *.wma);;Todos (*.*)");
    if (!file.isEmpty()) {
        data_.filePath = file;
        fileEdit_->setText(QFileInfo(file).fileName());
        // Auto-llenar nombre con el nombre del archivo si el campo está vacío
        if (nameEdit_->text().trimmed().isEmpty())
            nameEdit_->setText(QFileInfo(file).baseName());
    }
}

void SoundCardEditor::selectColor(int index) {
    // Quitar selección anterior
    colorBtns_[colorIndex_]->setStyleSheet(QString(R"(
        QPushButton { background: %1; border: 2.5px solid transparent; border-radius: 14px; }
        QPushButton:hover { border-color: #94A3B8; }
    )").arg(COLORS[colorIndex_]));
    colorIndex_ = index;
    colorBtns_[colorIndex_]->setStyleSheet(QString(R"(
        QPushButton { background: %1; border: 2.5px solid #3064E8; border-radius: 14px; }
    )").arg(COLORS[colorIndex_]));
}
