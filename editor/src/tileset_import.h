#pragma once
// Assemble des images séparées (une PNG par tile) en UNE feuille de tileset.
//  - Les rotations nommées « ..._N / _E / _S / _W » sont rangées sur une ligne, dans cet ordre
//    (pratique pour les packs isométriques : une ligne = un objet et ses 4 orientations).
//  - « Rogner » enlève le vide commun à toutes les images (les tiles iso en ont beaucoup).
#include <QPoint>
#include <QSize>
#include <QString>
#include <QStringList>

struct PackedSheet {
    bool ok = false;
    QString error;
    QSize tileSize;        // taille d'une tile dans la feuille
    int columns = 0;
    int count = 0;         // cases de la grille (certaines peuvent être vides)
    QPoint cropOffset;     // décalage du rognage : ancre dans la feuille = ancre d'origine - cropOffset
    QSize sourceSize;      // taille des images d'origine (la plus grande)
};

PackedSheet packTilesetImages(const QStringList& files, const QString& outPng, bool groupRotations, bool trim);
