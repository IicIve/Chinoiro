#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "Vector.h"

class Camera;
class Input;
class MapChipField;
class Object3d;

class SceneEditor final {
public:
  SceneEditor() = default;
  ~SceneEditor();

  SceneEditor(const SceneEditor &) = delete;
  SceneEditor &operator=(const SceneEditor &) = delete;

  void Initialize(Camera *camera, Input *input);
  void Update(const Vector::Vector3 &spawnPosition);
  void Draw() const;
  void Finalize();
  void ClearSceneObjects();
  void RegisterSceneObject(
      const std::string &name,
      std::function<Vector::Vector3()> getPosition,
      std::function<void(const Vector::Vector3 &)> setPosition);
  void RegisterTileMap(MapChipField *mapChipField,
                       std::function<void()> onMapChanged);
  void LoadScene();

  bool IsEditMode() const { return mode == Mode::Edit; }
  bool ShouldRunGame() const {
    return mode == Mode::Play && !pauseGame;
  }

private:
  enum class Mode {
    Edit,
    Play,
  };

  struct EditorObject {
    std::string name;
    std::string modelPath;
    std::unique_ptr<Object3d> object;
  };

  struct SceneObject {
    std::string name;
    std::function<Vector::Vector3()> getPosition;
    std::function<void(const Vector::Vector3 &)> setPosition;
  };

  void RefreshAssets();
  void DrawUI(const Vector::Vector3 &spawnPosition);
  void AddObject(const std::string &modelPath, const std::string &name,
                 const Vector::Vector3 &translate,
                 const Vector::Vector3 &rotate,
                 const Vector::Vector3 &scale);
  void SaveScene();
  void DrawTileMapEditor();
  void DrawWorldGridEditor();
  void SetMode(Mode newMode);

  Camera *camera = nullptr;
  Input *input = nullptr;
  std::vector<std::string> assets;
  std::vector<EditorObject> objects;
  std::vector<SceneObject> sceneObjects;
  MapChipField *mapChipField = nullptr;
  std::function<void()> onMapChanged;
  std::string status;
  int selectedAsset = -1;
  int selectedObject = -1;
  int selectedSceneObject = -1;
  Mode mode = Mode::Edit;
  bool visible = true;
  bool pauseGame = false;
  bool tileMapDirty = false;
  bool worldGridEditing = true;
  int selectedTileType = 1;
  int tileMapWidth = 100;
  int tileMapHeight = 20;
  int pendingTileMapWidth = 100;
  int pendingTileMapHeight = 20;
};
