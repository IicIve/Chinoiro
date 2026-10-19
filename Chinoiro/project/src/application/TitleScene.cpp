#include "TitleScene.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
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

namespace {

std::array<uint8_t, 7> GetGlyph(char character) {
  switch (character) {
  case 'A': return {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11};
  case 'C': return {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E};
  case 'D': return {0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E};
  case 'E': return {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F};
  case 'I': return {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x1F};
  case 'M': return {0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11};
  case 'N': return {0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11};
  case 'O': return {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E};
  case 'P': return {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10};
  case 'R': return {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11};
  case 'S': return {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E};
  case 'T': return {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04};
  case 'U': return {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E};
  case 'W': return {0x11, 0x11, 0x11, 0x15, 0x15, 0x15, 0x0A};
  case '/': return {0x01, 0x02, 0x02, 0x04, 0x08, 0x08, 0x10};
  default: return {};
  }
}

float SmoothStep(float value) {
  value = std::clamp(value, 0.0f, 1.0f);
  return value * value * (3.0f - 2.0f * value);
}

} // namespace

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
  fadeAlpha = 1.0f;
  menuTransition = 0.0f;
  gameTransition = 0.0f;
  selectionY = 357.0f;
  selectedItem = 0;
  state = State::PressStart;

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

  menuShadeSprite = std::make_unique<Sprite>();
  menuShadeSprite->Initialize(SpriteCommon::GetInstance(),
                              "resources/white.png");
  menuShadeSprite->SetPosition({0.0f, 0.0f});
  menuShadeSprite->SetSize({static_cast<float>(Window::kClientWidth),
                            static_cast<float>(Window::kClientHeight)});
  menuShadeSprite->SetColor({0.015f, 0.025f, 0.055f, 0.0f});

  menuPanelSprite = std::make_unique<Sprite>();
  menuPanelSprite->Initialize(SpriteCommon::GetInstance(),
                              "resources/white.png");
  menuPanelSprite->SetAnchorPoint({0.5f, 0.5f});
  menuPanelSprite->SetPosition({640.0f, 400.0f});
  menuPanelSprite->SetSize({0.0f, 360.0f});
  menuPanelSprite->SetColor({0.025f, 0.045f, 0.09f, 0.0f});

  selectionGlowSprite = std::make_unique<Sprite>();
  selectionGlowSprite->Initialize(SpriteCommon::GetInstance(),
                                  "resources/white.png");
  selectionGlowSprite->SetAnchorPoint({0.5f, 0.5f});
  selectionGlowSprite->SetPosition({640.0f, selectionY});
  selectionGlowSprite->SetSize({430.0f, 66.0f});

  selectionLineSprite = std::make_unique<Sprite>();
  selectionLineSprite->Initialize(SpriteCommon::GetInstance(),
                                  "resources/white.png");
  selectionLineSprite->SetAnchorPoint({0.5f, 0.5f});
  selectionLineSprite->SetPosition({640.0f, selectionY + 32.0f});
  selectionLineSprite->SetSize({330.0f, 2.0f});

  gameStartButtonSprite = std::make_unique<Sprite>();
  gameStartButtonSprite->Initialize(SpriteCommon::GetInstance(),
                                    "resources/white.png");
  gameStartButtonSprite->SetAnchorPoint({0.5f, 0.5f});
  gameStartButtonSprite->SetPosition({640.0f, 357.0f});
  gameStartButtonSprite->SetSize({430.0f, 66.0f});

  quitButtonSprite = std::make_unique<Sprite>();
  quitButtonSprite->Initialize(SpriteCommon::GetInstance(),
                               "resources/white.png");
  quitButtonSprite->SetAnchorPoint({0.5f, 0.5f});
  quitButtonSprite->SetPosition({640.0f, 457.0f});
  quitButtonSprite->SetSize({430.0f, 66.0f});

  gameStartSprite = std::make_unique<Sprite>();
  gameStartSprite->Initialize(SpriteCommon::GetInstance(),
                              "resources/gameStart.png");
  gameStartSprite->SetAnchorPoint({0.5f, 0.5f});
  gameStartSprite->SetPosition({640.0f, 357.0f});
  gameStartSprite->SetSize({316.0f, 37.0f});
  gameStartSprite->SetTextureLeftTop({92.0f, 159.0f});
  gameStartSprite->SetTextureSize({526.0f, 61.0f});

  quitSprite = std::make_unique<Sprite>();
  quitSprite->Initialize(SpriteCommon::GetInstance(), "resources/quit.png");
  quitSprite->SetAnchorPoint({0.5f, 0.5f});
  quitSprite->SetPosition({640.0f, 457.0f});
  quitSprite->SetSize({122.0f, 43.0f});
  quitSprite->SetTextureLeftTop({227.0f, 159.0f});
  quitSprite->SetTextureSize({203.0f, 72.0f});

  CreateBitmapText(menuTitleText, "MAIN MENU", 505.0f, 254.0f, 5.0f);
  CreateBitmapText(controlsText, "UP / DOWN   SPACE / ENTER", 388.0f,
                   564.0f, 3.0f);

  fadeSprite = std::make_unique<Sprite>();
  fadeSprite->Initialize(SpriteCommon::GetInstance(), "resources/white.png");
  fadeSprite->SetPosition({0.0f, 0.0f});
  fadeSprite->SetSize({static_cast<float>(Window::kClientWidth),
                       static_cast<float>(Window::kClientHeight)});
  fadeSprite->SetColor({0.0f, 0.0f, 0.0f, fadeAlpha});

  promptPanelSprite->Update();
  promptSprite->Update();
  menuShadeSprite->Update();
  menuPanelSprite->Update();
  selectionGlowSprite->Update();
  selectionLineSprite->Update();
  gameStartButtonSprite->Update();
  quitButtonSprite->Update();
  gameStartSprite->Update();
  quitSprite->Update();
  UpdateText(menuTitleText);
  UpdateText(controlsText);
  fadeSprite->Update();
}

