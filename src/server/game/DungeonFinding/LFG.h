/*
 * Copyright (C) 2008-2015 TrinityCore <http://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _LFG_H
#define _LFG_H

#include "Common.h"
#include "ObjectGuid.h"
#include "LFGPackets.h"

class Player;
class Group;

enum LFGEnum
{
    LFG_TANKS_NEEDED                             = 1,
    LFG_HEALERS_NEEDED                           = 1,
    LFG_DPS_NEEDED                               = 3
};

enum LfgRoles
{
    PLAYER_ROLE_NONE                             = 0x00,
    PLAYER_ROLE_LEADER                           = 0x01,
    PLAYER_ROLE_TANK                             = 0x02,
    PLAYER_ROLE_HEALER                           = 0x04,
    PLAYER_ROLE_DAMAGE                           = 0x08
};

enum LfgUpdateType
{
    LFG_UPDATETYPE_DEFAULT                       = 0,      // Internal Use
    LFG_UPDATETYPE_LEADER_UNK1                   = 1,      // FIXME: At group leave
    LFG_UPDATETYPE_ROLECHECK_ABORTED             = 4,
    LFG_UPDATETYPE_JOIN_QUEUE                    = 6,
    LFG_UPDATETYPE_ROLECHECK_FAILED              = 7,
    LFG_UPDATETYPE_REMOVED_FROM_QUEUE            = 8,
    LFG_UPDATETYPE_PROPOSAL_FAILED               = 9,
    LFG_UPDATETYPE_PROPOSAL_DECLINED             = 10,
    LFG_UPDATETYPE_GROUP_FOUND                   = 11,
    LFG_UPDATETYPE_ADDED_TO_QUEUE                = 13,
    LFG_UPDATETYPE_PROPOSAL_BEGIN                = 14,
    LFG_UPDATETYPE_UPDATE_STATUS                 = 15,
    LFG_UPDATETYPE_GROUP_MEMBER_OFFLINE          = 16,
    LFG_UPDATETYPE_GROUP_DISBAND_UNK16           = 17,     // FIXME: Sometimes at group disband
    LFG_UPDATETYPE_JOIN_QUEUE_INITIAL            = 24,
    LFG_UPDATETYPE_DUNGEON_FINISHED              = 25,
    LFG_UPDATETYPE_PARTY_ROLE_NOT_AVAILABLE      = 43,
    LFG_UPDATETYPE_JOIN_LFG_OBJECT_FAILED        = 45,
};

enum LfgState
{
    LFG_STATE_NONE,                                        // Not using LFG / LFR
    LFG_STATE_ROLECHECK,                                   // Rolecheck active
    LFG_STATE_QUEUED,                                      // Queued
    LFG_STATE_PROPOSAL,                                    // Proposal active
    //LFG_STATE_BOOT,                                      // Vote kick active
    LFG_STATE_DUNGEON = 5,                                 // In LFG Group, in a Dungeon
    LFG_STATE_FINISHED_DUNGEON,                            // In LFG Group, in a finished Dungeon
    LFG_STATE_RAIDBROWSER                                  // Using Raid finder
};

/// Instance lock types
enum LfgLockStatusType
{
    LFG_LOCKSTATUS_INSUFFICIENT_EXPANSION        = 1,
    LFG_LOCKSTATUS_TOO_LOW_LEVEL                 = 2,
    LFG_LOCKSTATUS_TOO_HIGH_LEVEL                = 3,
    LFG_LOCKSTATUS_TOO_LOW_GEAR_SCORE            = 4,
    LFG_LOCKSTATUS_TOO_HIGH_GEAR_SCORE           = 5,
    LFG_LOCKSTATUS_RAID_LOCKED                   = 6,
    LFG_LOCKSTATUS_ATTUNEMENT_TOO_LOW_LEVEL      = 1001,
    LFG_LOCKSTATUS_ATTUNEMENT_TOO_HIGH_LEVEL     = 1002,
    LFG_LOCKSTATUS_QUEST_NOT_COMPLETED           = 1022,
    LFG_LOCKSTATUS_MISSING_ITEM                  = 1025,
    LFG_LOCKSTATUS_NOT_IN_SEASON                 = 1031,
    LFG_LOCKSTATUS_MISSING_ACHIEVEMENT           = 1034
};

/// Answer state (Also used to check compatibilites)
enum LfgAnswer
{
    LFG_ANSWER_PENDING                           = -1,
    LFG_ANSWER_DENY                              = 0,
    LFG_ANSWER_AGREE                             = 1
};

struct LfgLockInfoData
{
    LfgLockInfoData(uint32 _lockStatus = 0, uint16 _requiredItemLevel = 0, float _currentItemLevel = 0) :
        lockStatus(_lockStatus), requiredItemLevel(_requiredItemLevel), currentItemLevel(_currentItemLevel) { }

    uint32 lockStatus;
    uint16 requiredItemLevel;
    float currentItemLevel;
};

struct LfgBlackListData
{
    LfgBlackListData(int32 _slot = 0, int32 _reason = 0, int32 _sub1 = 0, int32 _sub2 = 0) : 
        Slot(_slot), Reason(_reason), SubReason1(_sub1), SubReason2(_sub2), Guid() { }

    int32 Slot;
    int32 Reason;
    int32 SubReason1;
    int32 SubReason2;
    ObjectGuid Guid;
};

typedef std::set<uint32> LfgDungeonSet;
typedef std::map<uint32, LfgLockInfoData> LfgLockMap;
typedef std::map<ObjectGuid, LfgLockMap> LfgLockPartyMap;
typedef std::map<ObjectGuid, uint8> LfgRolesMap;
typedef std::map<ObjectGuid, ObjectGuid> LfgGroupsMap;
typedef std::map<ObjectGuid, LfgBlackListData> LfgBlackListMap;
//typedef std::map<ObjectGuid, LfgRideTicket> RideTicketMap;

std::string ConcatenateDungeons(LfgDungeonSet const& dungeons);
std::string GetRolesString(uint8 roles);
std::string GetStateString(LfgState state);

class LfgPlayerData
{
public:
    LfgPlayerData() : m_State(LFG_STATE_NONE), m_OldState(LFG_STATE_NONE), m_Team(0), m_Group(), m_Roles(0), m_Comment("") { }
    ~LfgPlayerData() { }

    // General
    void SetState(LfgState state);
    void RestoreState();
    void SetTeam(uint8 team);
    void SetGroup(ObjectGuid group);

    // Queue
    void SetRoles(uint8 roles);
    void SetComment(std::string const& comment);
    void SetSelectedDungeons(const LfgDungeonSet& dungeons);

    // General
    LfgState GetState() const;
    LfgState GetOldState() const;
    uint8 GetTeam() const;
    ObjectGuid GetGroup() const;

    // Queue
    uint8 GetRoles() const;
    std::string const& GetComment() const;
    LfgDungeonSet const& GetSelectedDungeons() const;

private:
    // General
    LfgState m_State;                                  ///< State if group in LFG
    LfgState m_OldState;                               ///< Old State - Used to restore state after failed Rolecheck/Proposal
    // Player
    uint8 m_Team;                                      ///< Player team - determines the queue to join
    ObjectGuid m_Group;                                ///< Original group of player when joined LFG

    // Queue
    uint8 m_Roles;                                     ///< Roles the player selected when joined LFG
    std::string m_Comment;                             ///< Player comment used when joined LFG
    LfgDungeonSet m_SelectedDungeons;                  ///< Selected Dungeons when joined LFG
};

enum LfgGroupEnum
{
    LFG_GROUP_MAX_KICKS = 3,
};

/**
Stores all lfg data needed about a group.
*/
class LfgGroupData
{
public:
    LfgGroupData() : m_State(LFG_STATE_NONE), m_OldState(LFG_STATE_NONE), m_Leader(), m_Dungeon(0), m_KicksLeft(LFG_GROUP_MAX_KICKS), m_VoteKickActive(false) { }
    ~LfgGroupData() { }

