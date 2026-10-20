#include "SceneEditor.h"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <system_error>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

#include "externals/json.hpp"

#include "Camera.h"
#include "Input.h"
#include "MapChipField.h"
#include "Model.h"
#include "ModelManager.h"
#include "Object3d.h"
#include "Object3dCommon.h"

namespace {

constexpr char kScenePath[] = "resources/editor_scene.json";
constexpr char kMapPath[] = "resources/maptip.csv";
constexpr float kRadiansToDegrees = 180.0f / 3.14159265358979323846f;
constexpr float kDegreesToRadians = 3.14159265358979323846f / 180.0f;

} // namespace

SceneEditor::~SceneEditor() = default;

void SceneEditor::Initialize(Camera *camera, Input *input) {
  assert(camera);
  assert(input);
  this->camera = camera;
  this->input = input;
#ifdef USE_IMGUI
  mode = Mode::Edit;
  visible = true;
  status = "Edit Mode - place and edit objects";
#else
  // ReleaseではエディターUIが無いため、必ずゲーム実行状態で開始する。
  mode = Mode::Play;
  visible = false;
  status = "Play Mode";
#endif
  pauseGame = false;
  RefreshAssets();
}

void SceneEditor::Update(const Vector::Vector3 &spawnPosition) {
  assert(input);
#ifdef USE_IMGUI
  if (input->TriggerKey(DIK_F1)) {
    visible = !visible;
  }
  if (input->TriggerKey(DIK_F2)) {
    SetMode(mode == Mode::Edit ? Mode::Play : Mode::Edit);
  }
#endif

  DrawUI(spawnPosition);
  for (EditorObject &editorObject : objects) {
    editorObject.object->Update();
  }
}

void SceneEditor::Draw() const {
  for (const EditorObject &editorObject : objects) {
    editorObject.object->Draw();
  }
}

void SceneEditor::Finalize() {
  sceneObjects.clear();
  objects.clear();
  assets.clear();
  mapChipField = nullptr;
  onMapChanged = {};
  camera = nullptr;
  input = nullptr;
}

void SceneEditor::RegisterTileMap(MapChipField *newMapChipField,
                                  std::function<void()> newOnMapChanged) {
  assert(newMapChipField);
  mapChipField = newMapChipField;
  onMapChanged = std::move(newOnMapChanged);
  tileMapWidth = static_cast<int>(mapChipField->GetNumBlockHorizontal());
  tileMapHeight = static_cast<int>(mapChipField->GetNumBlockVertical());
}

void SceneEditor::ClearSceneObjects() {
  sceneObjects.clear();
  selectedSceneObject = -1;
}

void SceneEditor::RegisterSceneObject(
    const std::string &name,
    std::function<Vector::Vector3()> getPosition,
    std::function<void(const Vector::Vector3 &)> setPosition) {
  assert(getPosition);
  assert(setPosition);
  sceneObjects.push_back({name, std::move(getPosition),
                          std::move(setPosition)});
}

void SceneEditor::SetMode(Mode newMode) {
  mode = newMode;
  pauseGame = false;
  status = mode == Mode::Edit
               ? "Edit Mode - game logic and collisions are stopped"
               : "Play Mode - game logic and collisions are running";
}

void SceneEditor::RefreshAssets() {
  assets.clear();

  std::error_code error;
  const std::filesystem::path resourceDirectory = "resources";
  std::filesystem::recursive_directory_iterator iterator(resourceDirectory,
                                                          error);
  const std::filesystem::recursive_directory_iterator end;
  while (!error && iterator != end) {
    if (iterator->is_regular_file(error)) {
      std::string extension = iterator->path().extension().string();
      std::transform(extension.begin(), extension.end(), extension.begin(),
                     [](unsigned char character) {
                       return static_cast<char>(std::tolower(character));
                     });
      if (extension == ".obj" || extension == ".gltf") {
        const std::filesystem::path relativePath =
            std::filesystem::relative(iterator->path(), resourceDirectory,
                                      error);
        if (!error) {
          assets.emplace_back(relativePath.generic_string());
        }
      }
    }
    iterator.increment(error);
  }

  std::sort(assets.begin(), assets.end());
  selectedAsset = assets.empty() ? -1 : 0;
  if (error) {
    status = "Could not scan the resources directory";
  }
}

