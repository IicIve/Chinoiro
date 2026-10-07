#include "TitleScene.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <memory>

#include "Camera.h"
#include "Input.h"
#include "Model.h"
#include "Object3d.h"
#include "Object3dCommon.h"
#include "SceneManager.h"
#include "SkyBox.h"
#include "Sprite.h"
#include "SpriteCommon.h"
#include "Window.h"

TitleScene::TitleScene(SceneManager *sceneManager, Input *input,
                       DirectXCommon *dxCommon,
                       ModelCommon *modelCommon, AudioManager *audioManager)
    : sceneManager(sceneManager), input(input), dxCommon(dxCommon),
      modelCommon(modelCommon), audioManager(audioManager) {
  assert(sceneManager);
  assert(input);
  assert(dxCommon);
  assert(modelCommon);
  assert(audioManager);
}

TitleScene::~TitleScene() = default;

void TitleScene::Initialize() {
  animationTime = 0.0f;
  fadeAlpha = 0.0f;
  isFadingOut = false;

  titleBgm = audioManager->Load(
      "resources/sounds/bgm/MusMus-BGM-157.mp3");
  titleBgmVoice = audioManager->Play(titleBgm, true, 0.5f);

  camera = std::make_unique<Camera>();
  camera->SetRotate({0.3f, 0.0f, 0.0f});
  camera->SetTranslate({5.0f, 15.0f, -55.0f});
  camera->Update();

  skyBox = std::make_unique<SkyBox>();
  skyBox->Initialize(dxCommon, "resources/sky_sphere.png");
  skyBox->Update(camera.get());

  titleModel = std::make_unique<Model>();
  titleModel->initialize(modelCommon, "resources/title", "title.obj");

  titleObject = std::make_unique<Object3d>();
  titleObject->Initialize(Object3dCommon::GetInstance());
  titleObject->SetModel(titleModel.get());
  titleObject->SetCamera(camera.get());
  titleObject->SetScale({1.0f, 1.0f, 1.0f});
  titleObject->SetRotate({0.0f, 5.2f, 0.0f});
  titleObject->SetTranslate({-10.0f, -4.0f, 0.0f});
  titleObject->Update();

  playerModel = std::make_unique<Model>();
  playerModel->initialize(modelCommon, "resources/player", "player.obj");

  playerObject = std::make_unique<Object3d>();
  playerObject->Initialize(Object3dCommon::GetInstance());
  playerObject->SetModel(playerModel.get());
  playerObject->SetCamera(camera.get());
  playerObject->SetScale({1.2f, 1.2f, 1.2f});

  promptPanelSprite = std::make_unique<Sprite>();
  promptPanelSprite->Initialize(SpriteCommon::GetInstance(),
                                "resources/white.png");
  promptPanelSprite->SetPosition({478.0f, 569.0f});
  promptPanelSprite->SetSize({295.0f, 55.0f});
  promptPanelSprite->SetColor({1.0f, 1.0f, 1.0f, 0.78f});

  promptSprite = std::make_unique<Sprite>();
  promptSprite->Initialize(SpriteCommon::GetInstance(),
                           "resources/pressSpace.png");
  promptSprite->SetPosition({480.0f, 572.0f});
  promptSprite->SetSize({300.0f, 50.0f});
  promptSprite->SetTextureLeftTop({60.0f, 130.0f});
  promptSprite->SetTextureSize({600.0f, 100.0f});

  fadeSprite = std::make_unique<Sprite>();
  fadeSprite->Initialize(SpriteCommon::GetInstance(), "resources/white.png");
  fadeSprite->SetPosition({0.0f, 0.0f});
  fadeSprite->SetSize({static_cast<float>(Window::kClientWidth),
                       static_cast<float>(Window::kClientHeight)});
  fadeSprite->SetColor({0.0f, 0.0f, 0.0f, fadeAlpha});

  promptPanelSprite->Update();
  promptSprite->Update();
  fadeSprite->Update();
}

void TitleScene::Update() {
  animationTime += 1.0f / 60.0f;

  const float bob = std::sin(animationTime * 1.5f) * 0.4f;
  const float sway = std::sin(animationTime * 0.7f) * 0.05f;
  const float titleRotation = 5.2f + sway;
  titleObject->SetTranslate({-10.0f, -4.0f + bob, 0.0f});
  titleObject->SetRotate({0.0f, titleRotation, 0.0f});
  titleObject->Update();

  // 「ロ」の穴の中央をタイトルモデルのローカル座標からワールド座標へ変換する。
  constexpr float kPlayerLocalX = -0.5f;
  constexpr float kPlayerLocalY = 6.6f;
  constexpr float kPlayerLocalZ = -29.2f;
  const float cosY = std::cos(titleRotation);
  const float sinY = std::sin(titleRotation);
  const Vector::Vector3 playerPosition{
      -10.0f + kPlayerLocalX * cosY + kPlayerLocalZ * sinY,
      -4.0f + bob + kPlayerLocalY,
      -kPlayerLocalX * sinY + kPlayerLocalZ * cosY};
  playerObject->SetTranslate(playerPosition);
  playerObject->SetRotate({0.0f, titleRotation + 4.2f, 0.0f});
  playerObject->Update();

  constexpr float kSkyRotationSpeed = 0.08f;
  skyBox->SetRotate({0.0f, animationTime * kSkyRotationSpeed, 0.0f});
  skyBox->Update(camera.get());

  const float promptAlpha =
      0.55f + (std::sin(animationTime * 3.0f) + 1.0f) * 0.225f;
  promptSprite->SetColor({1.0f, 1.0f, 1.0f, promptAlpha});

  promptPanelSprite->Update();
  promptSprite->Update();

  if (!isFadingOut && input->TriggerKey(DIK_SPACE)) {
    isFadingOut = true;
  }

  if (isFadingOut) {
    constexpr float kFadeSpeed = 2.0f;
    fadeAlpha += kFadeSpeed / 60.0f;
    if (fadeAlpha > 1.0f) {
      fadeAlpha = 1.0f;
    }
    if (fadeAlpha >= 1.0f) {
      sceneManager->ChangeScene(SceneType::GamePlay);
    }
  }

  fadeSprite->SetColor({0.0f, 0.0f, 0.0f, fadeAlpha});
  fadeSprite->Update();
}

void TitleScene::Draw() {
  skyBox->CreatePrimitiveTopology();
  skyBox->Draw();

  Object3dCommon::GetInstance()->CreatePrimitiveTopology();
  titleObject->Draw();
  playerObject->Draw();

  SpriteCommon::GetInstance()->CreatePrimitiveTopology();
  promptPanelSprite->Draw();
  promptSprite->Draw();
  fadeSprite->Draw();
}

void TitleScene::Finalize() {
  audioManager->Stop(titleBgmVoice);
  audioManager->Unload(&titleBgm);

  fadeSprite.reset();
  promptSprite.reset();
  promptPanelSprite.reset();
  playerObject.reset();
  playerModel.reset();
  titleObject.reset();
  titleModel.reset();
  skyBox.reset();
  camera.reset();
}
