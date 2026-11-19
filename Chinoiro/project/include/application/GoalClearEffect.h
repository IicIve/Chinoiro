#pragma once

#include <cstdint>
#include <memory>

#include "Vector.h"

class Camera;
class DirectXCommon;
class ParticleManager;
class Player;
class Sprite;
class SrvManager;

class GoalClearEffect final {
public:
  GoalClearEffect() = default;
  ~GoalClearEffect();

  GoalClearEffect(const GoalClearEffect &) = delete;
  GoalClearEffect &operator=(const GoalClearEffect &) = delete;

  void Initialize(DirectXCommon *dxCommon, SrvManager *srvManager,
                  Camera *camera);
  void Start(Player *player);
  bool Update(float deltaTime);
  void DrawParticles() const;
  void DrawFade() const;

  bool IsPlaying() const { return isPlaying_; }

private:
  void EmitFirework(const Vector::Vector3 &offset);
  void EmitGroundFlames();
  void UpdateCameraAndPlayer();

  std::unique_ptr<ParticleManager> burstParticles_;
  std::unique_ptr<ParticleManager> sparkParticles_;
  std::unique_ptr<ParticleManager> flameParticles_;
  std::unique_ptr<ParticleManager> flameCoreParticles_;
  std::unique_ptr<Sprite> fadeSprite_;

  Camera *camera_ = nullptr;
  Player *player_ = nullptr;

  Vector::Vector3 effectPosition_{};
  Vector::Vector3 cameraStartPosition_{};
  float playerStartRotationY_ = 0.0f;
  float elapsedTime_ = 0.0f;
  float flameTimer_ = 0.0f;
  uint32_t nextFireworkIndex_ = 0;
  bool isPlaying_ = false;

  static constexpr float kDuration = 2.8f;
  static constexpr float kFadeStartTime = 1.8f;
};
