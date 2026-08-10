/*
data: 2026-08-10
file: src/features/visuals/radar/radar.hpp
author: HappyKuro
*/
#include "features/visuals/radar/radar.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <unordered_map>

#include "imgui/imgui.h"
#include "mono/window_chrome.hpp"

#include "core/entity_cache.hpp"
#include "core/types.hpp"
#include "features/menu/config.hpp"
#include "features/menu/menu.hpp"
#include "features/visuals/esp/esp.hpp"
#include "games/tf2/sdk/entities/entity.hpp"
#include "games/tf2/sdk/entities/player.hpp"
#include "games/tf2/sdk/interfaces/engine.hpp"
#include "games/tf2/sdk/interfaces/entity_list.hpp"
#include "games/tf2/sdk/interfaces/global_vars.hpp"

namespace radar
{

namespace
{

constexpr float radar_pi = 3.14159265358979323846f;
constexpr float radar_deg2rad = radar_pi / 180.0f;
constexpr int atlas_class_glyph_row = 5;
constexpr int atlas_team_disc_column = 11;
constexpr int atlas_team_disc_first_row = 6;
constexpr int tf_class_count = 9;
constexpr float track_lifetime = 8.0f;

struct player_track
{
  Vec3 origin{};
  tf_team team = tf_team::UNKNOWN;
  int tf_class = 0;
  float last_seen = 0.0f;
  std::uint32_t snapshot_serial = 0;
};

std::unordered_map<int, player_track> g_player_tracks{};
std::string g_track_level_name{};
std::uint32_t g_last_snapshot_serial = 0;

bool valid_team(const tf_team team)
{
  return team == tf_team::RED || team == tf_team::BLU;
}

int radar_size()
{
  return std::clamp(config.visuals.radar.size, 100, 600);
}

float radar_zoom()
{
  const float zoom = config.visuals.radar.zoom;
  return std::isfinite(zoom) ? std::clamp(zoom, 5.0f, 50.0f) : 10.0f;
}

float radar_icon_size()
{
  return static_cast<float>(std::clamp(config.visuals.radar.icon_size, 10, 40)) * mono::window_scale();
}

float radar_position(const float value, const float fallback)
{
  return std::isfinite(value) ? value : fallback;
}

void reset_tracks_on_level_change()
{
  if (engine == nullptr) {
    return;
  }

  const char* level_name = engine->get_level_name();
  const std::string current = level_name != nullptr ? std::string{ level_name } : std::string{};
  if (current != g_track_level_name) {
    g_track_level_name = current;
    g_player_tracks.clear();
    g_last_snapshot_serial = 0;
  }
}

void refresh_tracks(Player* localplayer, const float now)
{
  if (localplayer == nullptr) {
    return;
  }

  const entity_cache_snapshot& snapshot = entity_cache_current_snapshot();
  if (snapshot.serial == g_last_snapshot_serial) {
    return;
  }
  g_last_snapshot_serial = snapshot.serial;

  const int local_index = localplayer->get_index();
  for (const entity_cache_player_entry& entry : snapshot.players) {
    if (entry.index == local_index || !entry.alive || entry.dormant || !valid_team(entry.team)) {
      continue;
    }

    g_player_tracks[entry.index] = {
      .origin = entry.origin,
      .team = entry.team,
      .tf_class = entry.player_class,
      .last_seen = now,
      .snapshot_serial = snapshot.serial
    };
  }
}

void forget_dead_tracks()
{
  if (entity_list == nullptr) {
    return;
  }

  for (auto it = g_player_tracks.begin(); it != g_player_tracks.end();) {
    Entity* entity = entity_list->entity_from_index(static_cast<unsigned int>(it->first));
    if (entity != nullptr && entity->get_class_id() == class_id::PLAYER &&
        !static_cast<Player*>(entity)->is_alive()) {
      it = g_player_tracks.erase(it);
    } else {
      ++it;
    }
  }
}

void forget_stale_tracks(const float now)
{
  for (auto it = g_player_tracks.begin(); it != g_player_tracks.end();) {
    if (now < it->second.last_seen || now - it->second.last_seen > track_lifetime) {
      it = g_player_tracks.erase(it);
    } else {
      ++it;
    }
  }
}

ImVec2 world_to_radar_offset(
  const Vec3& origin,
  Player* localplayer,
  const float field_size,
  const float marker_radius)
{
  if (localplayer == nullptr || engine == nullptr) {
    return { 0.0f, 0.0f };
  }

  const Vec3 local_origin = localplayer->get_origin();
  Vec3 view_angles{};
  engine->get_view_angles(view_angles);

  const float dx = -((origin.x - local_origin.x) / radar_zoom());
  const float dy = (origin.y - local_origin.y) / radar_zoom();
  const float yaw = radar_deg2rad * view_angles.y + radar_pi / 2.0f;
  const float x = dx * std::cos(yaw) - dy * std::sin(yaw);
  const float y = dx * std::sin(yaw) + dy * std::cos(yaw);
  const float half = std::max(field_size * 0.5f - marker_radius, 0.0f);
  return { std::clamp(x, -half, half), std::clamp(y, -half, half) };
}

bool draw_class_icon(
  ImDrawList* draw_list,
  const ImVec2& center,
  const tf_team team,
  const int tf_class,
  const int alpha)
{
  if (tf_class <= 0 || tf_class > tf_class_count || !valid_team(team) || !atlas_texture_ready()) {
    return false;
  }

  const int team_index = static_cast<int>(team) - static_cast<int>(tf_team::RED);
  const float size = radar_icon_size();
  const ImU32 tint = IM_COL32(255, 255, 255, alpha);
  draw_shared_atlas_tile(
    draw_list, atlas_team_disc_column, atlas_team_disc_first_row + team_index, center, size, tint);
  draw_shared_atlas_tile(draw_list, tf_class - 1, atlas_class_glyph_row, center, size, tint);
  return true;
}

void draw_radar_field(ImDrawList* draw_list, const ImVec2& top_left, const float field_size, const float scale)
{
  const ImVec2 center{ top_left.x + field_size * 0.5f, top_left.y + field_size * 0.5f };
  const int rings = std::clamp(config.visuals.radar.range_rings, 0, 8);
  if (rings > 0) {
    const float step = field_size * 0.5f / static_cast<float>(rings);
    for (int ring = 1; ring <= rings; ++ring) {
      draw_list->AddCircle(center, step * static_cast<float>(ring), IM_COL32(100, 100, 100, 70), 48, 1.0f);
    }
  }

  if (config.visuals.radar.axis_lines) {
    const ImU32 axis_color = IM_COL32(100, 100, 100, 90);
    draw_list->AddLine({ top_left.x, center.y }, { top_left.x + field_size, center.y }, axis_color, 1.0f);
    draw_list->AddLine({ center.x, top_left.y }, { center.x, top_left.y + field_size }, axis_color, 1.0f);
  }

  const float cross = 10.0f * scale;
  const ImU32 cross_color = IM_COL32(100, 100, 100, 150);
  draw_list->AddLine({ center.x - cross, center.y }, { center.x + cross, center.y }, cross_color, 1.0f);
  draw_list->AddLine({ center.x, center.y - cross }, { center.x, center.y + cross }, cross_color, 1.0f);
}

void draw_blip(
  ImDrawList* draw_list,
  const player_track& track,
  Player* localplayer,
  const ImVec2& top_left,
  const float field_size,
  const bool live)
{
  const tf_team local_team = localplayer->get_team();
  if (!valid_team(track.team) || !valid_team(local_team)) {
    return;
  }

  const bool is_enemy = track.team != local_team;
  if ((is_enemy && !config.visuals.radar.show_enemies) ||
      (!is_enemy && !config.visuals.radar.show_teammates)) {
    return;
  }

  const float radius = radar_icon_size() * 0.5f;
  const ImVec2 offset = world_to_radar_offset(track.origin, localplayer, field_size, radius);
  const ImVec2 center{
    top_left.x + field_size * 0.5f + offset.x,
    top_left.y + field_size * 0.5f + offset.y
  };
  const int alpha = live ? 255 : 110;

  if (config.visuals.radar.use_icons && draw_class_icon(draw_list, center, track.team, track.tf_class, alpha)) {
    return;
  }

  const ImU32 color = is_enemy ? IM_COL32(255, 50, 50, alpha) : IM_COL32(50, 150, 255, alpha);
  draw_list->AddCircleFilled(center, radius, color);
  draw_list->AddCircle(center, radius, IM_COL32(0, 0, 0, alpha), 0, 1.5f);
}

void draw_localplayer_blip(ImDrawList* draw_list, Player* localplayer, const ImVec2& top_left, const float field_size)
{
  const ImVec2 center{ top_left.x + field_size * 0.5f, top_left.y + field_size * 0.5f };
  if (config.visuals.radar.use_icons &&
      draw_class_icon(
        draw_list, center, localplayer->get_team(), static_cast<int>(localplayer->get_tf_class()), 255)) {
    return;
  }

  const float radius = radar_icon_size() * 0.3f;
  draw_list->AddCircleFilled(center, radius, IM_COL32(255, 255, 255, 255));
  draw_list->AddCircle(center, radius, IM_COL32(0, 0, 0, 255), 0, 1.5f);
}

}

void draw_radar()
{
  if (!config.visuals.radar.enabled || entity_list == nullptr || engine == nullptr ||
      global_vars == nullptr || !engine->is_connected() || !engine->is_in_game()) {
    return;
  }

  Player* localplayer = entity_list->get_localplayer();
  if (localplayer == nullptr || !localplayer->is_alive()) {
    return;
  }

  reset_tracks_on_level_change();
  refresh_tracks(localplayer, global_vars->curtime);
  forget_dead_tracks();
  forget_stale_tracks(global_vars->curtime);

  const float scale = mono::window_scale();
  const float header_height = mono::window_header_height();
  const float field_size = static_cast<float>(radar_size()) * scale;
  const float width = field_size;
  const float height = header_height + field_size;
  const ImVec2 configured_position{
    radar_position(config.visuals.radar.x, 100.0f),
    radar_position(config.visuals.radar.y, 100.0f)
  };

  ImGui::SetNextWindowPos(configured_position, ImGuiCond_Always);
  ImGui::SetNextWindowSize({ width, height }, ImGuiCond_Always);
  ImGui::SetNextWindowBgAlpha(0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0.0f, 0.0f });
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
  ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
    ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
    ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoScrollbar;
  if (!menu_focused) {
    flags |= ImGuiWindowFlags_NoInputs;
  }

