#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "Vector.h"

enum class MapChipType {
  Blank,
  Block,
  Goal,
  MovingPlatform,
  Hole,
  Needle,
};

struct MapChipIndex {
  uint32_t x;
  uint32_t y;
};

struct MapChipRect {
  float left;
  float right;
  float bottom;
  float top;
};

class MapChipField {
public:
  static constexpr float kBlockWidth = 2.0f;
  static constexpr float kBlockHeight = 2.0f;
  static constexpr uint32_t kDefaultNumBlockVertical = 20;
  static constexpr uint32_t kDefaultNumBlockHorizontal = 100;
  static constexpr uint32_t kMaxNumBlockVertical = 100;
  static constexpr uint32_t kMaxNumBlockHorizontal = 500;

  MapChipField();

  void LoadMapChipCsv(const std::string &filePath);
  bool SaveMapChipCsv(const std::string &filePath) const;
  bool Resize(uint32_t numBlockHorizontal, uint32_t numBlockVertical);
  bool SetMapChipTypeByIndex(uint32_t xIndex, uint32_t yIndex,
                             MapChipType type);

  MapChipType GetMapChipTypeByIndex(uint32_t xIndex,
                                    uint32_t yIndex) const;
  Vector::Vector3 GetMapChipPositionByIndex(uint32_t xIndex,
                                            uint32_t yIndex) const;
  MapChipIndex
  GetMapChipIndexSetByPosition(const Vector::Vector3 &position) const;
  MapChipRect GetRectByIndex(uint32_t xIndex, uint32_t yIndex) const;

  uint32_t GetNumBlockVertical() const { return numBlockVertical; }
  uint32_t GetNumBlockHorizontal() const { return numBlockHorizontal; }

private:
  void ResetMapChipData(uint32_t horizontal, uint32_t vertical);

  std::vector<std::vector<MapChipType>> mapChipData;
  uint32_t numBlockVertical = kDefaultNumBlockVertical;
  uint32_t numBlockHorizontal = kDefaultNumBlockHorizontal;
};
