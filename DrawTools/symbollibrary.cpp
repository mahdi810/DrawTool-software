#include "symbollibrary.h"

#include <QBuffer>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLineEdit>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QListWidget>
#include <QLabel>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QStandardPaths>
#include <QVBoxLayout>

static QString customLibrariesFilePath()
{
    QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (base.isEmpty()) {
        base = QDir::homePath() + "/.drawtools";
    }
    QDir dir(base);
    dir.mkpath(".");
    return dir.filePath("symbol_libraries.json");
}

static QString encodePixmapBase64(const QPixmap &pix)
{
    QByteArray bytes;
    QBuffer buffer(&bytes);
    if (!buffer.open(QIODevice::WriteOnly)) return QString();
    if (!pix.save(&buffer, "PNG")) return QString();
    return QString::fromLatin1(bytes.toBase64());
}

static QPixmap decodePixmapBase64(const QString &encoded)
{
    QPixmap pix;
    const QByteArray bytes = QByteArray::fromBase64(encoded.toLatin1());
    pix.loadFromData(bytes, "PNG");
    return pix;
}

static QPixmap makeHiDpiSymbolCanvas()
{
    QPixmap px(192, 144);
    px.setDevicePixelRatio(2.0); // logical size: 96x72 with sharper rendering
    px.fill(Qt::transparent);
    return px;
}

static void drawPortMarkers(QPainter &p, const QList<QPointF> &ports)
{
    p.save();
    p.setPen(QPen(QColor(10, 102, 194), 1.2));
    p.setBrush(QColor(20, 130, 240));
    for (const QPointF &pt : ports) {
        p.drawEllipse(pt, 2.1, 2.1);
    }
    p.restore();
}

SymbolLibrary::SymbolLibrary(QWidget *parent)
    : QDockWidget("Symbol Library", parent)
{
    QWidget *container = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(container);
    layout->setContentsMargins(4, 4, 4, 4);

    QHBoxLayout *libRow = new QHBoxLayout;
    libRow->setContentsMargins(0, 0, 0, 0);
    libRow->setSpacing(4);
    libRow->addWidget(new QLabel("Library:"));

    m_libraryCombo = new QComboBox(container);
    libRow->addWidget(m_libraryCombo, 1);

    m_addLibraryBtn = new QPushButton("+", container);
    m_addLibraryBtn->setToolTip("Create custom library");
    m_addLibraryBtn->setFixedWidth(24);
    libRow->addWidget(m_addLibraryBtn);

    m_removeLibraryBtn = new QPushButton("-", container);
    m_removeLibraryBtn->setToolTip("Delete selected custom library");
    m_removeLibraryBtn->setFixedWidth(24);
    libRow->addWidget(m_removeLibraryBtn);

    layout->addLayout(libRow);
    layout->addWidget(new QLabel("Double-click to place:"));

    m_listWidget = new QListWidget(container);
    m_listWidget->setIconSize(QSize(96, 72));
    m_listWidget->setViewMode(QListWidget::IconMode);
    m_listWidget->setResizeMode(QListWidget::Adjust);
    m_listWidget->setGridSize(QSize(112, 88));
    m_listWidget->setSpacing(4);
    layout->addWidget(m_listWidget);

    m_removeSymbolBtn = new QPushButton("Delete selected symbol", container);
    layout->addWidget(m_removeSymbolBtn);

    container->setLayout(layout);
    setWidget(container);
    setMinimumWidth(170);

    connect(m_libraryCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) {
                refreshSymbolList();
                updateButtons();
            });

    connect(m_addLibraryBtn, &QPushButton::clicked, this, [this]() {
        bool ok = false;
        const QString name = QInputDialog::getText(this,
                                                   "New Custom Library",
                                                   "Library name:",
                                                   QLineEdit::Normal,
                                                   "My Symbols",
                                                   &ok).trimmed();
        if (!ok || name.isEmpty()) return;
        if (findLibraryIndex(name) >= 0) {
            QMessageBox::warning(this, "Library Exists", "A library with this name already exists.");
            return;
        }
        addLibraryInternal(name, true);
        rebuildLibraryCombo();
        m_libraryCombo->setCurrentIndex(findLibraryIndex(name));
        saveCustomLibraries();
    });

    connect(m_removeLibraryBtn, &QPushButton::clicked, this, [this]() {
        const int idx = currentLibraryIndex();
        if (idx < 0 || idx >= m_groups.size()) return;
        if (!m_groups[idx].custom) return;

        const auto btn = QMessageBox::question(this,
                                               "Delete Library",
                                               "Delete custom library '" + m_groups[idx].name + "' and all its symbols?");
        if (btn != QMessageBox::Yes) return;

        m_groups.removeAt(idx);
        if (m_groups.isEmpty()) {
            addLibraryInternal("Digital Electronics", false);
        }
        rebuildLibraryCombo();
        m_libraryCombo->setCurrentIndex(qBound(0, idx - 1, m_groups.size() - 1));
        refreshSymbolList();
        updateButtons();
        saveCustomLibraries();
    });

    connect(m_removeSymbolBtn, &QPushButton::clicked, this, [this]() {
        const int gidx = currentLibraryIndex();
        if (gidx < 0 || gidx >= m_groups.size()) return;
        if (!m_groups[gidx].custom) return;

        const int sidx = m_listWidget->currentRow();
        if (sidx < 0 || sidx >= m_groups[gidx].symbols.size()) return;

        m_groups[gidx].symbols.removeAt(sidx);
        refreshSymbolList();
        updateButtons();
        saveCustomLibraries();
    });

    connect(m_listWidget, &QListWidget::itemDoubleClicked, this, [this]() {
        int idx = m_listWidget->currentRow();
        if (idx >= 0) emit symbolActivated(idx);
    });

    connect(m_listWidget, &QListWidget::itemSelectionChanged,
            this, &SymbolLibrary::updateButtons);

    loadDefaultLibraries();
    loadCustomLibraries();
    if (firstCustomLibraryIndex() < 0) {
        addLibraryInternal("My Symbols", true);
    }
    rebuildLibraryCombo();
    m_libraryCombo->setCurrentIndex(0);
    refreshSymbolList();
    updateButtons();
}