void SceneEditor::AddObject(const std::string &modelPath,
                            const std::string &name,
                            const Vector::Vector3 &translate,
                            const Vector::Vector3 &rotate,
                            const Vector::Vector3 &scale) {
  ModelManager::GetInstance()->LoadModel(modelPath);
  Model *model = ModelManager::GetInstance()->FindModel(modelPath);
  if (!model) {
    status = "Could not load: " + modelPath;
    return;
  }

  EditorObject editorObject;
  editorObject.name = name.empty()
                          ? std::filesystem::path(modelPath).stem().string()
                          : name;
  editorObject.modelPath = modelPath;
  editorObject.object = std::make_unique<Object3d>();
  editorObject.object->Initialize(Object3dCommon::GetInstance());
  editorObject.object->SetModel(model);
  editorObject.object->SetCamera(camera);
  editorObject.object->SetTranslate(translate);
  editorObject.object->SetRotate(rotate);
  editorObject.object->SetScale(scale);
  editorObject.object->Update();

  objects.emplace_back(std::move(editorObject));
  selectedObject = static_cast<int>(objects.size()) - 1;
  status = "Added: " + modelPath;
}

void SceneEditor::SaveScene() {
  nlohmann::json document;
  document["name"] = "editor_scene";
  document["objects"] = nlohmann::json::array();

  for (const EditorObject &editorObject : objects) {
    const Vector::Vector3 &translate = editorObject.object->GetTranslate();
    const Vector::Vector3 &rotate = editorObject.object->GetRotate();
    const Vector::Vector3 &scale = editorObject.object->GetScale();
    document["objects"].push_back({
        {"type", "MESH"},
        {"name", editorObject.name},
        {"file_name", editorObject.modelPath},
        {"transform",
         {{"translation", {translate.x, translate.y, translate.z}},
          {"rotation",
           {rotate.x * kRadiansToDegrees, rotate.y * kRadiansToDegrees,
            rotate.z * kRadiansToDegrees}},
          {"scaling", {scale.x, scale.y, scale.z}}}},
    });
  }

  document["scene_objects"] = nlohmann::json::array();
  for (const SceneObject &sceneObject : sceneObjects) {
    const Vector::Vector3 position = sceneObject.getPosition();
    document["scene_objects"].push_back({
        {"name", sceneObject.name},
        {"position", {position.x, position.y, position.z}},
    });
  }

  std::ofstream output(kScenePath);
  if (!output) {
    status = "Could not save editor_scene.json";
    return;
  }
  output << document.dump(2);
  if (mapChipField && !mapChipField->SaveMapChipCsv(kMapPath)) {
    status = "Scene saved, but map save failed";
    return;
  }
  status = "Saved scene and tile map";
}

