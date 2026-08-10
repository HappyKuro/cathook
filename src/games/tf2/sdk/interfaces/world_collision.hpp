#ifndef WORLD_COLLISION_HPP
#define WORLD_COLLISION_HPP

#include <cstdint>

#include "core/types.hpp"
#include "games/tf2/sdk/interfaces/model_info.hpp"
#include "games/tf2/sdk/interfaces/utl_vector.hpp"
#include "games/tf2/sdk/interfaces/vphysics.hpp"

class IHandleEntity;
class IClientRenderable;
class IPhysicsEnvironment;
class IVPhysicsKeyHandler;

enum SolidType_t {
  SOLID_NONE = 0,
  SOLID_BSP = 1,
  SOLID_BBOX = 2,
  SOLID_OBB = 3,
  SOLID_OBB_YAW = 4,
  SOLID_CUSTOM = 5,
  SOLID_VPHYSICS = 6,
};

constexpr int FSOLID_CUSTOMRAYTEST = 0x0001;
constexpr int FSOLID_CUSTOMBOXTEST = 0x0002;
constexpr int FSOLID_NOT_SOLID = 0x0004;
constexpr int FSOLID_TRIGGER = 0x0008;

class ICollideable {
public:
  IHandleEntity* get_entity_handle() {
    auto** vtable = *reinterpret_cast<void***>(this);
    auto fn = reinterpret_cast<IHandleEntity* (*)(void*)>(vtable[0]);
    return fn(this);
  }

  const Vec3& obb_mins() const {
    auto** vtable = *reinterpret_cast<void***>(const_cast<ICollideable*>(this));
    auto fn = reinterpret_cast<const Vec3& (*)(const void*)>(vtable[3]);
    return fn(this);
  }

  const Vec3& obb_maxs() const {
    auto** vtable = *reinterpret_cast<void***>(const_cast<ICollideable*>(this));
    auto fn = reinterpret_cast<const Vec3& (*)(const void*)>(vtable[4]);
    return fn(this);
  }

  int get_collision_model_index() {
    auto** vtable = *reinterpret_cast<void***>(this);
    auto fn = reinterpret_cast<int (*)(void*)>(vtable[8]);
    return fn(this);
  }

  const model_t* get_collision_model() {
    auto** vtable = *reinterpret_cast<void***>(this);
    auto fn = reinterpret_cast<const model_t* (*)(void*)>(vtable[9]);
    return fn(this);
  }

  const Vec3& get_collision_origin() const {
    auto** vtable = *reinterpret_cast<void***>(const_cast<ICollideable*>(this));
    auto fn = reinterpret_cast<const Vec3& (*)(const void*)>(vtable[10]);
    return fn(this);
  }

  const Vec3& get_collision_angles() const {
    auto** vtable = *reinterpret_cast<void***>(const_cast<ICollideable*>(this));
    auto fn = reinterpret_cast<const Vec3& (*)(const void*)>(vtable[11]);
    return fn(this);
  }

  const matrix3x4& collision_to_world_transform() const {
    auto** vtable = *reinterpret_cast<void***>(const_cast<ICollideable*>(this));
    auto fn = reinterpret_cast<const matrix3x4& (*)(const void*)>(vtable[12]);
    return fn(this);
  }

  SolidType_t get_solid() const {
    auto** vtable = *reinterpret_cast<void***>(const_cast<ICollideable*>(this));
    auto fn = reinterpret_cast<SolidType_t (*)(const void*)>(vtable[13]);
    return fn(this);
  }

  int get_solid_flags() const {
    auto** vtable = *reinterpret_cast<void***>(const_cast<ICollideable*>(this));
    auto fn = reinterpret_cast<int (*)(const void*)>(vtable[14]);
    return fn(this);
  }

  bool is_solid() const {
    return get_solid() != SOLID_NONE && (get_solid_flags() & FSOLID_NOT_SOLID) == 0;
  }
};

class IStaticPropMgrClient {
public:
  bool is_static_prop(IHandleEntity* handle_entity) const {
    auto** vtable = *reinterpret_cast<void***>(const_cast<IStaticPropMgrClient*>(this));
    auto fn = reinterpret_cast<bool (*)(const void*, IHandleEntity*)>(vtable[2]);
    return fn(this, handle_entity);
  }

  ICollideable* get_static_prop_by_index(int prop_index) {
    auto** vtable = *reinterpret_cast<void***>(this);
    auto fn = reinterpret_cast<ICollideable* (*)(void*, int)>(vtable[4]);
    return fn(this, prop_index);
  }

  void get_all_static_props_in_aabb(const Vec3& mins, const Vec3& maxs, CUtlVector<ICollideable*>* output) {
    auto** vtable = *reinterpret_cast<void***>(this);
    auto fn = reinterpret_cast<void (*)(void*, const Vec3&, const Vec3&, CUtlVector<ICollideable*>*)>(vtable[11]);
    fn(this, mins, maxs, output);
  }
};

struct cbrushside_t {
  cplane_t* plane = nullptr;
  unsigned short surfaceIndex = 0;
  unsigned short bBevel = 0;
};

constexpr unsigned short NUMSIDES_BOXBRUSH = 0xFFFF;

struct cbrush_t {
  int contents = 0;
  unsigned short numsides = 0;
  unsigned short firstbrushside = 0;

  int get_box() const { return firstbrushside; }
  bool is_box() const { return numsides == NUMSIDES_BOXBRUSH; }
};

struct cboxbrush_t {
  Vec3_aligned mins{};
  Vec3_aligned maxs{};
  unsigned short surfaceIndex[6]{};
  unsigned short pad2[2]{};
};

template <typename T>
class CRangeValidatedArray {
public:
  T* Base() { return array; }
  const T* Base() const { return array; }
  T& operator[](int index) { return array[index]; }
  const T& operator[](int index) const { return array[index]; }

  T* array = nullptr;
};

template <typename T>
class CDiscardableArray {
public:
  int count = 0;
  int offset = -1;
  char filename[260]{};
  void* buffer_storage[5]{};
};

struct cmodel_t {
  Vec3 mins{};
  Vec3 maxs{};
  Vec3 origin{};
  int headnode = 0;
  vcollide_t vcollisionData{};
};

class CCollisionBSPData {
public:
  int numbrushsides = 0;
  CRangeValidatedArray<cbrushside_t> map_brushsides{};
  int numbrushes = 0;
  CRangeValidatedArray<cbrush_t> map_brushes{};
  int numboxbrushes = 0;
  CRangeValidatedArray<cboxbrush_t> map_boxbrushes{};
  int numcmodels = 0;
  CRangeValidatedArray<cmodel_t> map_cmodels{};
};

struct CDispCollTri {
  unsigned short tags = 0;
  Vec3 normal{};

  int get_vert(int index) const {
    return (tags >> (index * 5)) & 0x1f;
  }
};

class CDispCollTree {
public:
  void* vtable = nullptr;
  Vec3 mins{};
  Vec3 maxs{};
  Vec3 stab_dir{};
  int power = 0;
  int contents = 0;
  unsigned short surface_props[2]{};
  unsigned int flags = 0;
  unsigned int size = 0;
  CUtlVector<Vec3> verts{};
  CUtlVector<CDispCollTri> tris{};

  int get_face_count() const {
    return tris.Count();
  }
};

inline static IStaticPropMgrClient* static_prop_mgr = nullptr;
inline static CCollisionBSPData* collision_bsp_data = nullptr;
inline static CDispCollTree** disp_coll_trees = nullptr;
inline static int* disp_coll_tree_count = nullptr;

#endif