void TitleScene::Update() {
  animationTime += 1.0f / 60.0f;

  // Spaceでメニューを開くとき、タイトル全体を弾かれたように回転させる。
  // 回転は最初に強い勢いを与え、慣性が抜けるように減速しながら3回転する。
  constexpr float kPi = 3.1415926535f;
  const float kickProgress =
      state == State::OpeningMenu
          ? std::clamp(menuTransition, 0.0f, 1.0f)
          : (menuTransition >= 1.0f ? 1.0f : 0.0f);
  const float spinProgress =
      1.0f - std::pow(1.0f - kickProgress, 7.0f);
  const float kickArc = std::sin(kickProgress * kPi);
  const float kickWobble =
      std::sin(kickProgress * kPi * 3.0f) * (1.0f - kickProgress);

  const float bob = std::sin(animationTime * 1.5f) * 0.4f;
  const float sway = std::sin(animationTime * 0.7f) * 0.05f;
  const float baseTitleRotation = 5.2f + sway;
  const float titleRotation =
      baseTitleRotation + spinProgress * (6.0f * kPi) + kickArc * 0.55f +
      kickWobble * 0.18f;
  const float titleScale = 1.0f + kickArc * 0.08f;

  // title.objのバウンディングボックス中心を回転軸にする。
  // Object3dは原点回転なので、中心のワールド座標が変わらないように
  // translateを逆算する。
  constexpr float kTitlePivotX = 0.0f;
  constexpr float kTitlePivotY = 8.4333455f;
  constexpr float kTitlePivotZ = -14.888354f;
  const float baseCosY = std::cos(baseTitleRotation);
  const float baseSinY = std::sin(baseTitleRotation);
  const float pivotWorldX =
      -10.0f + kTitlePivotX * baseCosY + kTitlePivotZ * baseSinY -
      kickArc * 3.0f;
  const float pivotWorldY =
      -4.0f + bob + kTitlePivotY + kickArc * 4.2f;
  const float pivotWorldZ =
      -kTitlePivotX * baseSinY + kTitlePivotZ * baseCosY;
  const float cosY = std::cos(titleRotation);
  const float sinY = std::sin(titleRotation);
  const float titleX =
      pivotWorldX -
      (kTitlePivotX * cosY + kTitlePivotZ * sinY) * titleScale;
  const float titleY = pivotWorldY - kTitlePivotY * titleScale;
  const float titleZ =
      pivotWorldZ -
      (-kTitlePivotX * sinY + kTitlePivotZ * cosY) * titleScale;
  titleObject->SetTranslate({titleX, titleY, titleZ});
  titleObject->SetRotate({0.0f, titleRotation, 0.0f});
  titleObject->SetScale({titleScale, titleScale, titleScale});

  // 「ロ」の穴の中央をタイトルモデルのローカル座標からワールド座標へ変換する。
  constexpr float kPlayerLocalX = -0.5f;
  constexpr float kPlayerLocalY = 6.6f;
  constexpr float kPlayerLocalZ = -29.2f;
  const Vector::Vector3 playerPosition{
      titleX +
          (kPlayerLocalX * cosY + kPlayerLocalZ * sinY) * titleScale,
      titleY + kPlayerLocalY * titleScale,
      titleZ +
          (-kPlayerLocalX * sinY + kPlayerLocalZ * cosY) * titleScale};
  playerObject->SetTranslate(playerPosition);
  playerObject->SetRotate({0.0f, titleRotation + 4.2f, 0.0f});
  playerObject->SetScale(
      {1.2f * titleScale, 1.2f * titleScale, 1.2f * titleScale});

  // ゲーム開始時はプレイヤーの顔へ一気にカメラを寄せる。
  // 顔が画面全体を覆ったフレームでシーンを切り替える。
  if (state == State::FadingToGame) {
    gameTransition =
        (std::min)(1.0f, gameTransition + 1.25f / 60.0f);
    const float cameraProgress = SmoothStep(gameTransition);
    const Vector::Vector3 startCamera{5.0f, 15.0f, -55.0f};
    const Vector::Vector3 faceCamera{playerPosition.x,
                                     playerPosition.y + 0.35f,
                                     playerPosition.z - 4.0f};
    camera->SetTranslate(
        {startCamera.x + (faceCamera.x - startCamera.x) * cameraProgress,
         startCamera.y + (faceCamera.y - startCamera.y) * cameraProgress,
         startCamera.z + (faceCamera.z - startCamera.z) * cameraProgress});
    camera->SetRotate({0.3f * (1.0f - cameraProgress), 0.0f, 0.0f});
    camera->Update();

    if (gameTransition >= 1.0f) {
      sceneManager->ChangeScene(SceneType::GamePlay);
    }
  }

  titleObject->Update();
  playerObject->Update();

  constexpr float kSkyRotationSpeed = 0.08f;
  skyBox->SetRotate(
      {0.0f, animationTime * kSkyRotationSpeed + kickProgress * 0.3f, 0.0f});
  skyBox->Update(camera.get());

  if (state == State::PressStart && fadeAlpha > 0.0f) {
    fadeAlpha = (std::max)(0.0f, fadeAlpha - 1.5f / 60.0f);
  }

  const float easedMenu = SmoothStep(menuTransition);
  const float promptPulse =
      0.55f + (std::sin(animationTime * 3.0f) + 1.0f) * 0.225f;
  const float promptAlpha = promptPulse * (1.0f - easedMenu);
  promptPanelSprite->SetColor({1.0f, 1.0f, 1.0f,
                               0.78f * (1.0f - easedMenu)});
  promptSprite->SetColor({1.0f, 1.0f, 1.0f, promptAlpha});

  promptPanelSprite->Update();
  promptSprite->Update();

  if (state == State::PressStart && fadeAlpha <= 0.0f &&
      input->TriggerKey(DIK_SPACE)) {
    state = State::OpeningMenu;
  }

  if (state == State::OpeningMenu) {
    menuTransition = (std::min)(1.0f, menuTransition + 1.35f / 60.0f);
    if (menuTransition >= 1.0f) {
      state = State::Menu;
    }
  } else if (state == State::Menu) {
    if (input->TriggerKey(DIK_UP) || input->TriggerKey(DIK_W) ||
        input->TriggerKey(DIK_DOWN) || input->TriggerKey(DIK_S)) {
      selectedItem = 1 - selectedItem;
    }

    if (input->TriggerKey(DIK_ESCAPE)) {
      state = State::ClosingMenu;
    } else if (input->TriggerKey(DIK_SPACE) ||
               input->TriggerKey(DIK_RETURN)) {
      state = selectedItem == 0 ? State::FadingToGame
                                : State::FadingToQuit;
    }
  }

  if (state == State::ClosingMenu) {
    menuTransition = (std::max)(0.0f, menuTransition - 3.0f / 60.0f);
    if (menuTransition <= 0.0f) {
      selectedItem = 0;
      state = State::PressStart;
    }
  }

  if (state == State::FadingToQuit) {
    fadeAlpha = (std::min)(1.0f, fadeAlpha + 2.2f / 60.0f);
    if (fadeAlpha >= 1.0f) {
      PostQuitMessage(0);
    }
  }

  const float targetSelectionY = selectedItem == 0 ? 357.0f : 457.0f;
  selectionY += (targetSelectionY - selectionY) * 0.22f;
  const float selectionPulse =
      0.72f + (std::sin(animationTime * 5.0f) + 1.0f) * 0.1f;

  const float menuVisibility = 1.0f - SmoothStep(gameTransition);
  menuShadeSprite->SetColor(
      {0.015f, 0.025f, 0.055f, 0.58f * easedMenu * menuVisibility});
  menuPanelSprite->SetSize({540.0f * easedMenu, 360.0f});
  menuPanelSprite->SetColor(
      {0.025f, 0.045f, 0.09f, 0.91f * easedMenu * menuVisibility});
  selectionGlowSprite->SetPosition({640.0f, selectionY});
  selectionGlowSprite->SetSize({430.0f + std::sin(animationTime * 4.0f) * 8.0f,
                                66.0f});
  selectionGlowSprite->SetColor(
      {0.18f, 0.65f, 1.0f,
       0.22f * selectionPulse * easedMenu * menuVisibility});
  selectionLineSprite->SetPosition({640.0f, selectionY + 32.0f});
  selectionLineSprite->SetColor(
      {0.35f, 0.82f, 1.0f, easedMenu * menuVisibility});

  const float selectedButtonAlpha = 0.92f * easedMenu * menuVisibility;
  const float normalButtonAlpha = 0.62f * easedMenu * menuVisibility;
  gameStartButtonSprite->SetColor(
      selectedItem == 0
          ? Vector::Vector4{0.72f, 0.9f, 1.0f, selectedButtonAlpha}
          : Vector::Vector4{0.65f, 0.72f, 0.8f, normalButtonAlpha});
  quitButtonSprite->SetColor(
      selectedItem == 1
          ? Vector::Vector4{0.72f, 0.9f, 1.0f, selectedButtonAlpha}
          : Vector::Vector4{0.65f, 0.72f, 0.8f, normalButtonAlpha});

  const float selectedScale =
      1.0f + (std::sin(animationTime * 5.0f) + 1.0f) * 0.012f;
  gameStartSprite->SetSize(
      selectedItem == 0 ? Vector::Vector2{316.0f * selectedScale,
                                          37.0f * selectedScale}
                        : Vector::Vector2{316.0f, 37.0f});
  quitSprite->SetSize(
      selectedItem == 1
          ? Vector::Vector2{122.0f * selectedScale, 43.0f * selectedScale}
          : Vector::Vector2{122.0f, 43.0f});
  gameStartSprite->SetColor({1.0f, 1.0f, 1.0f,
                             easedMenu * menuVisibility});
  quitSprite->SetColor(
      {1.0f, 1.0f, 1.0f, easedMenu * menuVisibility});

  SetTextColor(menuTitleText, 0.45f, 0.86f, 1.0f,
               0.8f * easedMenu * menuVisibility);
  SetTextColor(controlsText, 0.55f, 0.68f, 0.8f,
               0.72f * easedMenu * menuVisibility);

  menuShadeSprite->Update();
  menuPanelSprite->Update();
  selectionGlowSprite->Update();
  selectionLineSprite->Update();
  gameStartButtonSprite->Update();
  quitButtonSprite->Update();
  gameStartSprite->Update();
  quitSprite->Update();
  UpdateText(menuTitleText);
  UpdateText(controlsText);

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
  menuShadeSprite->Draw();
  menuPanelSprite->Draw();
  selectionGlowSprite->Draw();
  gameStartButtonSprite->Draw();
  quitButtonSprite->Draw();
  selectionLineSprite->Draw();
  gameStartSprite->Draw();
  quitSprite->Draw();
  for (const auto &sprite : menuTitleText) { sprite->Draw(); }
  for (const auto &sprite : controlsText) { sprite->Draw(); }
  fadeSprite->Draw();
}

