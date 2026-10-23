#pragma once

#include <SFML/System/Vector2.hpp>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace Rts {
    inline std::vector<sf::Vector2f> formationPositions(sf::Vector2f center,
        sf::Vector2f dragEnd, std::size_t unitCount) {
        std::vector<sf::Vector2f> positions;
        positions.reserve(unitCount);
        if (unitCount == 0) return positions;

        constexpr float spacing = 40.f;
        const float width = std::abs(dragEnd.x - center.x);
        const float height = std::abs(dragEnd.y - center.y);
        const bool noDrag = width < 12.f && height < 12.f;
        const bool verticalLineDrag = width < 12.f && height >= 12.f;
        std::size_t requestedColumns = noDrag
            ? unitCount
            : static_cast<std::size_t>(std::floor(
                (verticalLineDrag ? height : width) / spacing)) + 1;
        const std::size_t columns = std::clamp<std::size_t>(requestedColumns, 1, unitCount);
        const std::size_t rowsNeeded = (unitCount + columns - 1) / columns;
        const std::size_t rows = rowsNeeded;
        const std::size_t columnsPerRow = columns;

        float dragAngle = noDrag ? 0.f : std::atan2(dragEnd.y - center.y, dragEnd.x - center.x);
        constexpr float pi = 3.14159265359f;
        const float baseAngle = height > width && !verticalLineDrag ? pi * 0.5f : 0.f;
        const float rotation = dragAngle - baseAngle;
        const float cosAngle = std::cos(rotation);
        const float sinAngle = std::sin(rotation);
        std::vector<std::size_t> rowCounts(rows, 0);
        std::size_t remainingUnits = unitCount;
        for (std::size_t row = 0; row < rows && remainingUnits > 0; ++row) {
            rowCounts[row] = std::min(columnsPerRow, remainingUnits);
            remainingUnits -= rowCounts[row];
        }
        for (std::size_t row = 0; row < rows; ++row) {
            const std::size_t rowCount = rowCounts[row];
            for (std::size_t column = 0; column < rowCount; ++column) {
                const float localX = (static_cast<float>(column)
                    - static_cast<float>(rowCount - 1) * 0.5f) * spacing;
                const float localY = (static_cast<float>(row)
                    - static_cast<float>(rows - 1) * 0.5f) * spacing;
                positions.push_back({ center.x + localX * cosAngle - localY * sinAngle,
                    center.y + localX * sinAngle + localY * cosAngle });
            }
        }
        return positions;
    }
}
