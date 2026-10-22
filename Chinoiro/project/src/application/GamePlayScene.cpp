#include "GamePlayScene.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <memory>
#include <numbers>

#include "Camera.h"
#include "Enemy.h"
#include "Input.h"
#include "MapChipField.h"
#include "Model.h"
#include "Object3d.h"
#include "Object3dCommon.h"
#include "ParticleManager.h"
#include "Player.h"
#include "SceneEditor.h"
#include "SceneManager.h"
#include "SkyBox.h"
#include "Sprite.h"
#include "SpriteCommon.h"
#include "Window.h"

namespace {

float ClampCameraCoordinate(float value, float blockSize,
                            uint32_t numBlocks, float margin) {
  const float extent = blockSize * static_cast<float>(numBlocks - 1);
  if (extent <= margin * 2.0f) {
    return extent * 0.5f;
  }
  return std::clamp(value, margin, extent - margin);
}

} // namespace

GamePlayScene::GamePlayScene(SceneManager *sceneManager, Input *input,
                             DirectXCommon *dxCommon, SrvManager *srvManager,
                             ModelCommon *modelCommon)
    : sceneManager(sceneManager), input(input), dxCommon(dxCommon),
      srvManager(srvManager), modelCommon(modelCommon) {
  assert(sceneManager);
  assert(input);
  assert(dxCommon);
  assert(srvManager);
  assert(modelCommon);
}

GamePlayScene::~GamePlayScene() = default;

void GamePlayScene::Initialize() {
  fadeAlpha = 0.0f;
  introCameraProgress = 0.0f;
  introPlayerSpinProgress = 0.0f;
  introSparkEmitted = false;

  mapChipField = std::make_unique<MapChipField>();
  mapChipField->LoadMapChipCsv("resources/maptip.csv");

  blockModel = std::make_unique<Model>();
  blockModel->initialize(modelCommon, "resources/block", "block.obj");

  goalModel = std::make_unique<Model>();
  goalModel->initialize(modelCommon, "resources/goal", "goal.obj");

  needleModel = std::make_unique<Model>();
  needleModel->initialize(modelCommon, "resources/block", "needle.obj");

  playerModel = std::make_unique<Model>();
  playerModel->initialize(modelCommon, "resources/player", "player.obj");

  enemyModel = std::make_unique<Model>();
  enemyModel->initialize(modelCommon, "resources/enemy3", "enemy3.obj");

  camera = std::make_unique<Camera>();
  camera->SetRotate({0.0f, 0.0f, 0.0f});
  camera->SetTranslate(cameraPosition);
  camera->SetFarClip(300.0f);
  camera->Update();

  skyBox = std::make_unique<SkyBox>();
  skyBox->Initialize(dxCommon, "resources/sky_sphere.png");
  skyBox->Update(camera.get());

  fadeSprite = std::make_unique<Sprite>();
  fadeSprite->Initialize(SpriteCommon::GetInstance(), "resources/white.png");
  fadeSprite->SetPosition({0.0f, 0.0f});
  fadeSprite->SetSize({static_cast<float>(Window::kClientWidth),
                       static_cast<float>(Window::kClientHeight)});
  fadeSprite->SetColor({0.0f, 0.0f, 0.0f, fadeAlpha});
  fadeSprite->Update();

  backToTitleSprite = std::make_unique<Sprite>();
  backToTitleSprite->Initialize(SpriteCommon::GetInstance(),
                                "resources/backToTitle.png");
  backToTitleSprite->SetAnchorPoint({0.5f, 0.5f});
  backToTitleSprite->SetPosition({310.0f, 340.0f});
  backToTitleSprite->SetSize({310.0f, 33.0f});
  backToTitleSprite->SetTextureLeftTop({42.0f, 142.0f});
  backToTitleSprite->SetTextureSize({620.0f, 66.0f});
  backToTitleSprite->SetColor({1.0f, 1.0f, 1.0f, 0.0f});
  backToTitleSprite->Update();

  GenerateMapObjects();

  player = std::make_unique<Player>();
  const Vector::Vector3 playerPosition =
      mapChipField->GetMapChipPositionByIndex(2, 11);
  player->Initialize(playerModel.get(), camera.get(), input,
                     mapChipField.get(), playerPosition);
  player->SetRenderRotationY(std::numbers::pi_v<float>);

  introSparkParticles = std::make_unique<ParticleManager>();
  introSparkParticles->Initialize(
      dxCommon, srvManager, camera.get(), "resources/circle.png",
      ParticleManager::PrimitiveType::Plane,
      ParticleManager::BlendMode::Add);
  introSparkParticles->SetPlaneSize(0.5f, 0.5f);
  introSparkParticles->SetUniformScaleRange(2.5f, 3.5f);
  introSparkParticles->SetScaleVelocityRange(-1.0f, -0.55f);
  introSparkParticles->SetColor({1.0f, 0.78f, 0.16f, 1.0f});
  introSparkParticles->SetLifeTimeRange(0.55f, 0.9f);
  introSparkParticles->SetSpeedRange(0.8f, 2.0f);
  introSparkParticles->SetEmitCount(14);

  // タイトル側と同じく顔の直前から開始する。
  camera->SetRotate({0.0f, 0.0f, 0.0f});
  camera->SetTranslate(
      {playerPosition.x, playerPosition.y + 0.35f, playerPosition.z - 4.0f});
  camera->Update();

  GenerateEnemies();
  sceneEditor = std::make_unique<SceneEditor>();
  sceneEditor->Initialize(camera.get(), input);
  sceneEditor->RegisterTileMap(mapChipField.get(),
                               [this]() { GenerateMapObjects(); });
  RegisterEditorObjects();
  sceneEditor->LoadScene();
}

