#include "tileset_import.h"

#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QMap>
#include <QPainter>
#include <QRegularExpression>

#include <algorithm>
#include <array>
#include <vector>

namespace {
constexpr int kMaxSheetSide = 8192;   // au-delà, certaines cartes graphiques refusent la texture

struct Source {
    QString path;
    QImage image;
};

// Ordre « naturel » : « Tile 2 » avant « Tile 10 » (QCollator ne le fait pas sur toutes les plateformes)
bool naturalLess(const QString& a, const QString& b) {
    int i = 0, j = 0;
    while (i < a.size() && j < b.size()) {
        if (a[i].isDigit() && b[j].isDigit()) {
            int i2 = i, j2 = j;
            while (i2 < a.size() && a[i2].isDigit()) ++i2;
            while (j2 < b.size() && b[j2].isDigit()) ++j2;
            const qulonglong na = a.mid(i, i2 - i).toULongLong(), nb = b.mid(j, j2 - j).toULongLong();
            if (na != nb) return na < nb;
            i = i2;
            j = j2;
        } else {
            const QChar ca = a[i].toLower(), cb = b[j].toLower();
            if (ca != cb) return ca < cb;
            ++i;
            ++j;
        }
    }
    return a.size() - i < b.size() - j;
}
} // namespace

PackedSheet packTilesetImages(const QStringList& files, const QString& outPng, bool groupRotations, bool trim) {
    PackedSheet r;
    if (files.isEmpty()) { r.error = QObject::tr("Aucune image."); return r; }

    // Ordre naturel : « Tile 2 » avant « Tile 10 »
    QStringList sorted = files;
    std::sort(sorted.begin(), sorted.end(), [](const QString& a, const QString& b) {
        return naturalLess(QFileInfo(a).completeBaseName(), QFileInfo(b).completeBaseName());
    });

    std::vector<Source> sources;
    for (const QString& f : sorted) {
        QImage img(f);
        if (img.isNull()) { r.error = QObject::tr("Image illisible : %1").arg(f); return r; }
        sources.push_back({ f, img.convertToFormat(QImage::Format_ARGB32_Premultiplied) });
        r.sourceSize = r.sourceSize.expandedTo(img.size());
    }

    // Placement : liste de lignes ; une case = index dans `sources` ou -1 (vide)
    std::vector<std::vector<int>> rows;
    int columns = 0;
    static const QRegularExpression rot(R"(^(.*?)[ _\-]?([NESW])$)");
    if (groupRotations) {
        QMap<QString, std::array<int, 4>> groups;   // préfixe -> N, E, S, W
        QStringList order;
        std::vector<int> loose;
        for (int i = 0; i < int(sources.size()); ++i) {
            const QRegularExpressionMatch m = rot.match(QFileInfo(sources[std::size_t(i)].path).completeBaseName());
            if (!m.hasMatch()) { loose.push_back(i); continue; }
            const QString key = m.captured(1);
            if (!groups.contains(key)) { groups.insert(key, { -1, -1, -1, -1 }); order << key; }
            groups[key][std::size_t(QString("NESW").indexOf(m.captured(2)))] = i;
        }
        if (!order.isEmpty()) {
            columns = 4;
            for (const QString& key : order) {
                const auto& g = groups[key];
                rows.push_back({ g[0], g[1], g[2], g[3] });
            }
            for (std::size_t k = 0; k < loose.size(); k += 4) {
                std::vector<int> row(4, -1);
                for (std::size_t j = 0; j < 4 && k + j < loose.size(); ++j) row[j] = loose[k + j];
                rows.push_back(row);
            }
        }
    }
    if (rows.empty()) {   // pas de rotations : grille simple
        columns = std::min<int>(8, int(sources.size()));
        for (std::size_t k = 0; k < sources.size(); k += std::size_t(columns)) {
            std::vector<int> row(std::size_t(columns), -1);
            for (std::size_t j = 0; j < std::size_t(columns) && k + j < sources.size(); ++j) row[j] = int(k + j);
            rows.push_back(row);
        }
    }

    // Rognage : boîte commune de tout ce qui n'est pas transparent
    QRect box(QPoint(0, 0), r.sourceSize);
    if (trim) {
        int x0 = r.sourceSize.width(), y0 = r.sourceSize.height(), x1 = -1, y1 = -1;
        for (const Source& s : sources)
            for (int y = 0; y < s.image.height(); ++y) {
                const QRgb* line = reinterpret_cast<const QRgb*>(s.image.constScanLine(y));
                for (int x = 0; x < s.image.width(); ++x)
                    if (qAlpha(line[x]) > 8) { x0 = std::min(x0, x); x1 = std::max(x1, x); y0 = std::min(y0, y); y1 = std::max(y1, y); }
            }
        if (x1 >= x0) box = QRect(QPoint(x0, y0), QPoint(x1, y1));
    }
    r.tileSize = box.size();
    r.cropOffset = box.topLeft();

    // Trop haut pour une texture : on met plusieurs lignes côte à côte
    int perRow = 1;
    while (int(rows.size() + perRow - 1) / perRow * r.tileSize.height() > kMaxSheetSide &&
           (perRow + 1) * columns * r.tileSize.width() <= kMaxSheetSide)
        ++perRow;
    if (perRow > 1) {
        std::vector<std::vector<int>> merged;
        for (std::size_t k = 0; k < rows.size(); k += std::size_t(perRow)) {
            std::vector<int> row;
            for (int j = 0; j < perRow; ++j) {
                const std::vector<int> part = k + j < rows.size() ? rows[k + j] : std::vector<int>(std::size_t(columns), -1);
                row.insert(row.end(), part.begin(), part.end());
            }
            merged.push_back(row);
        }
        rows = std::move(merged);
        columns *= perRow;
    }
    const QSize sheetSize(columns * r.tileSize.width(), int(rows.size()) * r.tileSize.height());
    if (sheetSize.width() > kMaxSheetSide || sheetSize.height() > kMaxSheetSide) {
        r.error = QObject::tr("Trop d'images pour une seule feuille (%1 x %2 px, maximum %3) : faites plusieurs tilesets.")
                      .arg(sheetSize.width()).arg(sheetSize.height()).arg(kMaxSheetSide);
        return r;
    }

    QImage sheet(sheetSize, QImage::Format_ARGB32_Premultiplied);
    sheet.fill(Qt::transparent);
    {
        QPainter p(&sheet);
        p.setCompositionMode(QPainter::CompositionMode_Source);
        for (std::size_t y = 0; y < rows.size(); ++y)
            for (std::size_t x = 0; x < rows[y].size(); ++x) {
                const int i = rows[y][x];
                if (i < 0) continue;
                p.drawImage(QPoint(int(x) * r.tileSize.width(), int(y) * r.tileSize.height()),
                            sources[std::size_t(i)].image, box);
            }
    }
    QDir().mkpath(QFileInfo(outPng).absolutePath());
    if (!sheet.convertToFormat(QImage::Format_ARGB32).save(outPng, "PNG")) {
        r.error = QObject::tr("Impossible d'écrire %1").arg(outPng);
        return r;
    }
    r.columns = columns;
    r.count = columns * int(rows.size());
    r.ok = true;
    return r;
}