void SceneEditor::LoadScene() {
  std::ifstream inputFile(kScenePath);
  if (!inputFile) {
    status = "No saved editor scene yet";
    return;
  }

  std::vector<EditorObject> previousObjects = std::move(objects);
  objects.clear();
  selectedObject = -1;

  try {
    nlohmann::json document;
    inputFile >> document;
    for (const nlohmann::json &item :
         document.value("objects", nlohmann::json::array())) {
      if (item.value("type", "") != "MESH") {
        continue;
      }

      const std::string modelPath = item.value("file_name", "");
      if (modelPath.empty()) {
        continue;
      }

      const nlohmann::json &transform = item.at("transform");
      const auto translation = transform.at("translation");
      const auto rotation = transform.at("rotation");
      const auto scaling = transform.at("scaling");
      AddObject(
          modelPath, item.value("name", "Model"),
          {translation.at(0).get<float>(), translation.at(1).get<float>(),
           translation.at(2).get<float>()},
          {rotation.at(0).get<float>() * kDegreesToRadians,
           rotation.at(1).get<float>() * kDegreesToRadians,
           rotation.at(2).get<float>() * kDegreesToRadians},
          {scaling.at(0).get<float>(), scaling.at(1).get<float>(),
           scaling.at(2).get<float>()});
    }

    for (const nlohmann::json &item :
         document.value("scene_objects", nlohmann::json::array())) {
      const std::string name = item.value("name", "");
      const auto position = item.at("position");
      const auto found = std::find_if(
          sceneObjects.begin(), sceneObjects.end(),
          [&name](const SceneObject &sceneObject) {
            return sceneObject.name == name;
          });
      if (found != sceneObjects.end()) {
        found->setPosition(
            {position.at(0).get<float>(), position.at(1).get<float>(),
             position.at(2).get<float>()});
      }
    }
    if (mapChipField) {
      mapChipField->LoadMapChipCsv(kMapPath);
      tileMapWidth =
          static_cast<int>(mapChipField->GetNumBlockHorizontal());
      tileMapHeight =
          static_cast<int>(mapChipField->GetNumBlockVertical());
      if (onMapChanged) {
        onMapChanged();
      }
    }
    status = "Loaded: resources/editor_scene.json";
  } catch (const std::exception &exception) {
    objects = std::move(previousObjects);
    selectedObject = objects.empty() ? -1 : 0;
    status = std::string("Load failed: ") + exception.what();
  }
}