void SymbolLibrary::addSymbol(const QString &name, const QPixmap &thumbnail)
{
    if (thumbnail.isNull()) return;

    const QString targetLibrary = promptForCustomLibrary();
    if (targetLibrary.isEmpty()) return;
    addSymbolToLibrary(targetLibrary, name, thumbnail, {}, true);
}

void SymbolLibrary::addSymbolToLibrary(const QString &libraryName,
                                       const QString &name,
                                       const QPixmap &thumbnail,
                                       const QList<QPointF> &portsNormalized,
                                       bool customLibrary)
{
    if (libraryName.trimmed().isEmpty() || thumbnail.isNull()) return;

    int libIdx = findLibraryIndex(libraryName);
    if (libIdx < 0) {
        addLibraryInternal(libraryName, customLibrary);
        libIdx = findLibraryIndex(libraryName);
    }
    if (libIdx < 0) return;

    m_groups[libIdx].symbols.append({name, thumbnail, portsNormalized});

    rebuildLibraryCombo();
    m_libraryCombo->setCurrentIndex(libIdx);
    refreshSymbolList();
    updateButtons();
    saveCustomLibraries();
}

bool SymbolLibrary::hasSymbolAt(int index) const
{
    const int libIdx = currentLibraryIndex();
    if (libIdx < 0 || libIdx >= m_groups.size()) return false;
    return index >= 0 && index < m_groups[libIdx].symbols.size();
}

const Symbol &SymbolLibrary::symbolAt(int index) const
{
    static const Symbol kEmpty = {QString(), QPixmap()};
    const int libIdx = currentLibraryIndex();
    if (libIdx < 0 || libIdx >= m_groups.size()) return kEmpty;
    const auto &symbols = m_groups[libIdx].symbols;
    if (index < 0 || index >= symbols.size()) return kEmpty;
    return symbols[index];
}

int SymbolLibrary::currentLibraryIndex() const
{
    return m_libraryCombo ? m_libraryCombo->currentIndex() : -1;
}

int SymbolLibrary::findLibraryIndex(const QString &name) const
{
    for (int i = 0; i < m_groups.size(); ++i) {
        if (m_groups[i].name.compare(name, Qt::CaseInsensitive) == 0) return i;
    }
    return -1;
}

int SymbolLibrary::firstCustomLibraryIndex() const
{
    for (int i = 0; i < m_groups.size(); ++i) {
        if (m_groups[i].custom) return i;
    }
    return -1;
}

void SymbolLibrary::addLibraryInternal(const QString &name, bool custom)
{
    if (name.trimmed().isEmpty()) return;
    if (findLibraryIndex(name) >= 0) return;
    m_groups.append({name, custom, {}});
}

void SymbolLibrary::rebuildLibraryCombo()
{
    const QString prev = m_libraryCombo->currentData().toString();
    m_libraryCombo->blockSignals(true);
    m_libraryCombo->clear();
    for (const auto &g : m_groups) {
        const QString label = g.custom ? (g.name + " (Custom)") : g.name;
        m_libraryCombo->addItem(label, g.name);
    }
    int restore = findLibraryIndex(prev);
    if (restore < 0 && !m_groups.isEmpty()) restore = 0;
    if (restore >= 0) m_libraryCombo->setCurrentIndex(restore);
    m_libraryCombo->blockSignals(false);
}

