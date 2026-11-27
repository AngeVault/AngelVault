#include "component_library_panel.h"
#include "component_descriptors.h"
#include "component_visuals.h"
#include "editor_context.h"

#include "ecs/behavior_factory.h"

#include <QAbstractButton>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFile>
#include <QFrame>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QListWidget>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPointer>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QStyledItemDelegate>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <cstring>
#include <functional>

namespace {
enum Role { KeyRole = Qt::UserRole, IconRole, PresentRole, NewKindRole };

const QColor kFolderColor(0xe0, 0xa8, 0x3e);

QColor alpha(QColor c, int a) { c.setAlpha(a); return c; }

QString mimeKey(const QMimeData* mime) {
    return mime && mime->hasFormat(kComponentMimeType) ? QString::fromUtf8(mime->data(kComponentMimeType)) : QString();
}

// trait de séparation aux couleurs du thème
class Rule : public QWidget {
public:
    Rule(int w, int h) { setFixedSize(w, h); }
    Rule() { setFixedHeight(1); }
protected:
    void paintEvent(QPaintEvent*) override { QPainter(this).fillRect(rect(), alpha(palette().color(QPalette::WindowText), 35)); }
};

// bouton de la barre : catégorie (icône seule), favoris, dossier (icône + nom) ou nouveau dossier
class LibraryChip : public QAbstractButton {
public:
    LibraryChip(const ComponentLibraryPanel* panel, const QString& glyph, const QString& label, QColor glyphColor = {})
        : m_panel(panel), m_glyph(glyph), m_label(label), m_glyphColor(glyphColor) {
        setCheckable(true);
        setCursor(Qt::PointingHandCursor);
        setAttribute(Qt::WA_Hover);
    }

    std::function<void()> onRemove;
    std::function<void()> onRename;
    std::function<void(const QString&)> onDropKey;

    QSize sizeHint() const override {
        if (m_label.isEmpty()) return { 44, 44 };
        return { std::clamp(fontMetrics().horizontalAdvance(m_label) + 24, 60, 140), 44 };
    }

    void setDropTarget(bool on) { setAcceptDrops(on); }

protected:
    QRectF closeRect() const { return QRectF(width() - 15, 3, 12, 12); }

    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const QColor text = palette().color(QPalette::WindowText);
        const QColor accent = m_panel->accentColor();
        const QRectF r = QRectF(rect()).adjusted(1, 1, -1, -1);
        if (m_dropHover) { p.setPen(QPen(accent, 1.2)); p.setBrush(alpha(accent, 70)); }
        else if (isChecked()) { p.setPen(QPen(accent, 1.2)); p.setBrush(alpha(accent, 38)); }
        else if (underMouse()) { p.setPen(Qt::NoPen); p.setBrush(alpha(text, 18)); }
        else { p.setPen(Qt::NoPen); p.setBrush(Qt::NoBrush); }
        p.drawRoundedRect(r, 7, 7);

        const QColor glyph = m_glyphColor.isValid() ? m_glyphColor : isChecked() ? text : alpha(text, 150);
        if (m_label.isEmpty()) {
            drawGlyph(p, m_glyph, QRectF(width() / 2.0 - 9, height() / 2.0 - 9, 18, 18), glyph);
        } else {
            drawGlyph(p, m_glyph, QRectF(width() / 2.0 - 8, 6, 16, 16), glyph);
            QFont f = font();
            f.setPointSizeF(f.pointSizeF() * 0.85);
            if (isChecked()) f.setBold(true);
            p.setFont(f);
            p.setPen(isChecked() ? text : alpha(text, 190));
            const QRectF tr(4, 24, width() - 8, height() - 26);
            p.drawText(tr, Qt::AlignHCenter | Qt::AlignTop, QFontMetrics(f).elidedText(m_label, Qt::ElideRight, int(tr.width())));
        }
        if (onRemove && underMouse()) {
            p.setPen(Qt::NoPen);
            p.setBrush(alpha(text, 40));
            p.drawEllipse(closeRect());
            drawGlyph(p, "close", closeRect().adjusted(2, 2, -2, -2), text);
        }
    }

