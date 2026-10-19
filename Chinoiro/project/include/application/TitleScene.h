#pragma once

#include <memory>
#include <vector>

#include "AudioManager.h"
#include "BaseScene.h"

class Camera;
class DirectXCommon;
class Input;
class Model;
class ModelCommon;
class Object3d;
class SceneManager;
class SkyBox;
class Sprite;

class TitleScene final : public BaseScene {
public:
  TitleScene(SceneManager *sceneManager, Input *input, DirectXCommon *dxCommon,
             ModelCommon *modelCommon, AudioManager *audioManager);
  ~TitleScene() override;

  TitleScene(const TitleScene &) = delete;
  TitleScene &operator=(const TitleScene &) = delete;

  void Initialize() override;
  void Update() override;
  void Draw() override;
  void Finalize() override;

private:
  enum class State {
    PressStart,
    OpeningMenu,
    Menu,
    ClosingMenu,
    FadingToGame,
    FadingToQuit,
  };

  void CreateBitmapText(std::vector<std::unique_ptr<Sprite>> &sprites,
                        const char *text, float x, float y, float pixelSize);
  void SetTextColor(std::vector<std::unique_ptr<Sprite>> &sprites,
                    float red, float green, float blue, float alpha);
  void UpdateText(std::vector<std::unique_ptr<Sprite>> &sprites);

  SceneManager *sceneManager = nullptr;
  Input *input = nullptr;
  DirectXCommon *dxCommon = nullptr;
  ModelCommon *modelCommon = nullptr;
  AudioManager *audioManager = nullptr;

  std::unique_ptr<Camera> camera;
  std::unique_ptr<SkyBox> skyBox;
  std::unique_ptr<Model> titleModel;
  std::unique_ptr<Object3d> titleObject;
  std::unique_ptr<Model> playerModel;
  std::unique_ptr<Object3d> playerObject;
  std::unique_ptr<Sprite> promptPanelSprite;
  std::unique_ptr<Sprite> promptSprite;
  std::unique_ptr<Sprite> menuShadeSprite;
  std::unique_ptr<Sprite> menuPanelSprite;
  std::unique_ptr<Sprite> selectionGlowSprite;
  std::unique_ptr<Sprite> selectionLineSprite;
  std::unique_ptr<Sprite> gameStartButtonSprite;
  std::unique_ptr<Sprite> quitButtonSprite;
  std::unique_ptr<Sprite> gameStartSprite;
  std::unique_ptr<Sprite> quitSprite;
  std::unique_ptr<Sprite> fadeSprite;

  std::vector<std::unique_ptr<Sprite>> menuTitleText;
  std::vector<std::unique_ptr<Sprite>> controlsText;

  AudioManager::SoundData titleBgm{};
  IXAudio2SourceVoice *titleBgmVoice = nullptr;

  float animationTime = 0.0f;
  float fadeAlpha = 0.0f;
  float menuTransition = 0.0f;
  float gameTransition = 0.0f;
  float selectionY = 357.0f;
  int selectedItem = 0;
  State state = State::PressStart;
};
