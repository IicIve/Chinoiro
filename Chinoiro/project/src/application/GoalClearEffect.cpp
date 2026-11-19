#include "GoalClearEffect.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <numbers>

#include "Camera.h"
#include "DirectXCommon.h"
#include "ParticleManager.h"
#include "Player.h"
#include "Sprite.h"
#include "SpriteCommon.h"
#include "SrvManager.h"
#include "Window.h"

GoalClearEffect::~GoalClearEffect() = default;

void GoalClearEffect::Initialize(DirectXCommon *dxCommon,
                                 SrvManager *srvManager, Camera *camera) {
  assert(dxCommon);
  assert(srvManager);
  assert(camera);
  camera_ = camera;

  burstParticles_ = std::make_unique<ParticleManager>();
  burstParticles_->Initialize(
      dxCommon, srvManager, camera, "resources/circle.png",
      ParticleManager::PrimitiveType::Plane,
      ParticleManager::BlendMode::Add);
  burstParticles_->SetPlaneSize(0.5f, 0.5f);
  burstParticles_->SetUniformScaleRange(1.5f, 3.0f);
  burstParticles_->SetScaleVelocityRange(-1.4f, -0.7f);
  burstParticles_->SetColor({1.0f, 0.72f, 0.08f, 1.0f});
  burstParticles_->SetLifeTimeRange(0.75f, 1.25f);
  burstParticles_->SetSpeedRange(5.0f, 11.0f);
  burstParticles_->SetEmitCount(42);

  sparkParticles_ = std::make_unique<ParticleManager>();
  sparkParticles_->Initialize(
      dxCommon, srvManager, camera, "resources/white.png",
      ParticleManager::PrimitiveType::Plane,
      ParticleManager::BlendMode::Add);
  sparkParticles_->SetPlaneSize(0.1f, 0.5f);
  sparkParticles_->SetScaleRange(0.45f, 0.9f);
  sparkParticles_->SetLengthRange(1.2f, 2.8f);
  sparkParticles_->SetScaleVelocityRange(-0.45f, -0.2f);
  sparkParticles_->SetRotateVelocityRange(-7.0f, 7.0f);
  sparkParticles_->SetColor({1.0f, 0.96f, 0.65f, 1.0f});
  sparkParticles_->SetLifeTimeRange(0.55f, 1.0f);
  sparkParticles_->SetSpeedRange(8.0f, 15.0f);
  sparkParticles_->SetEmitCount(56);

  // 地面から噴き上がる炎。外側の橙と中心の黄を重ねる。
  flameParticles_ = std::make_unique<ParticleManager>();
  flameParticles_->Initialize(
      dxCommon, srvManager, camera, "resources/circle.png",
      ParticleManager::PrimitiveType::Plane,
      ParticleManager::BlendMode::Add);
  flameParticles_->SetPlaneSize(0.5f, 0.5f);
  flameParticles_->SetUniformScaleRange(0.9f, 1.8f);
  flameParticles_->SetScaleVelocityRange(-0.9f, -0.35f);
  flameParticles_->SetColor({1.0f, 0.2f, 0.01f, 0.95f});
  flameParticles_->SetLifeTimeRange(0.35f, 0.65f);
  flameParticles_->SetSpeedRange(7.0f, 12.0f);
  flameParticles_->SetDirectionRange({-0.35f, 0.8f, -0.12f},
                                     {0.35f, 1.0f, 0.12f});
  flameParticles_->SetEmitCount(10);

  flameCoreParticles_ = std::make_unique<ParticleManager>();
  flameCoreParticles_->Initialize(
      dxCommon, srvManager, camera, "resources/circle.png",
      ParticleManager::PrimitiveType::Plane,
      ParticleManager::BlendMode::Add);
  flameCoreParticles_->SetPlaneSize(0.5f, 0.5f);
  flameCoreParticles_->SetUniformScaleRange(0.5f, 1.0f);
  flameCoreParticles_->SetScaleVelocityRange(-0.8f, -0.3f);
  flameCoreParticles_->SetColor({1.0f, 0.82f, 0.08f, 1.0f});
  flameCoreParticles_->SetLifeTimeRange(0.25f, 0.5f);
  flameCoreParticles_->SetSpeedRange(9.0f, 15.0f);
  flameCoreParticles_->SetDirectionRange({-0.18f, 0.9f, -0.08f},
                                         {0.18f, 1.0f, 0.08f});
  flameCoreParticles_->SetEmitCount(7);

  fadeSprite_ = std::make_unique<Sprite>();
  fadeSprite_->Initialize(SpriteCommon::GetInstance(), "resources/white.png");
  fadeSprite_->SetPosition({0.0f, 0.0f});
  fadeSprite_->SetSize({static_cast<float>(Window::kClientWidth),
                        static_cast<float>(Window::kClientHeight)});
  fadeSprite_->SetColor({0.0f, 0.0f, 0.0f, 0.0f});
  fadeSprite_->Update();
}

void GoalClearEffect::Start(Player *player) {
  if (isPlaying_) {
    return;
  }
  assert(player);

  isPlaying_ = true;
  player_ = player;
  elapsedTime_ = 0.0f;
  flameTimer_ = 0.0f;
  nextFireworkIndex_ = 0;
  effectPosition_ = player_->GetPosition();
  effectPosition_.z -= 0.25f;
  cameraStartPosition_ = camera_->GetTranslate();
  playerStartRotationY_ = player_->GetRenderRotationY();

  EmitGroundFlames();
}