std::unique_ptr<Object3d>
GamePlayScene::CreateMapObject(Model *model,
                               const Vector::Vector3 &position) {
  auto object = std::make_unique<Object3d>();
  object->Initialize(Object3dCommon::GetInstance());
  object->SetModel(model);
  object->SetCamera(camera.get());
  object->SetTranslate(position);
  object->Update();
  return object;
}

void GamePlayScene::GenerateMapObjects() {
  blockObjects.clear();
  goalObjects.clear();
  needleObjects.clear();
  movingPlatforms.clear();
  hazardColliders.clear();
  goalColliders.clear();

  for (uint32_t y = 0; y < mapChipField->GetNumBlockVertical(); ++y) {
    for (uint32_t x = 0; x < mapChipField->GetNumBlockHorizontal(); ++x) {
      const MapChipType type = mapChipField->GetMapChipTypeByIndex(x, y);
      const Vector::Vector3 position =
          mapChipField->GetMapChipPositionByIndex(x, y);

      switch (type) {
      case MapChipType::Block:
        blockObjects.emplace_back(CreateMapObject(blockModel.get(), position));
        break;

      case MapChipType::Goal:
        goalObjects.emplace_back(CreateMapObject(goalModel.get(), position));
        goalColliders.emplace_back(mapChipField->GetRectByIndex(x, y));
        break;

      case MapChipType::MovingPlatform: {
        MovingPlatform platform;
        platform.startPosition = position;
        platform.object = CreateMapObject(blockModel.get(), position);
        movingPlatforms.emplace_back(std::move(platform));
        break;
      }

      case MapChipType::Hole:
        hazardColliders.emplace_back(mapChipField->GetRectByIndex(x, y));
        break;

      case MapChipType::Needle:
        needleObjects.emplace_back(CreateMapObject(
            needleModel.get(),
            {position.x, position.y - MapChipField::kBlockHeight / 2.0f,
             position.z}));
        hazardColliders.emplace_back(mapChipField->GetRectByIndex(x, y));
        break;

      case MapChipType::Blank:
        break;
      }
    }
  }
}

