#pragma once
#include "alltheoffsets.h"
#include <cstdint>

// Flat aliases used across sober-cpp (maps to alltheoffsets.h dump).
namespace offsets {

inline constexpr std::uint64_t VISUAL_ENGINE_POINTER = VisualEngine::Pointer;
inline constexpr std::uint64_t FAKE_DATAMODEL_POINTER = FakeDataModel::Pointer;
inline constexpr std::uint64_t VISUAL_ENGINE_FAKE_DATAMODEL = VisualEngine::FakeDataModel;
inline constexpr std::uint64_t FAKE_DATAMODEL_REAL_DATAMODEL = FakeDataModel::RealDataModel;

inline constexpr std::uint64_t INSTANCE_CHILDREN_START = Instance::ChildrenStart;
inline constexpr std::uint64_t INSTANCE_CHILDREN_END = Instance::ChildrenEnd;
inline constexpr std::uint64_t INSTANCE_CLASS_DESCRIPTOR = Instance::ClassDescriptor;
inline constexpr std::uint64_t INSTANCE_CLASS_NAME = Instance::ClassName;
inline constexpr std::uint64_t PLAYERS_LOCAL_PLAYER = Players::LocalPlayer;
inline constexpr std::uint64_t PLAYER_CHARACTER = Player::Character;

inline constexpr std::uint64_t HUMANOID_WALK_SPEED = Humanoid::WalkSpeed;
inline constexpr std::uint64_t HUMANOID_WALK_SPEED_CHECK = Humanoid::WalkSpeedCheck;
inline constexpr std::uint64_t HUMANOID_JUMP_POWER = Humanoid::JumpPower;
inline constexpr std::uint64_t HUMANOID_USE_JUMP_POWER = Humanoid::UseJumpPower;

inline constexpr std::uint64_t BASEPART_PRIMITIVE = BasePart::Primitive;
inline constexpr std::uint64_t TEAM_BRICK_COLOR = Team::TeamColor;
inline constexpr std::uint64_t VISUAL_ENGINE_VIEW_MATRIX = VisualEngine::ViewMatrix;
inline constexpr std::uint64_t VISUAL_ENGINE_DIMENSIONS = VisualEngine::Dimensions;

inline constexpr std::uint64_t CHILD_ENTRY_SIZE = 0x10;
inline constexpr std::uint64_t MIN_PTR = 0x10000;
inline constexpr std::uint64_t HEAP_PTR_MIN = 0x00007F0000000000ULL;
inline constexpr std::uint64_t HEAP_PTR_MAX = 0x00007FFFFFFFFFFFULL;

inline constexpr std::uint64_t POS_OFFSET_CANDIDATES[] = {
    0xEC, 0xE4, 0x11C, 0x120, 0x148,
};

}  // namespace offsets
