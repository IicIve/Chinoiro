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
#include "Model.h"
#include "ModelManager.h"
#include "Object3d.h"
#include "Object3dCommon.h"

namespace {

constexpr char kScenePath[] = "resources/editor_scene.json";
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
  camera = nullptr;
  input = nullptr;
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
  status = "Saved: resources/editor_scene.json";
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
    status = "Loaded: resources/editor_scene.json";
  } catch (const std::exception &exception) {
    objects = std::move(previousObjects);
    selectedObject = objects.empty() ? -1 : 0;
    status = std::string("Load failed: ") + exception.what();
  }
}

void SceneEditor::DrawUI(const Vector::Vector3 &spawnPosition) {
#ifdef USE_IMGUI
  if (!visible) {
    return;
  }

  if (ImGui::BeginMainMenuBar()) {
    const bool editMode = mode == Mode::Edit;
    if (ImGui::Button(editMode ? "Play  [F5]" : "Stop  [F5]")) {
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

  ImGui::SetNextWindowPos({8.0f, 28.0f}, ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize({270.0f, 320.0f}, ImGuiCond_FirstUseEver);
  ImGui::Begin("Hierarchy");
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

  ImGui::SetNextWindowPos({8.0f, 356.0f}, ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize({420.0f, 350.0f}, ImGuiCond_FirstUseEver);
  ImGui::Begin("Assets");
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

  ImGui::SetNextWindowPos({950.0f, 28.0f}, ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize({322.0f, 678.0f}, ImGuiCond_FirstUseEver);
  ImGui::Begin("Inspector");
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
