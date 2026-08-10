/*
/^-----^\   data: 2026-04-30
V  o o  V  file: src/features/automation/navbot/navbot_hazards.cpp
 |  Y  |   author: pupnoodle
  \ Q /
  / - \
  |    \
  |     \     )
  || (___\====
*/
#include "features/automation/navbot/navbot_hazards.hpp"
#include <algorithm>

namespace navbot
{

namespace
{

bool hazard_affects_path_validity(const hazard_record& record)
{
  return record.policy != hazard_policy::soft_cost;
}

bool hazard_expired(const hazard_record& record, float current_time)
{
  return record.expire_time > 0.0f && record.expire_time <= current_time;
}

bool hazard_same_nav_edge(nav_edge_id left, nav_edge_id right)
{
  return left.from_area == right.from_area && left.connection_index == right.connection_index;
}

bool hazard_nav_edge_valid(nav_edge_id edge_id)
{
  return edge_id.from_area != 0;
}

uint64_t hazard_edge_key(nav_edge_id edge_id)
{
  return (static_cast<uint64_t>(edge_id.from_area) << 16u) | edge_id.connection_index;
}

}

void navbot_hazards::index_record(size_t index)
{
  const auto& record = records_[index];
  if (record.area_id.valid())
  {
    area_record_indexes_[record.area_id.value].push_back(index);
  }
  if (hazard_nav_edge_valid(record.edge_id))
  {
    edge_record_indexes_[hazard_edge_key(record.edge_id)].push_back(index);
  }
}

void navbot_hazards::rebuild_indexes()
{
  area_record_indexes_.clear();
  edge_record_indexes_.clear();
  for (size_t index = 0; index < records_.size(); ++index)
  {
    index_record(index);
  }
}

void navbot_hazards::clear()
{
  records_.clear();
  area_record_indexes_.clear();
  edge_record_indexes_.clear();
  ++generation_;
}

void navbot_hazards::clear_soft_costs()
{
  const auto old_size = records_.size();
  records_.erase(
    std::remove_if(records_.begin(), records_.end(), [](const hazard_record& record)
    {
      return record.policy == hazard_policy::soft_cost;
    }),
    records_.end());
  if (records_.size() != old_size)
  {
    rebuild_indexes();
  }
}

void navbot_hazards::update_expired(float current_time)
{
  auto old_size = records_.size();
  auto removed_path_validity_hazard = false;

  records_.erase(
    std::remove_if(records_.begin(), records_.end(), [current_time, &removed_path_validity_hazard](const hazard_record& record)
    {
      const auto expired = hazard_expired(record, current_time);
      if (expired && hazard_affects_path_validity(record))
      {
        removed_path_validity_hazard = true;
      }

      return expired;
    }),
    records_.end());

  if (records_.size() != old_size && removed_path_validity_hazard)
  {
    ++generation_;
  }
  if (records_.size() != old_size)
  {
    rebuild_indexes();
  }
}

void navbot_hazards::add_area_hazard(const hazard_record& record)
{
  records_.push_back(record);
  index_record(records_.size() - 1);
  if (hazard_affects_path_validity(record))
  {
    ++generation_;
  }
}

void navbot_hazards::add_edge_hazard(const hazard_record& record)
{
  records_.push_back(record);
  index_record(records_.size() - 1);
  if (hazard_affects_path_validity(record))
  {
    ++generation_;
  }
}

void navbot_hazards::add_transition_failure(nav_edge_id edge_id, float current_time, float duration)
{
  hazard_record record{};
  record.kind = hazard_kind::transition_failure;
  record.policy = hazard_policy::temporary_forbid;
  record.edge_id = edge_id;
  record.expire_time = current_time + duration;
  add_edge_hazard(record);
}

void navbot_hazards::add_crumb_blacklist(nav_edge_id edge_id, float current_time, float duration)
{
  if (duration <= 0.0f || !hazard_nav_edge_valid(edge_id))
  {
    return;
  }

  auto added_record = false;
  const auto expire_time = current_time + duration;
  auto changed_record = false;

  auto found_record = false;
  for (auto& record : records_)
  {
    if (record.kind != hazard_kind::crumb_blacklist
      || !hazard_same_nav_edge(record.edge_id, edge_id)
      || hazard_expired(record, current_time))
    {
      continue;
    }

    found_record = true;
    record.policy = hazard_policy::temporary_forbid;
    if (record.expire_time > 0.0f && record.expire_time < expire_time)
    {
      record.expire_time = expire_time;
      changed_record = true;
    }
  }

  if (!found_record)
  {
    hazard_record record{};
    record.kind = hazard_kind::crumb_blacklist;
    record.policy = hazard_policy::temporary_forbid;
    record.edge_id = edge_id;
    record.expire_time = expire_time;
    records_.push_back(record);
    added_record = true;
  }

  if (added_record || changed_record)
  {
    rebuild_indexes();
    ++generation_;
  }
}

bool navbot_hazards::is_area_blocked(nav_area_id area_id, float current_time) const
{
  const auto found = area_record_indexes_.find(area_id.value);
  if (found == area_record_indexes_.end())
  {
    return false;
  }

  for (const auto index : found->second)
  {
    const auto& record = records_[index];

    if (hazard_expired(record, current_time))
    {
      continue;
    }

    if (record.policy == hazard_policy::hard_block || record.policy == hazard_policy::temporary_forbid)
    {
      return true;
    }
  }

  return false;
}

bool navbot_hazards::is_edge_blocked(nav_edge_id edge_id, float current_time) const
{
  const auto found = edge_record_indexes_.find(hazard_edge_key(edge_id));
  if (found == edge_record_indexes_.end())
  {
    return false;
  }

  for (const auto index : found->second)
  {
    const auto& record = records_[index];

    if (hazard_expired(record, current_time))
    {
      continue;
    }

    if (record.policy == hazard_policy::temporary_forbid || record.policy == hazard_policy::hard_block)
    {
      return true;
    }
  }

  return false;
}

float navbot_hazards::area_cost(nav_area_id area_id, float current_time) const
{
  auto total_cost = 0.0f;
  const auto found = area_record_indexes_.find(area_id.value);
  if (found == area_record_indexes_.end())
  {
    return total_cost;
  }

  for (const auto index : found->second)
  {
    const auto& record = records_[index];

    if (hazard_expired(record, current_time))
    {
      continue;
    }

    if (record.policy == hazard_policy::soft_cost)
    {
      total_cost += record.cost;
    }
  }

  return total_cost;
}

bool navbot_hazards::has_active_world_hazard(float current_time) const
{
  for (const auto& record : records_)
  {
    if (hazard_expired(record, current_time))
    {
      continue;
    }

    if (record.kind == hazard_kind::transition_failure)
    {
      continue;
    }

    return true;
  }

  return false;
}

uint32_t navbot_hazards::generation() const
{
  return generation_;
}

const std::vector<hazard_record>& navbot_hazards::records() const
{
  return records_;
}

}
