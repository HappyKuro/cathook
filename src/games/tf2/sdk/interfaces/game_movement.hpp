/*
/^-----^\   data: 2026-03-30
V  o o  V  file: src/games/tf2/sdk/interfaces/game_movement.hpp
 |  Y  |   author: pupnoodle
  \ Q /
  / - \
  |    \
  |     \     )
  || (___\====
*/

#ifndef GAME_MOVEMENT_HPP
#define GAME_MOVEMENT_HPP

#include "core/types.hpp"

class Player;

class MoveData {
public:
  bool			m_bFirstRunOfFunctions : 1;
  bool			m_bGameCodeMovedPlayer : 1;

  int	m_nPlayerHandle;

  int				m_nImpulseCommand;
  Vec3			m_vecViewAngles;
  Vec3			m_vecAbsViewAngles;
  int				m_nButtons;
  int				m_nOldButtons;
  float			m_flForwardMove;
  float			m_flOldForwardMove;
  float			m_flSideMove;
  float			m_flUpMove;

  float			m_flMaxSpeed;
  float			m_flClientMaxSpeed;

  Vec3			m_vecVelocity;
  Vec3			m_vecAngles;
  Vec3			m_vecOldAngles;

  float			m_outStepHeight;
  Vec3			m_outWishVel;
  Vec3			m_outJumpVel;

  Vec3			m_vecConstraintCenter;
  float			m_flConstraintRadius;
  float			m_flConstraintWidth;
  float			m_flConstraintSpeedFactor;

  void			SetAbsOrigin( const Vec3 &vec );
  const Vec3	&GetAbsOrigin() const;

private:
  Vec3			m_vecAbsOrigin;
};

inline void MoveData::SetAbsOrigin(const Vec3& vec) {
  m_vecAbsOrigin = vec;
}

inline const Vec3& MoveData::GetAbsOrigin() const {
  return m_vecAbsOrigin;
}

class GameMovement {
public:
  bool process_movement(Player* player, MoveData* move) {
    if (this == nullptr || player == nullptr || move == nullptr) {
      return false;
    }

    void** vtable = *(void***)this;
    if (vtable == nullptr) {
      return false;
    }

    void (*process_movement_fn)(void*, Player*, MoveData*) = (void (*)(void*, Player*, MoveData*))vtable[2];
    if (process_movement_fn == nullptr) {
      return false;
    }

    process_movement_fn(this, player, move);
    return true;
  }
};

inline static GameMovement* game_movement;

#endif