    void mousePressEvent(QMouseEvent* e) override {
        if (onRemove && e->button() == Qt::LeftButton && closeRect().contains(e->position())) {
            QTimer::singleShot(0, this, onRemove);
            return;
        }
        QAbstractButton::mousePressEvent(e);
    }

    void mouseDoubleClickEvent(QMouseEvent* e) override {
        if (onRename && e->button() == Qt::LeftButton) QTimer::singleShot(0, this, onRename);
    }

    void dragEnterEvent(QDragEnterEvent* e) override {
        if (!onDropKey || mimeKey(e->mimeData()).isEmpty()) return;
        e->acceptProposedAction();
        m_dropHover = true;
        update();
    }

    void dragLeaveEvent(QDragLeaveEvent*) override { m_dropHover = false; update(); }

    void dropEvent(QDropEvent* e) override {
        m_dropHover = false;
        update();
        const QString key = mimeKey(e->mimeData());
        if (key.isEmpty() || !onDropKey) return;
        e->acceptProposedAction();
        onDropKey(key);
    }

private:
    const ComponentLibraryPanel* m_panel;
    QString m_glyph, m_label;
    QColor m_glyphColor;
    bool m_dropHover = false;
};

// les cartes se glissent vers l'Inspector ou un dossier : la donnée est la clé du composant
class CardList : public QListWidget {
public:
    using QListWidget::QListWidget;

protected:
    // dossier ou favoris vides : grand glyphe estompé à la place du texte
    void paintEvent(QPaintEvent* e) override {
        QListWidget::paintEvent(e);
        const QString glyph = property("emptyGlyph").toString();
        if (count() > 0 || glyph.isEmpty()) return;
        QPainter p(viewport());
        const QRectF r(viewport()->width() / 2.0 - 28, viewport()->height() / 2.0 - 28, 56, 56);
        drawGlyph(p, glyph, r, alpha(palette().color(QPalette::Text), 45));
    }
    QStringList mimeTypes() const override { return { kComponentMimeType }; }
    QMimeData* mimeData(const QList<QListWidgetItem*>& items) const override {
        if (items.isEmpty() || items.first()->data(KeyRole).toString().isEmpty()) return nullptr;
        auto* mime = new QMimeData;
        mime->setData(kComponentMimeType, items.first()->data(KeyRole).toString().toUtf8());
        return mime;
    }
};

class CardDelegate : public QStyledItemDelegate {
public:
    CardDelegate(ComponentLibraryPanel* panel) : QStyledItemDelegate(panel), m_panel(panel) {}

    std::function<void(const QString&)> onStar, onRemove;
    std::function<void(int)> onCreate;

    static QRectF cardRect(const QStyleOptionViewItem& opt) { return QRectF(opt.rect).adjusted(3, 3, -3, -3); }
    static QRectF starRect(const QRectF& r) { return QRectF(r.right() - 22, r.top() + 6, 16, 16); }
    static QRectF closeRect(const QRectF& r) { return QRectF(r.right() - 42, r.top() + 6, 16, 16); }

    QSize sizeHint(const QStyleOptionViewItem&, const QModelIndex&) const override { return { 180, 62 }; }