void GamePlayScene::GenerateEnemies() {
  const auto spawnEnemy = [this](uint32_t x, uint32_t y) {
    auto enemy = std::make_unique<Enemy>();
    enemy->Initialize(enemyModel.get(), camera.get(), mapChipField.get(),
                      mapChipField->GetMapChipPositionByIndex(x, y));
    enemies.emplace_back(std::move(enemy));
  };

  spawnEnemy(16, 18);
  spawnEnemy(24, 18);
  spawnEnemy(32, 18);
  spawnEnemy(27, 11);
  spawnEnemy(19, 4);
  spawnEnemy(32, 4);
}

void GamePlayScene::RegisterEditorObjects() {
  sceneEditor->ClearSceneObjects();
  sceneEditor->RegisterSceneObject(
      "Player", [this]() { return player->GetPosition(); },
      [this](const Vector::Vector3 &position) {
        player->SetPosition(position);
      });

  for (size_t index = 0; index < enemies.size(); ++index) {
    Enemy *enemy = enemies[index].get();
    sceneEditor->RegisterSceneObject(
        "Enemy " + std::to_string(index),
        [enemy]() { return enemy->GetPosition(); },
        [enemy](const Vector::Vector3 &position) {
          enemy->SetPosition(position);
        });
  }
}

void GamePlayScene::CheckPlayerEnemyCollisions() {
  const PlayerAABB playerAABB = player->GetAABB();
  for (auto enemyIterator = enemies.begin();
       enemyIterator != enemies.end();) {
    const EnemyAABB enemyAABB = (*enemyIterator)->GetAABB();
    const bool isColliding =
        playerAABB.min.x < enemyAABB.max.x &&
        playerAABB.max.x > enemyAABB.min.x &&
        playerAABB.min.y < enemyAABB.max.y &&
        playerAABB.max.y > enemyAABB.min.y &&
        playerAABB.min.z < enemyAABB.max.z &&
        playerAABB.max.z > enemyAABB.min.z;

    if (isColliding) {
      if (player->IsAttacking()) {
        enemyIterator = enemies.erase(enemyIterator);
        RegisterEditorObjects();
        continue;
      }

      if (player->CanBeHit()) {
        RespawnPlayer();
        return;
      }
    }

    ++enemyIterator;
  }
}

bool GamePlayScene::CheckPlayerHazardCollision() const {
  const PlayerAABB playerAABB = player->GetAABB();
  for (const MapChipRect &hazardRect : hazardColliders) {
    const bool isOverlapping =
        playerAABB.min.x < hazardRect.right &&
        playerAABB.max.x > hazardRect.left &&
        playerAABB.min.y < hazardRect.top &&
        playerAABB.max.y > hazardRect.bottom;
    if (isOverlapping) {
      return true;
    }
  }
  return false;
}

bool GamePlayScene::CheckPlayerGoalCollision() const {
  const PlayerAABB playerAABB = player->GetAABB();
  for (const MapChipRect &goalRect : goalColliders) {
    const bool isOverlapping =
        playerAABB.min.x < goalRect.right &&
        playerAABB.max.x > goalRect.left &&
        playerAABB.min.y < goalRect.top &&
        playerAABB.max.y > goalRect.bottom;
    if (isOverlapping) {
      return true;
    }
  }
  return false;
}

void GamePlayScene::RespawnPlayer() {
  player->Respawn();

  const Vector::Vector3 &position = player->GetPosition();
  cameraPosition.x = ClampCameraCoordinate(
      position.x, MapChipField::kBlockWidth,
      mapChipField->GetNumBlockHorizontal(), 20.0f);
  cameraPosition.y = ClampCameraCoordinate(
      position.y, MapChipField::kBlockHeight,
      mapChipField->GetNumBlockVertical(), 10.0f);
  camera->SetTranslate(cameraPosition);
  camera->Update();
}