bool GoalClearEffect::Update(float deltaTime) {
  if (!isPlaying_) {
    return false;
  }

  elapsedTime_ += deltaTime;
  flameTimer_ += deltaTime;
  UpdateCameraAndPlayer();

  constexpr std::array<float, 3> kFireworkTimes = {0.12f, 0.52f, 0.92f};
  constexpr std::array<Vector::Vector3, 3> kFireworkOffsets = {
      Vector::Vector3{-4.5f, 5.5f, 0.0f},
      Vector::Vector3{4.0f, 7.0f, 0.0f},
      Vector::Vector3{0.0f, 5.0f, 0.0f},
  };
  while (nextFireworkIndex_ < kFireworkTimes.size() &&
         elapsedTime_ >= kFireworkTimes[nextFireworkIndex_]) {
    EmitFirework(kFireworkOffsets[nextFireworkIndex_]);
    ++nextFireworkIndex_;
  }

  if (elapsedTime_ < 1.45f && flameTimer_ >= 0.09f) {
    EmitGroundFlames();
    flameTimer_ = 0.0f;
  }

  const float fadeDuration = kDuration - kFadeStartTime;
  const float fadeAlpha = std::clamp(
      (elapsedTime_ - kFadeStartTime) / fadeDuration, 0.0f, 1.0f);
  fadeSprite_->SetColor({0.0f, 0.0f, 0.0f, fadeAlpha});
  fadeSprite_->Update();

  burstParticles_->Update(deltaTime);
  sparkParticles_->Update(deltaTime);
  flameParticles_->Update(deltaTime);
  flameCoreParticles_->Update(deltaTime);

  return elapsedTime_ >= kDuration;
}

void GoalClearEffect::DrawParticles() const {
  burstParticles_->Draw();
  sparkParticles_->Draw();
  flameParticles_->Draw();
  flameCoreParticles_->Draw();
}

void GoalClearEffect::DrawFade() const {
  if (isPlaying_) {
    fadeSprite_->Draw();
  }
}

void GoalClearEffect::EmitFirework(const Vector::Vector3 &offset) {
  const Vector::Vector3 position = {
      effectPosition_.x + offset.x,
      effectPosition_.y + offset.y,
      effectPosition_.z + offset.z,
  };
  burstParticles_->Emit(position);
  sparkParticles_->Emit(position);
}

void GoalClearEffect::EmitGroundFlames() {
  constexpr float kHorizontalOffset = 3.2f;
  Vector::Vector3 leftPosition = effectPosition_;
  leftPosition.x -= kHorizontalOffset;
  leftPosition.y -= 0.8f;

  Vector::Vector3 rightPosition = effectPosition_;
  rightPosition.x += kHorizontalOffset;
  rightPosition.y -= 0.8f;

  flameParticles_->Emit(leftPosition);
  flameParticles_->Emit(rightPosition);
  flameCoreParticles_->Emit(leftPosition);
  flameCoreParticles_->Emit(rightPosition);
}

void GoalClearEffect::UpdateCameraAndPlayer() {
  // 先にプレイヤーだけ正面へ振り向かせる。
  const float turnProgress = std::clamp(elapsedTime_ / 0.45f, 0.0f, 1.0f);
  const float smoothTurn =
      turnProgress * turnProgress * (3.0f - 2.0f * turnProgress);
  const float frontRotation = std::numbers::pi_v<float>;
  player_->SetRenderRotationY(
      playerStartRotationY_ +
      (frontRotation - playerStartRotationY_) * smoothTurn);

  // 花火を見せた後、顔の高さへ一気に寄って余韻を作る。
  const float cameraProgress =
      std::clamp((elapsedTime_ - 0.75f) / 0.75f, 0.0f, 1.0f);
  const float smoothCamera =
      cameraProgress * cameraProgress * (3.0f - 2.0f * cameraProgress);
  const Vector::Vector3 closeUpPosition = {
      effectPosition_.x,
      effectPosition_.y + 0.35f,
      effectPosition_.z - 3.75f,
  };

  Vector::Vector3 cameraPosition = {
      cameraStartPosition_.x +
          (closeUpPosition.x - cameraStartPosition_.x) * smoothCamera,
      cameraStartPosition_.y +
          (closeUpPosition.y - cameraStartPosition_.y) * smoothCamera,
      cameraStartPosition_.z +
          (closeUpPosition.z - cameraStartPosition_.z) * smoothCamera,
  };

  // 寄り切った後に小さく呼吸するような揺れを加える。
  if (cameraProgress >= 1.0f) {
    const float closeUpTime = elapsedTime_ - 1.5f;
    cameraPosition.y += std::sin(closeUpTime * 5.0f) * 0.06f;
    cameraPosition.z += std::sin(closeUpTime * 3.5f) * 0.08f;
  }

  camera_->SetTranslate(cameraPosition);
  camera_->SetRotate({0.0f, 0.0f, 0.0f});
  camera_->Update();
  player_->RefreshRenderObject();
}