    void paint(QPainter* p, const QStyleOptionViewItem& opt, const QModelIndex& index) const override {
        p->save();
        p->setRenderHint(QPainter::Antialiasing);
        const QRectF r = cardRect(opt);
        const QColor text = opt.palette.color(QPalette::Text);
        const QColor accent = m_panel->accentColor();
        const bool hover = opt.state & QStyle::State_MouseOver;
        const bool selected = opt.state & QStyle::State_Selected;

        const int newKind = index.data(NewKindRole).toInt();
        if (newKind >= 0) {
            p->setPen(QPen(hover ? accent : alpha(text, 70), 1.2, Qt::DashLine));
            p->setBrush(hover ? alpha(accent, 20) : Qt::NoBrush);
            p->drawRoundedRect(r, 8, 8);
            drawGlyph(*p, "plus", QRectF(r.center().x() - 10, r.center().y() - 10, 20, 20), hover ? accent : alpha(text, 120));
            p->restore();
            return;
        }

        const QString key = index.data(KeyRole).toString();
        const bool present = index.data(PresentRole).toBool();
        p->setPen(QPen(selected || hover ? accent : alpha(text, 30), selected ? 1.6 : 1.0));
        p->setBrush(selected ? alpha(accent, 40) : alpha(text, hover ? 22 : 12));
        p->drawRoundedRect(r, 8, 8);

        if (present) p->setOpacity(0.45);
        const QRectF box(r.left() + 10, r.center().y() - 16, 32, 32);
        p->setPen(Qt::NoPen);
        p->setBrush(alpha(accent, 55));
        p->drawRoundedRect(box, 7, 7);
        drawGlyph(*p, index.data(IconRole).toString(), box.adjusted(7, 7, -7, -7), text);

        QFont f = opt.font;
        f.setBold(true);
        p->setFont(f);
        p->setPen(text);
        const QRectF name(box.right() + 10, r.top(), r.right() - box.right() - 34, r.height());
        p->drawText(name, Qt::AlignVCenter | Qt::AlignLeft,
                    QFontMetrics(f).elidedText(index.data(Qt::DisplayRole).toString(), Qt::ElideRight, int(name.width())));
        p->setOpacity(1.0);

        if (m_panel->isFavorite(key)) drawGlyph(*p, "star_filled", starRect(r), kFolderColor);
        else if (hover) drawGlyph(*p, "star", starRect(r), alpha(text, 130));
        if (hover && m_panel->inFolderView()) drawGlyph(*p, "close", closeRect(r).adjusted(2, 2, -2, -2), alpha(text, 160));
        if (present) drawGlyph(*p, "check", QRectF(r.right() - 21, r.bottom() - 21, 14, 14), accent);
        p->restore();
    }

    bool editorEvent(QEvent* event, QAbstractItemModel* model, const QStyleOptionViewItem& opt, const QModelIndex& index) override {
        if (event->type() != QEvent::MouseButtonRelease) return QStyledItemDelegate::editorEvent(event, model, opt, index);
        auto* me = static_cast<QMouseEvent*>(event);
        if (me->button() != Qt::LeftButton) return false;
        const int newKind = index.data(NewKindRole).toInt();
        if (newKind >= 0) { onCreate(newKind); return true; }
        const QRectF r = cardRect(opt);
        const QString key = index.data(KeyRole).toString();
        if (starRect(r).contains(me->position())) { onStar(key); return true; }
        if (m_panel->inFolderView() && closeRect(r).contains(me->position())) { onRemove(key); return true; }
        return false;
    }

private:
    ComponentLibraryPanel* m_panel;
};
}