void GamePlayScene::UpdateCamera() {
  assert(player);

  constexpr float kFollowRate = 0.1f;
  const Vector::Vector3 &playerPosition = player->GetPosition();
  const float targetX = playerPosition.x;
  const float targetY = playerPosition.y;

  cameraPosition.x += (targetX - cameraPosition.x) * kFollowRate;
  cameraPosition.y += (targetY - cameraPosition.y) * kFollowRate;

  cameraPosition.x = ClampCameraCoordinate(
      cameraPosition.x, MapChipField::kBlockWidth,
      mapChipField->GetNumBlockHorizontal(), 20.0f);
  cameraPosition.y = ClampCameraCoordinate(
      cameraPosition.y, MapChipField::kBlockHeight,
      mapChipField->GetNumBlockVertical(), 10.0f);

  camera->SetTranslate(cameraPosition);
  camera->Update();
}

void GamePlayScene::Update() {
  if (introCameraProgress < 1.0f || introPlayerSpinProgress < 1.0f) {
    if (input->TriggerKey(DIK_ESCAPE)) {
      sceneManager->ChangeScene(SceneType::Title);
      return;
    }

    if (introCameraProgress < 1.0f) {
      introCameraProgress =
          (std::min)(1.0f, introCameraProgress + 0.9f / 60.0f);
    } else {
      // カメラが引ききってから、プレイヤーの回転を始める。
      introPlayerSpinProgress =
          (std::min)(1.0f, introPlayerSpinProgress + 1.4f / 60.0f);
    }

    const float cameraProgress = introCameraProgress * introCameraProgress *
                                 (3.0f - 2.0f * introCameraProgress);
    const Vector::Vector3 &playerPosition = player->GetPosition();
    const Vector::Vector3 faceCamera{playerPosition.x,
                                     playerPosition.y + 0.35f,
                                     playerPosition.z - 4.0f};
    camera->SetTranslate(
        {faceCamera.x + (cameraPosition.x - faceCamera.x) * cameraProgress,
         faceCamera.y + (cameraPosition.y - faceCamera.y) * cameraProgress,
         faceCamera.z + (cameraPosition.z - faceCamera.z) * cameraProgress});
    camera->SetRotate({0.0f, 0.0f, 0.0f});
    camera->Update();

    // 正面向きから時計回りに1回転し、最後の1/4回転で
    // ゲーム中の進行方向（右）へ向きを揃える。
    const float spinProgress =
        introPlayerSpinProgress * introPlayerSpinProgress *
        (3.0f - 2.0f * introPlayerSpinProgress);
    const float playerRotation =
        std::numbers::pi_v<float> -
        std::numbers::pi_v<float> * 2.5f * spinProgress;
    player->SetRenderRotationY(playerRotation);

    // 1回転した瞬間に、プレイヤーの周りへ光粒を一度だけ放出する。
    if (!introSparkEmitted && spinProgress >= 0.8f) {
      Vector::Vector3 sparkPosition = playerPosition;
      sparkPosition.y += 0.6f;
      sparkPosition.z -= 0.2f;
      introSparkParticles->Emit(sparkPosition);
      introSparkEmitted = true;
    }
    introSparkParticles->Update(1.0f / 60.0f);
    skyBox->Update(camera.get());

    for (const std::unique_ptr<Object3d> &object : blockObjects) {
      object->Update();
    }
    for (const std::unique_ptr<Object3d> &object : goalObjects) {
      object->Update();
    }
    for (const std::unique_ptr<Object3d> &object : needleObjects) {
      object->Update();
    }
    for (MovingPlatform &platform : movingPlatforms) {
      platform.object->Update();
    }
    for (const std::unique_ptr<Enemy> &enemy : enemies) {
      enemy->Update();
    }
    return;
  }

  sceneEditor->Update({cameraPosition.x, cameraPosition.y, 0.0f});
  if (input->TriggerKey(DIK_ESCAPE)) {
    sceneManager->ChangeScene(SceneType::Title);
    return;
  }

  if (!sceneEditor->ShouldRunGame()) {
    return;
  }

  movingPlatformTime += 1.0f / 60.0f;
  const float movingOffset = std::sin(movingPlatformTime) * 3.0f;
  for (MovingPlatform &platform : movingPlatforms) {
    const Vector::Vector3 previousPosition = platform.object->GetTranslate();
    Vector::Vector3 position = platform.startPosition;
    position.x += movingOffset;
    platform.moveDelta = {
        position.x - previousPosition.x,
        position.y - previousPosition.y,
        position.z - previousPosition.z,
    };
    platform.object->SetTranslate(position);
  }

  player->Update();
  if (CheckPlayerGoalCollision()) {
    sceneManager->ChangeScene(SceneType::Title);
    return;
  }
  if (CheckPlayerHazardCollision()) {
    RespawnPlayer();
  }
  for (const std::unique_ptr<Enemy> &enemy : enemies) {
    enemy->Update();
  }
  CheckPlayerEnemyCollisions();
  for (const MovingPlatform &platform : movingPlatforms) {
    const Vector::Vector3 &position = platform.object->GetTranslate();
    const MapChipRect platformRect = {
        position.x - MapChipField::kBlockWidth / 2.0f,
        position.x + MapChipField::kBlockWidth / 2.0f,
        position.y - MapChipField::kBlockHeight / 2.0f,
        position.y + MapChipField::kBlockHeight / 2.0f,
    };
    player->ResolveMovingPlatformCollision(platformRect,
                                           platform.moveDelta);
  }
  UpdateCamera();
  skyBox->Update(camera.get());

  for (const std::unique_ptr<Object3d> &object : blockObjects) {
    object->Update();
  }
  for (const std::unique_ptr<Object3d> &object : goalObjects) {
    object->Update();
  }
  for (const std::unique_ptr<Object3d> &object : needleObjects) {
    object->Update();
  }
  for (MovingPlatform &platform : movingPlatforms) {
    platform.object->Update();
  }

  constexpr float kFadeSpeed = 2.0f;
  fadeAlpha -= kFadeSpeed / 60.0f;
  if (fadeAlpha < 0.0f) {
    fadeAlpha = 0.0f;
  }
  fadeSprite->SetColor({0.0f, 0.0f, 0.0f, fadeAlpha});
  fadeSprite->Update();
  backToTitleSprite->SetColor({1.0f, 1.0f, 1.0f, 0.9f});
  backToTitleSprite->Update();
  introSparkParticles->Update(1.0f / 60.0f);
}

