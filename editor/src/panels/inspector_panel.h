#pragma once
#include "ecs/entity.h"

#include <QSet>
#include <QString>
#include <QWidget>
#include <functional>
#include <vector>

class EditorContext;
class QFormLayout;
class QScrollArea;
class QVBoxLayout;
struct FieldDesc;

// Composants de l'entité sélectionnée, avec des champs éditables.
// Les champs viennent de componentDescriptors() (component_descriptors.cpp).
class InspectorPanel : public QWidget {
    Q_OBJECT
public:
    explicit InspectorPanel(EditorContext& context, QWidget* parent = nullptr);

private:
    void rebuild();
    void refreshValues();
    QWidget* buildHeader(Entity e);
    QWidget* buildSection(const QString& title, bool removable, const std::function<void()>& onRemove, QFormLayout*& form);
    QWidget* buildAddButton(Entity e);
    QWidget* createEditor(const FieldDesc& field, Entity e);
    void edited(Entity e);

    EditorContext& m_ctx;
    QScrollArea* m_scroll = nullptr;
    std::vector<std::function<void()>> m_refreshers;   // relisent les valeurs dans les champs
    QSet<QString> m_collapsed;                         // sections repliées (par nom de composant)
};
