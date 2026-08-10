/*
/^-----^\   data: 2026-03-30
V  o o  V  file: src/games/tf2/sdk/interfaces/steam_friends.hpp
 |  Y  |   author: pupnoodle
  \ Q /
  / - \
  |    \
  |     \     )
  || (___\====
*/

#ifndef STEAM_FRIENDS_HPP
#define STEAM_FRIENDS_HPP

enum EAccountType {
  k_EAccountTypeInvalid = 0,
  k_EAccountTypeIndividual = 1,
  k_EAccountTypeMultiseat = 2,
  k_EAccountTypeGameServer = 3,
  k_EAccountTypeAnonGameServer = 4,
  k_EAccountTypePending = 5,
  k_EAccountTypeContentServer = 6,
  k_EAccountTypeClan = 7,
  k_EAccountTypeChat = 8,
  k_EAccountTypeConsoleUser = 9,
  k_EAccountTypeAnonUser = 10,

  k_EAccountTypeMax
};

enum EFriendFlags {
  k_EFriendFlagNone			= 0x00,
  k_EFriendFlagBlocked		= 0x01,
  k_EFriendFlagFriendshipRequested	= 0x02,
  k_EFriendFlagImmediate		= 0x04,
  k_EFriendFlagClanMember		= 0x08,
  k_EFriendFlagOnGameServer	= 0x10,

  k_EFriendFlagRequestingFriendship = 0x80,
  k_EFriendFlagRequestingInfo = 0x100,
  k_EFriendFlagIgnored		= 0x200,
  k_EFriendFlagIgnoredFriend	= 0x400,

  k_EFriendFlagChatMember		= 0x1000,
  k_EFriendFlagAll			= 0xFFFF,
};

enum EUniverse {
  k_EUniverseInvalid = 0,
  k_EUniversePublic = 1,
  k_EUniverseBeta = 2,
  k_EUniverseInternal = 3,
  k_EUniverseDev = 4,

  k_EUniverseMax
};

class SteamID {
public:

  SteamID(int unAccountID, unsigned int unAccountInstance, EUniverse eUniverse, EAccountType eAccountType) {
    InstancedSet( unAccountID, unAccountInstance, eUniverse, eAccountType );
  }

  void InstancedSet(int unAccountID, int unInstance, EUniverse eUniverse, EAccountType eAccountType) {
    m_steamid.m_comp.m_unAccountID = unAccountID;
    m_steamid.m_comp.m_EUniverse = eUniverse;
    m_steamid.m_comp.m_EAccountType = eAccountType;
    m_steamid.m_comp.m_unAccountInstance = unInstance;
  }

  union SteamID_t {
    struct SteamIDComponent_t {
      int				m_unAccountID : 32;
      unsigned int		m_unAccountInstance : 20;
      unsigned int		m_EAccountType : 4;
      EUniverse			m_EUniverse : 8;
    } m_comp;

    unsigned long m_unAll64Bits;
  } m_steamid;
};

class SteamFriends {
public:

  bool has_friend(SteamID steam_friend_id, int friend_flags) {
    void** vtable = *(void***)this;

    bool (*has_friend_fn)(void*, SteamID, int) = (bool (*)(void*, SteamID, int))vtable[17];

    return has_friend_fn(this, steam_friend_id, friend_flags);
  }

  bool is_friend(int friend_id) {
    return has_friend({friend_id, 1, k_EUniversePublic, k_EAccountTypeIndividual}, k_EFriendFlagImmediate);
  }
};

inline static SteamFriends* steam_friends;

#endif