void GamePlayScene::Draw() {
  skyBox->CreatePrimitiveTopology();
  skyBox->Draw();

  Object3dCommon::GetInstance()->CreatePrimitiveTopology();

  for (const std::unique_ptr<Object3d> &object : blockObjects) {
    object->Draw();
  }
  for (const std::unique_ptr<Object3d> &object : goalObjects) {
    object->Draw();
  }
  for (const std::unique_ptr<Object3d> &object : needleObjects) {
    object->Draw();
  }
  sceneEditor->Draw();
  for (const MovingPlatform &platform : movingPlatforms) {
    platform.object->Draw();
  }
  player->Draw();
  for (const std::unique_ptr<Enemy> &enemy : enemies) {
    enemy->Draw();
  }
  introSparkParticles->Draw();

  SpriteCommon::GetInstance()->CreatePrimitiveTopology();
  if (!sceneEditor->IsEditMode()) {
    backToTitleSprite->Draw();
    fadeSprite->Draw();
  }
}

void GamePlayScene::Finalize() {
  if (sceneEditor) {
    sceneEditor->Finalize();
    sceneEditor.reset();
  }
  introSparkParticles.reset();
  backToTitleSprite.reset();
  fadeSprite.reset();
  enemies.clear();
  player.reset();
  movingPlatforms.clear();
  hazardColliders.clear();
  goalColliders.clear();
  needleObjects.clear();
  goalObjects.clear();
  blockObjects.clear();
  skyBox.reset();
  camera.reset();
  playerModel.reset();
  enemyModel.reset();
  needleModel.reset();
  goalModel.reset();
  blockModel.reset();
  mapChipField.reset();
}
