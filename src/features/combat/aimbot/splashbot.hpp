namespace projectile_aim::detail {

struct splash_target_state {
  Vec3 mins{};
  Vec3 maxs{};
  Vec3 body{};
};

struct splash_candidate {
  Vec3 point{};
  Vec3 normal{};
  float falloff = 0.0f;
  int face_order = 0;
};

class splashbot final {
  static constexpr int max_cached_faces = 2048;
  static constexpr int max_candidates = 96;

  struct cached_face {
    Vec3 point{};
    Vec3 normal{};
    int order = 0;
  };

  std::array<cached_face, max_cached_faces> faces_{};
  int face_count_ = 0;
  int next_face_order_ = 0;
  std::string map_name_{};

  static Vec3 nearest_point(const Vec3& point, const Vec3& mins, const Vec3& maxs) {
    return {
      std::clamp(point.x, mins.x, maxs.x),
      std::clamp(point.y, mins.y, maxs.y),
      std::clamp(point.z, mins.z, maxs.z)
    };
  }

  static float length_squared(const Vec3& value) {
    return value.x * value.x + value.y * value.y + value.z * value.z;
  }

  static float hull_support(const Vec3& hull, const Vec3& normal) {
    return std::fabs(hull.x * normal.x) + std::fabs(hull.y * normal.y) +
      std::fabs(hull.z * normal.z);
  }

  static bool overlaps(const Vec3& point, const splash_target_state& target, float radius) {
    return point.x >= target.mins.x - radius && point.x <= target.maxs.x + radius &&
      point.y >= target.mins.y - radius && point.y <= target.maxs.y + radius &&
      point.z >= target.mins.z - radius && point.z <= target.maxs.z + radius;
  }

  static bool duplicate(const splash_candidate* candidates, int count,
    const Vec3& point, const Vec3& normal) {
    for (int index = 0; index < count; ++index) {
      if (distance_3d(candidates[index].point, point) <= 1.0f &&
          (candidates[index].normal.x * normal.x + candidates[index].normal.y * normal.y +
           candidates[index].normal.z * normal.z) > 0.99f) {
        return true;
      }
    }
    return false;
  }

  bool add_candidate(const splash_target_state& target, float radius, const Vec3& hull,
    const Vec3& surface_point, const Vec3& normal, int order,
    splash_candidate* out, int& count, int capacity) const {
    if (out == nullptr || count >= capacity || radius <= 0.0f || length_squared(normal) <= 0.0001f) {
      return false;
    }

    const Vec3 normalized = aimbot_normalize_vector(normal);
    const Vec3 point = surface_point + normalized * hull_support(hull, normalized);
    const Vec3 nearest = nearest_point(point, target.mins, target.maxs);
    const float distance = distance_3d(point, nearest);
    const Vec3 to_body = target.body - surface_point;
    const float facing = normalized.x * to_body.x + normalized.y * to_body.y + normalized.z * to_body.z;

    if (distance > radius || facing < -0.01f || duplicate(out, count, point, normalized)) {
      return false;
    }

    out[count++] = {point, normalized, std::clamp(1.0f - distance / radius, 0.0f, 1.0f), order};
    return true;
  }

  void reset_for_map() {
    face_count_ = 0;
    next_face_order_ = 0;
  }

  void ensure_map() {
    const char* raw_name = engine != nullptr ? engine->get_level_name() : nullptr;
    const std::string current = raw_name != nullptr ? std::string(raw_name) : std::string{};
    if (current != map_name_) {
      map_name_ = current;
      reset_for_map();
    }
  }

  void cache_face(const Vec3& point, const Vec3& normal) {
    if (face_count_ >= max_cached_faces || length_squared(normal) <= 0.0001f) {
      return;
    }

    const Vec3 normalized = aimbot_normalize_vector(normal);
    for (int index = 0; index < face_count_; ++index) {
      const cached_face& face = faces_[index];
      if (distance_3d(face.point, point) <= 1.0f &&
          face.normal.x * normalized.x + face.normal.y * normalized.y + face.normal.z * normalized.z > 0.99f) {
        return;
      }
    }

    faces_[face_count_++] = {point, normalized, next_face_order_++};
  }

public:
  static constexpr int candidate_limit = max_candidates;

  int collect_candidates(const splash_target_state& target, float radius, const Vec3& hull,
    splash_candidate* out, int capacity) {
    ensure_map();
    if (out == nullptr || capacity <= 0 || radius <= 0.0f || engine_trace == nullptr) {
      return 0;
    }

    int count = 0;
    const Vec3 center = (target.mins + target.maxs) * 0.5f;
    const Vec3 extents = (target.maxs - target.mins) * 0.5f;
    const std::array<Vec3, 15> probes{
      center,
      {target.mins.x, center.y, center.z}, {target.maxs.x, center.y, center.z},
      {center.x, target.mins.y, center.z}, {center.x, target.maxs.y, center.z},
      {center.x, center.y, target.mins.z}, {center.x, center.y, target.maxs.z},
      {center.x - extents.x, center.y - extents.y, center.z},
      {center.x - extents.x, center.y + extents.y, center.z},
      {center.x + extents.x, center.y - extents.y, center.z},
      {center.x + extents.x, center.y + extents.y, center.z},
      {center.x, center.y - extents.y, center.z - extents.z},
      {center.x, center.y + extents.y, center.z - extents.z},
      {center.x, center.y - extents.y, center.z + extents.z},
      {center.x, center.y + extents.y, center.z + extents.z}
    };
    const std::array<Vec3, 14> directions{
      Vec3{1.0f, 0.0f, 0.0f}, Vec3{-1.0f, 0.0f, 0.0f},
      Vec3{0.0f, 1.0f, 0.0f}, Vec3{0.0f, -1.0f, 0.0f},
      Vec3{0.0f, 0.0f, 1.0f}, Vec3{0.0f, 0.0f, -1.0f},
      aimbot_normalize_vector(Vec3{1.0f, 1.0f, 0.0f}),
      aimbot_normalize_vector(Vec3{1.0f, -1.0f, 0.0f}),
      aimbot_normalize_vector(Vec3{-1.0f, 1.0f, 0.0f}),
      aimbot_normalize_vector(Vec3{-1.0f, -1.0f, 0.0f}),
      aimbot_normalize_vector(Vec3{1.0f, 0.0f, 1.0f}),
      aimbot_normalize_vector(Vec3{-1.0f, 0.0f, 1.0f}),
      aimbot_normalize_vector(Vec3{0.0f, 1.0f, 1.0f}),
      aimbot_normalize_vector(Vec3{0.0f, -1.0f, 1.0f})
    };

    for (int index = 0; index < face_count_ && count < capacity; ++index) {
      const cached_face& face = faces_[index];
      if (overlaps(face.point, target, radius)) {
        add_candidate(target, radius, hull, face.point, face.normal, face.order, out, count, capacity);
      }
    }

    const float hull_length = std::sqrt(length_squared(hull));
    int probe_order = max_cached_faces;
    for (const Vec3& probe : probes) {
      for (const Vec3& direction : directions) {
        Vec3 start = probe;
        Vec3 end = probe + direction * (radius + hull_length + 2.0f);
        ray_t ray = engine_trace->init_ray(&start, &end);
        trace_filter filter{};
        engine_trace->init_world_trace_filter(&filter);
        trace_t trace{};
        engine_trace->trace_ray(&ray, MASK_SOLID, &filter, &trace);
        if (trace.fraction >= 1.0f || trace.start_solid || trace.all_solid || (trace.surface.flags & 0x0004u)) {
          continue;
        }

        if (trace.entity == nullptr) {
          cache_face(trace.endpos, trace.plane.normal);
        }
        add_candidate(target, radius, hull, trace.endpos, trace.plane.normal,
          probe_order++, out, count, capacity);
        if (count >= capacity) {
          break;
        }
      }
      if (count >= capacity) {
        break;
      }
    }

    std::sort(out, out + count, [](const splash_candidate& left, const splash_candidate& right) {
      if (left.falloff != right.falloff) {
        return left.falloff > right.falloff;
      }
      return left.face_order < right.face_order;
    });
    return count;
  }

  bool has_exposure(const splash_target_state& target, const splash_candidate& candidate, float radius) const {
    const Vec3 nearest = nearest_point(candidate.point, target.mins, target.maxs);
    if (distance_3d(candidate.point, nearest) > radius || engine_trace == nullptr) {
      return false;
    }

    constexpr float exposure_epsilon = 0.03125f;
    Vec3 start = candidate.point + candidate.normal * exposure_epsilon;
    Vec3 end = nearest;
    ray_t ray = engine_trace->init_ray(&start, &end);
    trace_filter filter{};
    engine_trace->init_world_trace_filter(&filter);
    trace_t trace{};
    engine_trace->trace_ray(&ray, MASK_SHOT & ~CONTENTS_HITBOX, &filter, &trace);
    return trace.fraction >= 1.0f && !trace.start_solid && !trace.all_solid;
  }
};

inline splashbot splashbot_instance{};

}