void TitleScene::Finalize() {
  audioManager->Stop(titleBgmVoice);
  audioManager->Unload(&titleBgm);

  controlsText.clear();
  menuTitleText.clear();
  fadeSprite.reset();
  quitSprite.reset();
  gameStartSprite.reset();
  quitButtonSprite.reset();
  gameStartButtonSprite.reset();
  selectionLineSprite.reset();
  selectionGlowSprite.reset();
  menuPanelSprite.reset();
  menuShadeSprite.reset();
  promptSprite.reset();
  promptPanelSprite.reset();
  playerObject.reset();
  playerModel.reset();
  titleObject.reset();
  titleModel.reset();
  skyBox.reset();
  camera.reset();
}

void TitleScene::CreateBitmapText(
    std::vector<std::unique_ptr<Sprite>> &sprites, const char *text, float x,
    float y, float pixelSize) {
  constexpr float kCharacterWidth = 6.0f;
  for (size_t characterIndex = 0; text[characterIndex] != '\0';
       ++characterIndex) {
    const auto glyph = GetGlyph(text[characterIndex]);
    for (size_t row = 0; row < glyph.size(); ++row) {
      for (int column = 0; column < 5;) {
        if ((glyph[row] & (1 << (4 - column))) == 0) {
          ++column;
          continue;
        }

        const int runStart = column;
        while (column < 5 &&
               (glyph[row] & (1 << (4 - column))) != 0) {
          ++column;
        }
        const int runLength = column - runStart;

        auto sprite = std::make_unique<Sprite>();
        sprite->Initialize(SpriteCommon::GetInstance(), "resources/white.png");
        sprite->SetPosition(
            {x + (static_cast<float>(characterIndex) * kCharacterWidth +
                  static_cast<float>(runStart)) * pixelSize,
             y + static_cast<float>(row) * pixelSize});
        sprite->SetSize({pixelSize * static_cast<float>(runLength), pixelSize});
        sprite->SetColor({1.0f, 1.0f, 1.0f, 0.0f});
        sprites.push_back(std::move(sprite));
      }
    }
  }
}

void TitleScene::SetTextColor(
    std::vector<std::unique_ptr<Sprite>> &sprites, float red, float green,
    float blue, float alpha) {
  for (auto &sprite : sprites) {
    sprite->SetColor({red, green, blue, alpha});
  }
}

void TitleScene::UpdateText(
    std::vector<std::unique_ptr<Sprite>> &sprites) {
  for (auto &sprite : sprites) {
    sprite->Update();
  }
}