ComponentLibraryPanel::ComponentLibraryPanel(EditorContext& context, QWidget* parent) : QWidget(parent), m_ctx(context) {
    setObjectName("componentLibraryPanel");

    for (const ComponentDesc& d : componentDescriptors())
        if (d.add) m_entries.push_back({ "component:" + d.name, d.name, d.category, d.icon });
    for (const std::string& name : BehaviorFactory::get().names()) {
        const QString n = QString::fromStdString(name);
        m_entries.push_back({ "behavior:" + n, n, kBehaviorCategory, "code" });
    }

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 6, 8, 6);
    layout->setSpacing(6);

    auto* top = new QHBoxLayout;
    top->setSpacing(8);
    auto* chipsHost = new QWidget;
    m_chips = new QHBoxLayout(chipsHost);
    m_chips->setContentsMargins(0, 0, 0, 0);
    m_chips->setSpacing(4);
    auto* chipsScroll = new QScrollArea;
    chipsScroll->setWidget(chipsHost);
    chipsScroll->setWidgetResizable(true);
    chipsScroll->setFrameShape(QFrame::NoFrame);
    chipsScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    chipsScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    chipsScroll->setFixedHeight(52);
    chipsScroll->setStyleSheet("QScrollArea, QScrollArea > QWidget > QWidget { background: transparent; }");
    top->addWidget(chipsScroll, 1);

    m_search = new QLineEdit;
    m_search->setObjectName("componentSearch");
    m_search->setClearButtonEnabled(true);
    m_search->addAction(glyphIcon("search", QColor(140, 140, 140)), QLineEdit::LeadingPosition);
    m_search->setFixedWidth(200);
    m_search->setToolTip(tr("chercher"));
    top->addWidget(m_search, 0, Qt::AlignVCenter);
    layout->addLayout(top);

    layout->addWidget(new Rule);

    m_cards = new CardList;
    m_cards->setObjectName("componentCards");
    m_cards->setStyleSheet("QListWidget#componentCards { background: transparent; border: none; }");
    m_cards->setViewMode(QListView::IconMode);
    m_cards->setMovement(QListView::Static);
    m_cards->setResizeMode(QListView::Adjust);
    m_cards->setGridSize(QSize(186, 68));
    m_cards->setUniformItemSizes(true);
    m_cards->setWrapping(true);
    m_cards->setDragEnabled(true);
    m_cards->setDragDropMode(QAbstractItemView::DragOnly);
    m_cards->setDefaultDropAction(Qt::CopyAction);
    m_cards->setSelectionMode(QAbstractItemView::SingleSelection);
    m_cards->setMouseTracking(true);
    m_cards->viewport()->setAttribute(Qt::WA_Hover);
    auto* delegate = new CardDelegate(this);
    delegate->onStar = [this](const QString& key) { QTimer::singleShot(0, this, [this, key] { toggleFavorite(key); }); };
    delegate->onRemove = [this](const QString& key) { QTimer::singleShot(0, this, [this, key] { removeFromView(key); }); };
    delegate->onCreate = [this](int kind) { QTimer::singleShot(0, this, [this, kind] { emit createCodeRequested(kind); }); };
    m_cards->setItemDelegate(delegate);
    layout->addWidget(m_cards, 1);

    connect(m_cards, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* item) {
        const QString key = item->data(KeyRole).toString();
        if (!key.isEmpty() && m_ctx.hasSelection()) m_ctx.addComponent(m_ctx.selected(), key);
    });
    connect(m_search, &QLineEdit::textChanged, this, &ComponentLibraryPanel::rebuildCards);

    connect(&m_ctx, &EditorContext::selectionChanged, this, &ComponentLibraryPanel::refreshPresence);
    connect(&m_ctx, &EditorContext::componentsChanged, this, &ComponentLibraryPanel::refreshPresence);
    connect(&m_ctx, &EditorContext::entityModified, this, &ComponentLibraryPanel::refreshPresence);
    connect(&m_ctx, &EditorContext::sceneChanged, this, &ComponentLibraryPanel::refreshPresence);
    connect(&m_ctx, &EditorContext::documentChanged, this, [this] {
        const QString file = m_ctx.hasProject() ? m_ctx.projectDir() + "/editor.json" : QString();
        if (file != m_file) load();
    });
    load();
}

void ComponentLibraryPanel::setAccentColor(const QColor& c) {
    m_accent = c;
    for (QWidget* w : findChildren<QAbstractButton*>()) w->update();
    m_cards->viewport()->update();
}

void ComponentLibraryPanel::load() {
    m_file = m_ctx.hasProject() ? m_ctx.projectDir() + "/editor.json" : QString();
    m_favorites.clear();
    m_folders.clear();
    QFile file(m_file);
    if (!m_file.isEmpty() && file.open(QIODevice::ReadOnly)) {
        const QJsonObject lib = QJsonDocument::fromJson(file.readAll()).object().value("componentLibrary").toObject();
        for (const QJsonValue& v : lib.value("favorites").toArray()) m_favorites << v.toString();
        for (const QJsonValue& v : lib.value("folders").toArray()) {
            const QJsonObject o = v.toObject();
            Folder f{ o.value("name").toString() };
            for (const QJsonValue& item : o.value("items").toArray()) f.items << item.toString();
            if (!f.name.isEmpty() && folderIndex(f.name) < 0) m_folders.push_back(f);
        }
    }
    if (m_view.startsWith(kFolderPrefix) && folderIndex(m_view.mid(int(strlen(kFolderPrefix)))) < 0) m_view = "base";
    rebuildChips();
    rebuildCards();
}

// le fichier peut contenir d'autres réglages de l'éditeur : on ne remplace que la bibliothèque
void ComponentLibraryPanel::save() {
    if (m_file.isEmpty()) return;
    QJsonObject root;
    QFile file(m_file);
    if (file.open(QIODevice::ReadOnly)) {
        root = QJsonDocument::fromJson(file.readAll()).object();
        file.close();
    }
    QJsonArray folders;
    for (const Folder& f : m_folders)
        folders.append(QJsonObject{ { "name", f.name }, { "items", QJsonArray::fromStringList(f.items) } });
    root["componentLibrary"] = QJsonObject{ { "favorites", QJsonArray::fromStringList(m_favorites) }, { "folders", folders } };
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
}