void SceneEditor::DrawTileMapEditor() {
#ifdef USE_IMGUI
  if (!mapChipField) {
    ImGui::TextDisabled("No tile map is registered");
    return;
  }

  const auto applyResize = [this](int width, int height) {
    width = std::clamp(width, 1,
                       static_cast<int>(MapChipField::kMaxNumBlockHorizontal));
    height = std::clamp(height, 1,
                        static_cast<int>(MapChipField::kMaxNumBlockVertical));
    if (mapChipField->Resize(static_cast<uint32_t>(width),
                             static_cast<uint32_t>(height))) {
      if (onMapChanged) {
        onMapChanged();
      }
      status = "Stage resized (save with Ctrl+S)";
    }
    tileMapWidth = static_cast<int>(mapChipField->GetNumBlockHorizontal());
    tileMapHeight = static_cast<int>(mapChipField->GetNumBlockVertical());
  };

  ImGui::BeginDisabled(mode != Mode::Edit);
  ImGui::TextUnformatted("Stage Size");
  ImGui::SetNextItemWidth(90.0f);
  ImGui::InputInt("Width", &tileMapWidth);
  ImGui::SameLine();
  ImGui::SetNextItemWidth(90.0f);
  ImGui::InputInt("Height", &tileMapHeight);
  ImGui::SameLine();
  if (ImGui::Button("Resize")) {
    pendingTileMapWidth =
        std::clamp(tileMapWidth, 1,
                   static_cast<int>(MapChipField::kMaxNumBlockHorizontal));
    pendingTileMapHeight =
        std::clamp(tileMapHeight, 1,
                   static_cast<int>(MapChipField::kMaxNumBlockVertical));
    const bool isShrinking =
        pendingTileMapWidth <
            static_cast<int>(mapChipField->GetNumBlockHorizontal()) ||
        pendingTileMapHeight <
            static_cast<int>(mapChipField->GetNumBlockVertical());
    if (isShrinking) {
      ImGui::OpenPopup("Confirm Stage Resize");
    } else {
      applyResize(pendingTileMapWidth, pendingTileMapHeight);
    }
  }
  ImGui::TextDisabled("Range: 1-500 wide, 1-100 high");
  ImGui::Separator();
  ImGui::TextUnformatted("Brush");
  ImGui::SameLine();
  ImGui::RadioButton("Ground", &selectedTileType,
                     static_cast<int>(MapChipType::Block));
  ImGui::SameLine();
  ImGui::RadioButton("Goal", &selectedTileType,
                     static_cast<int>(MapChipType::Goal));
  ImGui::SameLine();
  ImGui::RadioButton("Moving Floor", &selectedTileType,
                     static_cast<int>(MapChipType::MovingPlatform));
  ImGui::SameLine();
  ImGui::RadioButton("Hole", &selectedTileType,
                     static_cast<int>(MapChipType::Hole));
  ImGui::SameLine();
  ImGui::RadioButton("Needle", &selectedTileType,
                     static_cast<int>(MapChipType::Needle));
  ImGui::SameLine();
  ImGui::RadioButton("Eraser", &selectedTileType,
                     static_cast<int>(MapChipType::Blank));
  ImGui::TextDisabled("Left drag: paint  |  Right drag: erase");
  ImGui::TextDisabled("Hole/Needle: touching them respawns the player");

  constexpr float kCellSize = 22.0f;
  const float canvasWidth =
      kCellSize * static_cast<float>(mapChipField->GetNumBlockHorizontal());
  const float canvasHeight =
      kCellSize * static_cast<float>(mapChipField->GetNumBlockVertical());

  ImGui::BeginChild("TileMapCanvas", ImVec2(0.0f, 0.0f), true,
                    ImGuiWindowFlags_HorizontalScrollbar);
  const ImVec2 origin = ImGui::GetCursorScreenPos();
  ImGui::InvisibleButton("##TileMapGrid", ImVec2(canvasWidth, canvasHeight),
                         ImGuiButtonFlags_MouseButtonLeft |
                             ImGuiButtonFlags_MouseButtonRight);
  ImDrawList *drawList = ImGui::GetWindowDrawList();

  for (uint32_t y = 0; y < mapChipField->GetNumBlockVertical(); ++y) {
    for (uint32_t x = 0; x < mapChipField->GetNumBlockHorizontal(); ++x) {
      const ImVec2 cellMin(origin.x + static_cast<float>(x) * kCellSize,
                           origin.y + static_cast<float>(y) * kCellSize);
      const ImVec2 cellMax(cellMin.x + kCellSize, cellMin.y + kCellSize);
      ImU32 fillColor = IM_COL32(35, 38, 45, 255);
      switch (mapChipField->GetMapChipTypeByIndex(x, y)) {
      case MapChipType::Block:
        fillColor = IM_COL32(90, 150, 85, 255);
        break;
      case MapChipType::Goal:
        fillColor = IM_COL32(245, 190, 45, 255);
        break;
      case MapChipType::MovingPlatform:
        fillColor = IM_COL32(70, 155, 220, 255);
        break;
      case MapChipType::Hole:
        // Holes are invisible in-game, but remain visible while editing.
        fillColor = IM_COL32(150, 70, 180, 255);
        break;
      case MapChipType::Needle:
        fillColor = IM_COL32(205, 75, 65, 255);
        break;
      case MapChipType::Blank:
        break;
      }
      drawList->AddRectFilled(cellMin, cellMax, fillColor);
      drawList->AddRect(cellMin, cellMax, IM_COL32(75, 80, 90, 255));
    }
  }

  if (ImGui::IsItemHovered() &&
      (ImGui::IsMouseDown(ImGuiMouseButton_Left) ||
       ImGui::IsMouseDown(ImGuiMouseButton_Right))) {
    const ImVec2 mousePosition = ImGui::GetIO().MousePos;
    const int x = static_cast<int>((mousePosition.x - origin.x) / kCellSize);
    const int y = static_cast<int>((mousePosition.y - origin.y) / kCellSize);
    if (x >= 0 && y >= 0 &&
        x < static_cast<int>(mapChipField->GetNumBlockHorizontal()) &&
        y < static_cast<int>(mapChipField->GetNumBlockVertical())) {
      const MapChipType paintType =
          ImGui::IsMouseDown(ImGuiMouseButton_Right)
              ? MapChipType::Blank
              : static_cast<MapChipType>(selectedTileType);
      tileMapDirty |= mapChipField->SetMapChipTypeByIndex(
          static_cast<uint32_t>(x), static_cast<uint32_t>(y), paintType);
    }
  }
  ImGui::EndChild();
  ImGui::EndDisabled();

  if (ImGui::BeginPopupModal("Confirm Stage Resize", nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::TextUnformatted(
        "Shrinking removes tiles outside the new stage size.\n"
        "This cannot be undone after saving.");
    if (ImGui::Button("Resize", ImVec2(120.0f, 0.0f))) {
      applyResize(pendingTileMapWidth, pendingTileMapHeight);
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(120.0f, 0.0f))) {
      tileMapWidth =
          static_cast<int>(mapChipField->GetNumBlockHorizontal());
      tileMapHeight =
          static_cast<int>(mapChipField->GetNumBlockVertical());
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }

  if (tileMapDirty && !ImGui::IsMouseDown(ImGuiMouseButton_Left) &&
      !ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
    tileMapDirty = false;
    if (onMapChanged) {
      onMapChanged();
    }
    status = "Tile map changed (save with Ctrl+S)";
  }
#endif
}

void SceneEditor::DrawUI(const Vector::Vector3 &spawnPosition) {
#ifdef USE_IMGUI
  if (!visible) {
    return;
  }

  if (ImGui::BeginMainMenuBar()) {
    const bool editMode = mode == Mode::Edit;
    if (ImGui::Button(editMode ? "Play  [F2]" : "Stop  [F2]")) {
      SetMode(editMode ? Mode::Play : Mode::Edit);
    }
    ImGui::SameLine();
    ImGui::TextColored(editMode ? ImVec4(1.0f, 0.75f, 0.2f, 1.0f)
                                : ImVec4(0.3f, 1.0f, 0.4f, 1.0f),
                       editMode ? "EDIT MODE" : "PLAY MODE");
    ImGui::SameLine();
    if (ImGui::BeginMenu("File")) {
      ImGui::BeginDisabled(!editMode);
      if (ImGui::MenuItem("Save Scene", "Ctrl+S")) {
        SaveScene();
      }
      if (ImGui::MenuItem("Load Scene", "Ctrl+O")) {
        LoadScene();
      }
      ImGui::EndDisabled();
      ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Editor")) {
      ImGui::BeginDisabled(editMode);
      ImGui::MenuItem("Pause Game", nullptr, &pauseGame);
      ImGui::EndDisabled();
      ImGui::BeginDisabled(!editMode);
      if (ImGui::MenuItem("Refresh Assets")) {
        RefreshAssets();
      }
      ImGui::EndDisabled();
      ImGui::EndMenu();
    }
    ImGui::TextDisabled("  F1: Editor On/Off");
    ImGui::EndMainMenuBar();
  }

  const ImGuiIO &io = ImGui::GetIO();
  if (mode == Mode::Edit && io.KeyCtrl &&
      ImGui::IsKeyPressed(ImGuiKey_S)) {
    SaveScene();
  }
  if (mode == Mode::Edit && io.KeyCtrl &&
      ImGui::IsKeyPressed(ImGuiKey_O)) {
    LoadScene();
  }

  const float editorTop = ImGui::GetFrameHeight();
  const float editorHeight = (std::max)(1.0f, io.DisplaySize.y - editorTop);
  const float leftWidth =
      (std::min)(240.0f, (std::max)(180.0f, io.DisplaySize.x * 0.20f));
  const float rightWidth =
      (std::min)(280.0f, (std::max)(220.0f, io.DisplaySize.x * 0.22f));
  const float gameWidth =
      (std::max)(1.0f, io.DisplaySize.x - leftWidth - rightWidth);
  const float desiredGameHeight = gameWidth * 9.0f / 16.0f;
  const float gameHeight =
      std::clamp(desiredGameHeight, editorHeight * 0.45f,
                 editorHeight * 0.68f);
  const float tileMapHeight = editorHeight - gameHeight;
  const float hierarchyHeight = editorHeight * 0.38f;
  constexpr ImGuiWindowFlags kPanelFlags = ImGuiWindowFlags_None;

  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);

  ImGui::SetNextWindowPos({leftWidth, editorTop}, ImGuiCond_Always);
  ImGui::SetNextWindowSize({gameWidth, gameHeight}, ImGuiCond_Always);
  ImGui::SetNextWindowBgAlpha(0.0f);
  ImGui::Begin("##GameViewFrame", nullptr,
               kPanelFlags | ImGuiWindowFlags_NoTitleBar |
                   ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoInputs |
                   ImGuiWindowFlags_NoScrollbar);
  const ImVec2 gameViewMin = ImGui::GetWindowPos();
  const ImVec2 gameViewMax =
      {gameViewMin.x + ImGui::GetWindowWidth(),
       gameViewMin.y + ImGui::GetWindowHeight()};
  ImGui::GetWindowDrawList()->AddRect(
      gameViewMin, gameViewMax, IM_COL32(105, 110, 120, 255));
  ImGui::GetWindowDrawList()->AddText(
      {gameViewMin.x + 8.0f, gameViewMin.y + 6.0f},
      IM_COL32(210, 215, 225, 220), "GAME VIEW");
  ImGui::End();

  ImGui::SetNextWindowPos({0.0f, editorTop}, ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize({leftWidth, hierarchyHeight},
                           ImGuiCond_FirstUseEver);
  ImGui::Begin("Hierarchy", nullptr, kPanelFlags);
  ImGui::TextDisabled("Scene Objects");
  for (int index = 0; index < static_cast<int>(sceneObjects.size()); ++index) {
    ImGui::PushID(index);
    if (ImGui::Selectable(sceneObjects[index].name.c_str(),
                          selectedSceneObject == index)) {
      selectedSceneObject = index;
      selectedObject = -1;
    }
    ImGui::PopID();
  }
  ImGui::Separator();
  ImGui::TextDisabled("Placed Objects");
  for (int index = 0; index < static_cast<int>(objects.size()); ++index) {
    ImGui::PushID(index);
    if (ImGui::Selectable(objects[index].name.c_str(),
                          selectedObject == index)) {
      selectedObject = index;
      selectedSceneObject = -1;
    }
    ImGui::PopID();
  }
  ImGui::End();

  ImGui::SetNextWindowPos({leftWidth, editorTop + gameHeight},
                          ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize({gameWidth, tileMapHeight},
                           ImGuiCond_FirstUseEver);
  ImGui::Begin("Tile Map", nullptr, kPanelFlags);
  DrawTileMapEditor();
  ImGui::End();

  ImGui::SetNextWindowPos({0.0f, editorTop + hierarchyHeight},
                          ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize({leftWidth, editorHeight - hierarchyHeight},
                           ImGuiCond_FirstUseEver);
  ImGui::Begin("Assets", nullptr, kPanelFlags);
  ImGui::BeginDisabled(mode != Mode::Edit);
  static char assetFilter[128] = {};
  ImGui::InputTextWithHint("##AssetFilter", "Search models...", assetFilter,
                           sizeof(assetFilter));
  ImGui::SameLine();
  if (ImGui::Button("Refresh")) {
    RefreshAssets();
  }

  const std::string filter = assetFilter;
  for (int index = 0; index < static_cast<int>(assets.size()); ++index) {
    if (!filter.empty() && assets[index].find(filter) == std::string::npos) {
      continue;
    }
    if (ImGui::Selectable(assets[index].c_str(), selectedAsset == index)) {
      selectedAsset = index;
    }
    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
      AddObject(assets[index], "", spawnPosition, {0.0f, 0.0f, 0.0f},
                {1.0f, 1.0f, 1.0f});
    }
  }
  ImGui::Separator();
  if (selectedAsset >= 0 && selectedAsset < static_cast<int>(assets.size()) &&
      ImGui::Button("Add Selected Model")) {
    AddObject(assets[selectedAsset], "", spawnPosition, {0.0f, 0.0f, 0.0f},
              {1.0f, 1.0f, 1.0f});
  }
  ImGui::EndDisabled();
  ImGui::TextWrapped("%s", status.c_str());
  ImGui::End();

  ImGui::SetNextWindowPos({leftWidth + gameWidth, editorTop},
                          ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize({rightWidth, editorHeight},
                           ImGuiCond_FirstUseEver);
  ImGui::Begin("Inspector", nullptr, kPanelFlags);
  int objectToDelete = -1;
  int objectToDuplicate = -1;
  if (selectedSceneObject >= 0 &&
      selectedSceneObject < static_cast<int>(sceneObjects.size())) {
    SceneObject &selected = sceneObjects[selectedSceneObject];
    ImGui::Text("%s", selected.name.c_str());
    ImGui::TextDisabled("Existing game object");
    ImGui::Separator();
    ImGui::BeginDisabled(mode != Mode::Edit);
    Vector::Vector3 position = selected.getPosition();
    if (ImGui::DragFloat3("Position", &position.x, 0.1f)) {
      selected.setPosition(position);
    }
    ImGui::EndDisabled();
  } else if (selectedObject >= 0 &&
             selectedObject < static_cast<int>(objects.size())) {
    EditorObject &selected = objects[selectedObject];
    ImGui::BeginDisabled(mode != Mode::Edit);
    char nameBuffer[128] = {};
    strncpy_s(nameBuffer, sizeof(nameBuffer), selected.name.c_str(), _TRUNCATE);
    if (ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer))) {
      selected.name = nameBuffer;
    }
    ImGui::TextWrapped("Model: %s", selected.modelPath.c_str());
    ImGui::Separator();

    Vector::Vector3 translate = selected.object->GetTranslate();
    Vector::Vector3 rotate = selected.object->GetRotate();
    Vector::Vector3 scale = selected.object->GetScale();
    float rotationDegrees[3] = {rotate.x * kRadiansToDegrees,
                                rotate.y * kRadiansToDegrees,
                                rotate.z * kRadiansToDegrees};
    bool changed = ImGui::DragFloat3("Position", &translate.x, 0.1f);
    if (ImGui::DragFloat3("Rotation", rotationDegrees, 1.0f)) {
      rotate = {rotationDegrees[0] * kDegreesToRadians,
                rotationDegrees[1] * kDegreesToRadians,
                rotationDegrees[2] * kDegreesToRadians};
      changed = true;
    }
    changed |= ImGui::DragFloat3("Scale", &scale.x, 0.01f, 0.01f, 100.0f);
    if (changed) {
      selected.object->SetTranslate(translate);
      selected.object->SetRotate(rotate);
      selected.object->SetScale(scale);
    }

    if (ImGui::Button("Duplicate")) {
      objectToDuplicate = selectedObject;
    }
    ImGui::SameLine();
    if (ImGui::Button("Delete")) {
      objectToDelete = selectedObject;
    }
    ImGui::EndDisabled();
  } else {
    ImGui::TextDisabled("Select an object in Hierarchy");
  }
  ImGui::End();
  ImGui::PopStyleVar(2);

  if (objectToDuplicate >= 0) {
    const EditorObject &source = objects[objectToDuplicate];
    AddObject(source.modelPath, source.name + " Copy",
              source.object->GetTranslate(), source.object->GetRotate(),
              source.object->GetScale());
  }
  if (objectToDelete >= 0) {
    objects.erase(objects.begin() + objectToDelete);
    selectedObject =
        objects.empty()
            ? -1
            : std::min(objectToDelete, static_cast<int>(objects.size()) - 1);
    status = "Object deleted";
  }
#else
  (void)spawnPosition;
#endif
}
