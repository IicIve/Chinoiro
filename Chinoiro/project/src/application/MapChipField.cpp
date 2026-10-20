#include "MapChipField.h"

#include <algorithm>
#include <cassert>
#include <fstream>
#include <limits>
#include <sstream>
#include <string_view>

namespace {

MapChipType ParseMapChip(std::string_view value) {
  if (value == "1") {
    return MapChipType::Block;
  }
  if (value == "2") {
    return MapChipType::Goal;
  }
  if (value == "3") {
    return MapChipType::MovingPlatform;
  }
  if (value == "4") {
    return MapChipType::Hole;
  }
  if (value == "5") {
    return MapChipType::Needle;
  }
  return MapChipType::Blank;
}

int ToMapChipValue(MapChipType type) {
  switch (type) {
  case MapChipType::Block:
    return 1;
  case MapChipType::Goal:
    return 2;
  case MapChipType::MovingPlatform:
    return 3;
  case MapChipType::Hole:
    return 4;
  case MapChipType::Needle:
    return 5;
  case MapChipType::Blank:
  default:
    return 0;
  }
}

} // namespace

MapChipField::MapChipField() {
  ResetMapChipData(kDefaultNumBlockHorizontal, kDefaultNumBlockVertical);
}

void MapChipField::ResetMapChipData(uint32_t horizontal, uint32_t vertical) {
  numBlockHorizontal = horizontal;
  numBlockVertical = vertical;
  mapChipData.assign(numBlockVertical,
                     std::vector<MapChipType>(numBlockHorizontal,
                                              MapChipType::Blank));
}

void MapChipField::LoadMapChipCsv(const std::string &filePath) {
  std::ifstream file(filePath);
  assert(file.is_open());

  std::vector<std::vector<MapChipType>> loadedRows;
  std::string line;
  uint32_t loadedWidth = 0;
  while (std::getline(file, line) &&
         loadedRows.size() < kMaxNumBlockVertical) {
    if (line.empty()) {
      continue;
    }
    std::istringstream lineStream(line);
    std::vector<MapChipType> row;
    while (row.size() < kMaxNumBlockHorizontal) {
      std::string value;
      if (!std::getline(lineStream, value, ',')) {
        break;
      }
      row.push_back(ParseMapChip(value));
    }
    if (!row.empty()) {
      loadedWidth = (std::max)(loadedWidth,
                               static_cast<uint32_t>(row.size()));
      loadedRows.emplace_back(std::move(row));
    }
  }

  if (loadedRows.empty() || loadedWidth == 0) {
    ResetMapChipData(kDefaultNumBlockHorizontal,
                     kDefaultNumBlockVertical);
    return;
  }

  ResetMapChipData(loadedWidth,
                   static_cast<uint32_t>(loadedRows.size()));
  for (uint32_t y = 0; y < numBlockVertical; ++y) {
    std::copy(loadedRows[y].begin(), loadedRows[y].end(),
              mapChipData[y].begin());
  }
}

bool MapChipField::SaveMapChipCsv(const std::string &filePath) const {
  std::ofstream file(filePath);
  if (!file) {
    return false;
  }

  for (uint32_t y = 0; y < numBlockVertical; ++y) {
    for (uint32_t x = 0; x < numBlockHorizontal; ++x) {
      if (x > 0) {
        file << ',';
      }
      file << ToMapChipValue(GetMapChipTypeByIndex(x, y));
    }
    file << '\n';
  }
  return static_cast<bool>(file);
}

bool MapChipField::Resize(uint32_t newHorizontal, uint32_t newVertical) {
  newHorizontal = std::clamp(newHorizontal, 1u, kMaxNumBlockHorizontal);
  newVertical = std::clamp(newVertical, 1u, kMaxNumBlockVertical);
  if (newHorizontal == numBlockHorizontal && newVertical == numBlockVertical) {
    return false;
  }

  std::vector<std::vector<MapChipType>> resized(
      newVertical,
      std::vector<MapChipType>(newHorizontal, MapChipType::Blank));
  const uint32_t copyWidth = (std::min)(numBlockHorizontal, newHorizontal);
  const uint32_t copyHeight = (std::min)(numBlockVertical, newVertical);
  const uint32_t oldTop = numBlockVertical - copyHeight;
  const uint32_t newTop = newVertical - copyHeight;
  for (uint32_t row = 0; row < copyHeight; ++row) {
    std::copy_n(mapChipData[oldTop + row].begin(), copyWidth,
                resized[newTop + row].begin());
  }

  mapChipData = std::move(resized);
  numBlockHorizontal = newHorizontal;
  numBlockVertical = newVertical;
  return true;
}

bool MapChipField::SetMapChipTypeByIndex(uint32_t xIndex, uint32_t yIndex,
                                         MapChipType type) {
  if (xIndex >= numBlockHorizontal || yIndex >= numBlockVertical) {
    return false;
  }
  if (mapChipData[yIndex][xIndex] == type) {
    return false;
  }
  mapChipData[yIndex][xIndex] = type;
  return true;
}

MapChipType MapChipField::GetMapChipTypeByIndex(uint32_t xIndex,
                                                uint32_t yIndex) const {
  if (xIndex >= numBlockHorizontal || yIndex >= numBlockVertical) {
    return MapChipType::Blank;
  }
  return mapChipData[yIndex][xIndex];
}

Vector::Vector3
MapChipField::GetMapChipPositionByIndex(uint32_t xIndex,
                                        uint32_t yIndex) const {
  xIndex = (std::min)(xIndex, numBlockHorizontal - 1);
  yIndex = (std::min)(yIndex, numBlockVertical - 1);
  return {kBlockWidth * static_cast<float>(xIndex),
          kBlockHeight * static_cast<float>(numBlockVertical - 1 - yIndex),
          0.0f};
}

MapChipIndex MapChipField::GetMapChipIndexSetByPosition(
    const Vector::Vector3 &position) const {
  constexpr uint32_t kInvalidIndex = std::numeric_limits<uint32_t>::max();
  const float mapLeft = -kBlockWidth / 2.0f;
  const float mapRight =
      kBlockWidth * static_cast<float>(numBlockHorizontal - 1) +
      kBlockWidth / 2.0f;
  const float mapTop =
      kBlockHeight * static_cast<float>(numBlockVertical - 1) +
      kBlockHeight / 2.0f;
  const float mapBottom = -kBlockHeight / 2.0f;

  if (position.x < mapLeft || position.x >= mapRight ||
      position.y < mapBottom || position.y >= mapTop) {
    return {kInvalidIndex, kInvalidIndex};
  }

  return {
      static_cast<uint32_t>((position.x - mapLeft) / kBlockWidth),
      static_cast<uint32_t>((mapTop - position.y) / kBlockHeight),
  };
}

MapChipRect MapChipField::GetRectByIndex(uint32_t xIndex,
                                         uint32_t yIndex) const {
  const Vector::Vector3 center =
      GetMapChipPositionByIndex(xIndex, yIndex);
  return {
      center.x - kBlockWidth / 2.0f,
      center.x + kBlockWidth / 2.0f,
      center.y - kBlockHeight / 2.0f,
      center.y + kBlockHeight / 2.0f,
  };
}