  if (ImGui::Begin("mono_overlay_radar", nullptr, flags)) {
    const ImVec2 position = ImGui::GetWindowPos();
    const ImVec2 end{ position.x + width, position.y + height };
    const ImVec2 field_top_left{ position.x, position.y + header_height };
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    draw_list->AddRectFilled(position, end, ImGui::GetColorU32(ImGuiCol_WindowBg), 4.0f * scale);
    mono::draw_window_header(*draw_list, position, width, { "radar" });

    draw_list->PushClipRect(field_top_left, end, true);
    draw_radar_field(draw_list, field_top_left, field_size, scale);
    for (const auto& [index, track] : g_player_tracks) {
      (void)index;
      draw_blip(
        draw_list,
        track,
        localplayer,
        field_top_left,
        field_size,
        track.snapshot_serial == g_last_snapshot_serial);
    }
    draw_localplayer_blip(draw_list, localplayer, field_top_left, field_size);
    draw_list->PopClipRect();

    draw_list->AddRect(
      { position.x + 0.5f * scale, position.y + 0.5f * scale },
      { end.x - 0.5f * scale, end.y - 0.5f * scale },
      ImGui::GetColorU32(ImGuiCol_CheckMark), 4.0f * scale, 0, scale);

    static ImVec2 drag_offset{};
    static bool dragging{};
    const ImVec2 header_end{ end.x, position.y + header_height };
    if (menu_focused && ImGui::IsMouseHoveringRect(position, header_end) &&
        ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
      dragging = true;
      const ImVec2 mouse = ImGui::GetMousePos();
      drag_offset = { mouse.x - position.x, mouse.y - position.y };
    }
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
      dragging = false;
    }
    if (dragging) {
      const ImVec2 mouse = ImGui::GetMousePos();
      ImGui::SetWindowPos({ mouse.x - drag_offset.x, mouse.y - drag_offset.y });
    }

    config.visuals.radar.x = ImGui::GetWindowPos().x;
    config.visuals.radar.y = ImGui::GetWindowPos().y;
    ImGui::Dummy({ width, height });
  }
  ImGui::End();
  ImGui::PopStyleVar(2);
}

}