    bool IsLfgGroup();

    // General
    void SetState(LfgState state);
    void RestoreState();
    void AddPlayer(ObjectGuid guid);
    uint8 RemovePlayer(ObjectGuid guid);
    void RemoveAllPlayers();
    void SetLeader(ObjectGuid guid);

    // Dungeon
    void SetDungeon(uint32 dungeon);

    // VoteKick
    void DecreaseKicksLeft();

    // General
    LfgState GetState() const;
    LfgState GetOldState() const;
    GuidSet const& GetPlayers() const;
    uint8 GetPlayerCount() const;
    ObjectGuid GetLeader() const;

    // Dungeon
    uint32 GetDungeon(bool asId = true) const;

    // VoteKick
    uint8 GetKicksLeft() const;

    void SetVoteKick(bool active);
    bool IsVoteKickActive() const;

private:
    // General
    LfgState m_State;                                  ///< State if group in LFG
    LfgState m_OldState;                               ///< Old State
    ObjectGuid m_Leader;                               ///< Leader GUID
    GuidSet m_Players;                                 ///< Players in group
    // Dungeon
    uint32 m_Dungeon;                                  ///< Dungeon entry
    // Vote Kick
    uint8 m_KicksLeft;                                 ///< Number of kicks left
    bool m_VoteKickActive;
};

#endif