void SymbolLibrary::refreshSymbolList()
{
    m_listWidget->clear();

    const int libIdx = currentLibraryIndex();
    if (libIdx < 0 || libIdx >= m_groups.size()) return;

    for (const auto &s : m_groups[libIdx].symbols) {
        m_listWidget->addItem(new QListWidgetItem(QIcon(s.thumbnail), s.name));
    }
}

void SymbolLibrary::updateButtons()
{
    const int libIdx = currentLibraryIndex();
    const bool valid = libIdx >= 0 && libIdx < m_groups.size();
    const bool custom = valid && m_groups[libIdx].custom;
    m_removeLibraryBtn->setEnabled(custom);
    m_removeSymbolBtn->setEnabled(custom && m_listWidget->currentRow() >= 0);
}

void SymbolLibrary::loadDefaultLibraries()
{
    addLibraryInternal("Digital Electronics", false);
    addLibraryInternal("Digital I/O", false);

    const int digital = findLibraryIndex("Digital Electronics");
    if (digital >= 0) {
        m_groups[digital].symbols.append({"AND",  makeAndGateSymbol(),  {QPointF(0.0, 26.0/72.0), QPointF(0.0, 46.0/72.0), QPointF(84.0/96.0, 36.0/72.0)}});
        m_groups[digital].symbols.append({"OR",   makeOrGateSymbol(),   {QPointF(0.0, 26.0/72.0), QPointF(0.0, 46.0/72.0), QPointF(84.0/96.0, 36.0/72.0)}});
        m_groups[digital].symbols.append({"NOT",  makeNotGateSymbol(),  {QPointF(8.0/96.0, 36.0/72.0), QPointF(86.0/96.0, 36.0/72.0)}});
        m_groups[digital].symbols.append({"NAND", makeNandGateSymbol(), {QPointF(0.0, 26.0/72.0), QPointF(0.0, 46.0/72.0), QPointF(84.0/96.0, 36.0/72.0)}});
        m_groups[digital].symbols.append({"NOR",  makeNorGateSymbol(),  {QPointF(0.0, 26.0/72.0), QPointF(0.0, 46.0/72.0), QPointF(86.0/96.0, 36.0/72.0)}});
        m_groups[digital].symbols.append({"XOR",  makeXorGateSymbol(),  {QPointF(0.0, 26.0/72.0), QPointF(0.0, 46.0/72.0), QPointF(84.0/96.0, 36.0/72.0)}});
    }

    const int io = findLibraryIndex("Digital I/O");
    if (io >= 0) {
        m_groups[io].symbols.append({"Input",  makeInputPinSymbol(),  {QPointF(8.0/96.0, 36.0/72.0), QPointF(88.0/96.0, 36.0/72.0)}});
        m_groups[io].symbols.append({"Output", makeOutputPinSymbol(), {QPointF(8.0/96.0, 36.0/72.0), QPointF(88.0/96.0, 36.0/72.0)}});
        m_groups[io].symbols.append({"Clock",  makeClockSymbol(),     {QPointF(8.0/96.0, 36.0/72.0), QPointF(88.0/96.0, 36.0/72.0)}});
        m_groups[io].symbols.append({"GND",    makeGroundSymbol(),    {QPointF(48.0/96.0, 12.0/72.0)}});
        m_groups[io].symbols.append({"VCC",    makeVccSymbol(),       {QPointF(48.0/96.0, 58.0/72.0)}});
    }
}

void SymbolLibrary::loadCustomLibraries()
{
    QFile f(customLibrariesFilePath());
    if (!f.exists()) return;
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return;

    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    f.close();
    if (!doc.isObject()) return;

    const QJsonObject root = doc.object();
    const QJsonArray libs = root.value("customLibraries").toArray();
    for (const auto &lv : libs) {
        const QJsonObject lo = lv.toObject();
        const QString libName = lo.value("name").toString().trimmed();
        if (libName.isEmpty()) continue;

        addLibraryInternal(libName, true);
        const int idx = findLibraryIndex(libName);
        if (idx < 0) continue;

        const QJsonArray symbols = lo.value("symbols").toArray();
        for (const auto &sv : symbols) {
            const QJsonObject so = sv.toObject();
            const QString n = so.value("name").toString();
            const QString b64 = so.value("png").toString();
            const QPixmap px = decodePixmapBase64(b64);
            if (px.isNull()) continue;
            QList<QPointF> ports;
            const QJsonArray pa = so.value("ports").toArray();
            for (const auto &pv : pa) {
                const QJsonObject po = pv.toObject();
                ports.append(QPointF(po.value("x").toDouble(), po.value("y").toDouble()));
            }
            m_groups[idx].symbols.append({n, px, ports});
        }
    }
}