void ComponentLibraryPanel::rebuildChips() {
    while (QLayoutItem* item = m_chips->takeAt(0)) {
        if (QWidget* w = item->widget()) { w->hide(); w->deleteLater(); }
        delete item;
    }
    m_folderChips.clear();

    auto addChip = [this](LibraryChip* chip, const QString& view, const QString& tooltip) {
        chip->setToolTip(tooltip);
        chip->setProperty("view", view);
        chip->setChecked(m_view == view);
        connect(chip, &QAbstractButton::clicked, this, [this, view] { setView(view); });
        m_chips->addWidget(chip);
        return chip;
    };

    for (const ComponentCategory& c : componentCategories())
        addChip(new LibraryChip(this, c.icon, {}), c.id, c.tooltip);

    m_chips->addSpacing(4);
    m_chips->addWidget(new Rule(1, 28), 0, Qt::AlignVCenter);
    m_chips->addSpacing(4);

    auto* fav = addChip(new LibraryChip(this, "star", {}, kFolderColor), kFavorites, tr("favoris"));
    fav->setDropTarget(true);
    fav->onDropKey = [this](const QString& key) {
        if (!m_favorites.contains(key)) { m_favorites << key; save(); m_cards->viewport()->update(); }
        if (m_view == kFavorites) rebuildCards();
    };

    for (int i = 0; i < int(m_folders.size()); ++i) {
        auto* chip = addChip(new LibraryChip(this, "folder", m_folders[i].name, kFolderColor), kFolderPrefix + m_folders[i].name, m_folders[i].name);
        chip->setDropTarget(true);
        chip->onDropKey = [this, i](const QString& key) { addToFolder(i, key); };
        chip->onRename = [this, i] { renameFolder(i); };
        chip->onRemove = [this, i] { removeFolder(i); };
        m_folderChips << chip;
    }

    auto* add = new LibraryChip(this, "folder_plus", {});
    add->setCheckable(false);
    add->setToolTip(tr("nouveau dossier"));
    connect(add, &QAbstractButton::clicked, this, [this] { QTimer::singleShot(0, this, &ComponentLibraryPanel::addFolder); });
    m_chips->addWidget(add);
    m_chips->addStretch();
}

void ComponentLibraryPanel::rebuildCards() {
    m_cards->clear();
    const QString search = m_search->text().trimmed();
    auto entry = [this](const QString& key) -> const Entry* {
        for (const Entry& e : m_entries) if (e.key == key) return &e;
        return nullptr;
    };

    std::vector<const Entry*> shown;
    if (!search.isEmpty()) {
        for (const Entry& e : m_entries) if (e.name.contains(search, Qt::CaseInsensitive)) shown.push_back(&e);
    } else if (m_view == kFavorites) {
        for (const QString& key : m_favorites) if (const Entry* e = entry(key)) shown.push_back(e);
    } else if (m_view.startsWith(kFolderPrefix)) {
        const int i = folderIndex(m_view.mid(int(strlen(kFolderPrefix))));
        if (i >= 0) for (const QString& key : m_folders[i].items) if (const Entry* e = entry(key)) shown.push_back(e);
    } else {
        for (const Entry& e : m_entries) if (e.category == m_view) shown.push_back(&e);
    }

    for (const Entry* e : shown) {
        auto* item = new QListWidgetItem(e->name, m_cards);
        item->setData(KeyRole, e->key);
        item->setData(IconRole, e->icon);
        item->setData(NewKindRole, -1);
    }
    m_cards->setProperty("emptyGlyph", !search.isEmpty() ? "search" : m_view == kFavorites ? "star" : m_view.startsWith(kFolderPrefix) ? "folder" : "");
    // les comportements et composants du jeu se créent en C++ dans le projet
    if (search.isEmpty() && (m_view == kBehaviorCategory || m_view == kGameCategory)) {
        auto* item = new QListWidgetItem(m_cards);
        item->setData(NewKindRole, m_view == kBehaviorCategory ? 0 : 1);
        item->setFlags(Qt::ItemIsEnabled);
        item->setToolTip(m_view == kBehaviorCategory ? tr("nouveau comportement") : tr("nouveau composant"));
    }
    refreshPresence();
}

