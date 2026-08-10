/*
/^-----^\   data: 2026-04-30
V  o o  V  file: src/features/automation/navbot/navbot_jobs.cpp
 |  Y  |   author: pupnoodle
  \ Q /
  / - \
  |    \
  |     \     )
  || (___\====
*/
#include "features/automation/navbot/navbot_jobs.hpp"
#include <utility>

namespace navbot
{

navbot_jobs::~navbot_jobs()
{
  stop();
}

void navbot_jobs::start()
{
  stop();

  running_ = true;
  worker_ = std::thread(&navbot_jobs::worker_main, this);
}

void navbot_jobs::stop()
{
  {
    std::scoped_lock lock(mutex_);
    running_ = false;
    for (auto& request : pending_requests_)
    {
      request.token.cancel();
    }
    active_token_.cancel();
  }
  cv_.notify_all();
  if (worker_.joinable())
  {
    worker_.join();
  }

  std::scoped_lock lock(mutex_);
  pending_requests_.clear();
  completed_results_.clear();
  active_token_ = {};
  active_generation_id_ = 0;
}

void navbot_jobs::cancel_generation(uint32_t generation_id)
{
  std::scoped_lock lock(mutex_);
  for (auto& request : pending_requests_)
  {
    if (request.request.generation_id == generation_id)
    {
      request.token.cancel();
    }
  }
  if (active_generation_id_ == generation_id)
  {
    active_token_.cancel();
  }
}

job_handle navbot_jobs::submit_path_request(const path_request& request,
  const navbot_mesh& mesh,
  const navbot_hazards& hazards,
  float current_time)
{
  auto handle = job_handle{
    next_job_id_.fetch_add(1),
    request.generation_id
  };

  auto hazard_snapshot = hazards;

  {
    std::scoped_lock lock(mutex_);
    for (auto& existing : pending_requests_)
    {
      existing.token.cancel();
    }
    pending_requests_.clear();
    active_token_.cancel();

    pending_requests_.push_back(path_job_request{
      handle,
      request,
      mesh,
      std::move(hazard_snapshot),
      cancellation_token{handle.id, std::make_shared<std::atomic_bool>(false)},
      current_time
    });
  }
  cv_.notify_one();

  return handle;
}

std::optional<path_job_result> navbot_jobs::poll_path_result()
{
  std::scoped_lock lock(mutex_);
  if (completed_results_.empty())
  {
    return std::nullopt;
  }

  auto result = completed_results_.front();
  completed_results_.erase(completed_results_.begin());
  return result;
}

void navbot_jobs::worker_main()
{
  while (true)
  {
    path_job_request request{};
    {
      std::unique_lock lock(mutex_);
      cv_.wait(lock, [this]
      {
        return !running_.load(std::memory_order_acquire) || !pending_requests_.empty();
      });
      if (!running_.load(std::memory_order_acquire))
      {
        return;
      }
      if (pending_requests_.empty())
      {
        continue;
      }
      request = std::move(pending_requests_.back());
      pending_requests_.clear();
      active_token_ = request.token;
      active_generation_id_ = request.request.generation_id;
    }

    if (request.token.is_canceled())
    {
      std::scoped_lock lock(mutex_);
      if (active_token_.id == request.token.id)
      {
        active_token_ = {};
        active_generation_id_ = 0;
      }
      continue;
    }

    auto result = solve_path_request(request.mesh, request.hazards, request.request, request.token, request.submitted_at);

    std::scoped_lock lock(mutex_);
    if (active_token_.id == request.token.id)
    {
      active_token_ = {};
      active_generation_id_ = 0;
    }
    completed_results_.clear();
    completed_results_.push_back(path_job_result{request.handle, std::move(result)});
  }
}

}