void SymbolLibrary::saveCustomLibraries() const
{
    QJsonArray libs;
    for (const auto &g : m_groups) {
        if (!g.custom) continue;

        QJsonObject lo;
        lo["name"] = g.name;

        QJsonArray symbols;
        for (const auto &s : g.symbols) {
            QJsonObject so;
            so["name"] = s.name;
            so["png"] = encodePixmapBase64(s.thumbnail);
            QJsonArray pa;
            for (const QPointF &pt : s.portsNormalized) {
                QJsonObject po;
                po["x"] = pt.x();
                po["y"] = pt.y();
                pa.append(po);
            }
            so["ports"] = pa;
            symbols.append(so);
        }
        lo["symbols"] = symbols;
        libs.append(lo);
    }

    QJsonObject root;
    root["version"] = 1;
    root["customLibraries"] = libs;

    QFile f(customLibrariesFilePath());
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) return;
    f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    f.close();
}

QString SymbolLibrary::promptForCustomLibrary()
{
    QStringList customNames;
    for (const auto &g : m_groups) {
        if (g.custom) customNames << g.name;
    }
    customNames.sort(Qt::CaseInsensitive);
    customNames.prepend("<Create New Library...>");

    bool ok = false;
    const QString chosen = QInputDialog::getItem(this,
                                                  "Choose Custom Library",
                                                  "Save symbol to:",
                                                  customNames,
                                                  0,
                                                  false,
                                                  &ok);
    if (!ok || chosen.isEmpty()) return QString();

    if (chosen != "<Create New Library...>") {
        return chosen;
    }

    const QString name = QInputDialog::getText(this,
                                               "New Custom Library",
                                               "Library name:",
                                               QLineEdit::Normal,
                                               "My Symbols",
                                               &ok).trimmed();
    if (!ok || name.isEmpty()) return QString();
    if (findLibraryIndex(name) >= 0) return name;

    addLibraryInternal(name, true);
    rebuildLibraryCombo();
    saveCustomLibraries();
    return name;
}

QPixmap SymbolLibrary::makeAndGateSymbol() const
{
    QPixmap px = makeHiDpiSymbolCanvas();
    QPainter p(&px);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.scale(2.0, 2.0);
    p.setPen(QPen(Qt::black, 2));
    p.setBrush(Qt::NoBrush);

    p.drawLine(8, 18, 8, 54);
    p.drawLine(8, 18, 40, 18);
    p.drawLine(8, 54, 40, 54);
    p.drawArc(QRectF(24, 18, 32, 36), -90 * 16, 180 * 16);
    p.drawLine(56, 36, 84, 36);
    p.drawLine(0, 26, 8, 26);
    p.drawLine(0, 46, 8, 46);
    drawPortMarkers(p, {QPointF(0, 26), QPointF(0, 46), QPointF(84, 36)});
    return px;
}

QPixmap SymbolLibrary::makeOrGateSymbol() const
{
    QPixmap px = makeHiDpiSymbolCanvas();
    QPainter p(&px);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.scale(2.0, 2.0);
    p.setPen(QPen(Qt::black, 2));
    p.setBrush(Qt::NoBrush);

    QPainterPath path;
    path.moveTo(10, 18);
    path.quadTo(44, 18, 60, 36);
    path.quadTo(44, 54, 10, 54);
    path.quadTo(24, 36, 10, 18);
    p.drawPath(path);
    p.drawLine(0, 26, 10, 26);
    p.drawLine(0, 46, 10, 46);
    p.drawLine(60, 36, 84, 36);
    drawPortMarkers(p, {QPointF(0, 26), QPointF(0, 46), QPointF(84, 36)});
    return px;
}

QPixmap SymbolLibrary::makeNotGateSymbol() const
{
    QPixmap px = makeHiDpiSymbolCanvas();
    QPainter p(&px);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.scale(2.0, 2.0);
    p.setPen(QPen(Qt::black, 2));
    p.setBrush(Qt::NoBrush);

    p.drawLine(8, 36, 20, 36);
    QPolygonF tri;
    tri << QPointF(20, 18) << QPointF(20, 54) << QPointF(56, 36);
    p.drawPolygon(tri);
    p.drawEllipse(QRectF(56, 31, 10, 10));
    p.drawLine(66, 36, 86, 36);
    drawPortMarkers(p, {QPointF(8, 36), QPointF(86, 36)});
    return px;
}