void ComponentLibraryPanel::refreshPresence() {
    const Entity e = m_ctx.selected();
    for (int i = 0; i < m_cards->count(); ++i) {
        QListWidgetItem* item = m_cards->item(i);
        const QString key = item->data(KeyRole).toString();
        if (!key.isEmpty()) item->setData(PresentRole, m_ctx.hasSelection() && m_ctx.hasComponent(e, key));
    }
}

void ComponentLibraryPanel::setView(const QString& view) {
    m_view = view;
    for (int i = 0; i < m_chips->count(); ++i)
        if (auto* chip = qobject_cast<QAbstractButton*>(m_chips->itemAt(i)->widget()); chip && chip->isCheckable())
            chip->setChecked(chip->property("view").toString() == m_view);
    QSignalBlocker block(m_search);
    m_search->clear();
    rebuildCards();
}

void ComponentLibraryPanel::toggleFavorite(const QString& key) {
    if (!m_favorites.removeOne(key)) m_favorites << key;
    save();
    if (m_view == kFavorites) rebuildCards();
    else m_cards->viewport()->update();
}

void ComponentLibraryPanel::removeFromView(const QString& key) {
    const int i = folderIndex(m_view.mid(int(strlen(kFolderPrefix))));
    if (i < 0) return;
    m_folders[i].items.removeAll(key);
    save();
    rebuildCards();
}

void ComponentLibraryPanel::addToFolder(int folder, const QString& key) {
    if (folder < 0 || folder >= int(m_folders.size()) || m_folders[folder].items.contains(key)) return;
    m_folders[folder].items << key;
    save();
    if (m_view == kFolderPrefix + m_folders[folder].name) rebuildCards();
}

void ComponentLibraryPanel::addFolder() {
    m_folders.push_back({ uniqueFolderName(tr("dossier")) });
    save();
    m_view = kFolderPrefix + m_folders.back().name;
    rebuildChips();
    rebuildCards();
    QTimer::singleShot(0, this, [this] { renameFolder(int(m_folders.size()) - 1); });
}

// renommage sur place : un champ par-dessus le dossier
void ComponentLibraryPanel::renameFolder(int folder) {
    if (folder < 0 || folder >= m_folderChips.size()) return;
    QWidget* chip = m_folderChips[folder];
    auto* edit = new QLineEdit(m_folders[folder].name, chip);
    const QSize size = chip->sizeHint();
    edit->setGeometry(0, 8, size.width(), size.height() - 16);
    edit->setAlignment(Qt::AlignCenter);
    edit->selectAll();
    edit->show();
    edit->setFocus();
    QPointer<QLineEdit> guard(edit);
    connect(edit, &QLineEdit::editingFinished, this, [this, folder, guard] {
        if (!guard || guard->property("done").toBool()) return;
        guard->setProperty("done", true);
        const QString name = guard->text().trimmed();
        guard->deleteLater();
        if (folder >= int(m_folders.size())) return;
        Folder& f = m_folders[folder];
        if (!name.isEmpty() && name != f.name) {
            const bool current = m_view == kFolderPrefix + f.name;
            f.name = uniqueFolderName(name);
            if (current) m_view = kFolderPrefix + f.name;
            save();
        }
        QTimer::singleShot(0, this, [this] { rebuildChips(); });
    });
}

void ComponentLibraryPanel::removeFolder(int folder) {
    if (folder < 0 || folder >= int(m_folders.size())) return;
    if (m_view == kFolderPrefix + m_folders[folder].name) m_view = kFavorites;
    m_folders.erase(m_folders.begin() + folder);
    save();
    rebuildChips();
    setView(m_view);
}

int ComponentLibraryPanel::folderIndex(const QString& name) const {
    for (int i = 0; i < int(m_folders.size()); ++i) if (m_folders[i].name == name) return i;
    return -1;
}

QString ComponentLibraryPanel::uniqueFolderName(const QString& base) const {
    QString name = base;
    for (int n = 2; folderIndex(name) >= 0; ++n) name = QString("%1 %2").arg(base).arg(n);
    return name;
}