QPixmap SymbolLibrary::makeNandGateSymbol() const
{
    QPixmap px = makeAndGateSymbol();
    QPainter p(&px);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.scale(2.0, 2.0);
    p.setPen(QPen(Qt::black, 2));
    p.setBrush(Qt::white);
    p.drawEllipse(QRectF(56, 31, 10, 10));
    p.drawLine(66, 36, 84, 36);
    drawPortMarkers(p, {QPointF(0, 26), QPointF(0, 46), QPointF(84, 36)});
    return px;
}

QPixmap SymbolLibrary::makeNorGateSymbol() const
{
    QPixmap px = makeOrGateSymbol();
    QPainter p(&px);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.scale(2.0, 2.0);
    p.setPen(QPen(Qt::black, 2));
    p.setBrush(Qt::white);
    p.drawEllipse(QRectF(60, 31, 10, 10));
    p.drawLine(70, 36, 86, 36);
    drawPortMarkers(p, {QPointF(0, 26), QPointF(0, 46), QPointF(86, 36)});
    return px;
}

QPixmap SymbolLibrary::makeXorGateSymbol() const
{
    QPixmap px = makeOrGateSymbol();
    QPainter p(&px);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.scale(2.0, 2.0);
    p.setPen(QPen(Qt::black, 2));
    QPainterPath extra;
    extra.moveTo(6, 18);
    extra.quadTo(20, 36, 6, 54);
    p.drawPath(extra);
    drawPortMarkers(p, {QPointF(0, 26), QPointF(0, 46), QPointF(84, 36)});
    return px;
}

QPixmap SymbolLibrary::makeInputPinSymbol() const
{
    QPixmap px = makeHiDpiSymbolCanvas();
    QPainter p(&px);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.scale(2.0, 2.0);
    p.setPen(QPen(Qt::black, 2));
    p.setBrush(Qt::NoBrush);
    p.drawLine(8, 36, 28, 36);
    p.drawRect(QRectF(28, 24, 28, 24));
    p.drawLine(56, 36, 88, 36);
    drawPortMarkers(p, {QPointF(8, 36), QPointF(88, 36)});
    return px;
}

QPixmap SymbolLibrary::makeOutputPinSymbol() const
{
    QPixmap px = makeHiDpiSymbolCanvas();
    QPainter p(&px);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.scale(2.0, 2.0);
    p.setPen(QPen(Qt::black, 2));
    p.setBrush(Qt::NoBrush);
    p.drawLine(8, 36, 40, 36);
    p.drawRect(QRectF(40, 24, 28, 24));
    p.drawLine(68, 36, 88, 36);
    drawPortMarkers(p, {QPointF(8, 36), QPointF(88, 36)});
    return px;
}

QPixmap SymbolLibrary::makeClockSymbol() const
{
    QPixmap px = makeHiDpiSymbolCanvas();
    QPainter p(&px);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.scale(2.0, 2.0);
    p.setPen(QPen(Qt::black, 2));
    p.setBrush(Qt::NoBrush);
    p.drawRect(QRectF(24, 18, 40, 36));
    QPolygonF tri;
    tri << QPointF(35, 28) << QPointF(35, 44) << QPointF(49, 36);
    p.drawPolygon(tri);
    p.drawLine(8, 36, 24, 36);
    p.drawLine(64, 36, 88, 36);
    drawPortMarkers(p, {QPointF(8, 36), QPointF(88, 36)});
    return px;
}

QPixmap SymbolLibrary::makeGroundSymbol() const
{
    QPixmap px = makeHiDpiSymbolCanvas();
    QPainter p(&px);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.scale(2.0, 2.0);
    p.setPen(QPen(Qt::black, 2));
    p.drawLine(48, 12, 48, 28);
    p.drawLine(34, 28, 62, 28);
    p.drawLine(38, 34, 58, 34);
    p.drawLine(42, 40, 54, 40);
    drawPortMarkers(p, {QPointF(48, 12)});
    return px;
}

QPixmap SymbolLibrary::makeVccSymbol() const
{
    QPixmap px = makeHiDpiSymbolCanvas();
    QPainter p(&px);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.scale(2.0, 2.0);
    p.setPen(QPen(Qt::black, 2));
    p.drawLine(48, 58, 48, 30);
    QPolygonF tri;
    tri << QPointF(48, 14) << QPointF(40, 30) << QPointF(56, 30);
    p.drawPolygon(tri);
    drawPortMarkers(p, {QPointF(48, 58)});
    return px;
}