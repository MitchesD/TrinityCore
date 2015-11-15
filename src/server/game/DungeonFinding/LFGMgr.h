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

#ifndef _LFGMGR_H
#define _LFGMGR_H

#include "DBCStructure.h"
#include "Field.h"
#include "LFG.h"
#include "LFGQueue.h"

class Group;
class Player;
class Quest;
class LFG;

enum LfgOptions
{
    LFG_OPTION_ENABLE_DUNGEON_FINDER             = 0x01,
    LFG_OPTION_ENABLE_RAID_BROWSER               = 0x02,
};

enum LFGMgrEnum
{
    LFG_TIME_ROLECHECK                           = 45 * IN_MILLISECONDS,
    LFG_TIME_BOOT                                = 120,
    LFG_TIME_PROPOSAL                            = 45,
    LFG_QUEUEUPDATE_INTERVAL                     = 15 * IN_MILLISECONDS,
    LFG_SPELL_DUNGEON_COOLDOWN                   = 71328,
    LFG_SPELL_DUNGEON_DESERTER                   = 71041,
    LFG_SPELL_LUCK_OF_THE_DRAW                   = 72221,
    LFG_GROUP_KICK_VOTES_NEEDED                  = 3
};

enum LfgFlags
{
    LFG_FLAG_UNK1                                = 0x1,
    LFG_FLAG_UNK2                                = 0x2,
    LFG_FLAG_SEASONAL                            = 0x4,
    LFG_FLAG_UNK3                                = 0x8
};

/// Determines the type of instance
enum LfgType
{
    LFG_TYPE_NONE                                = 0,
    LFG_TYPE_DUNGEON                             = 1,
    LFG_TYPE_RAID                                = 2,
    LFG_TYPE_QUEST                               = 4,
    LFG_TYPE_RANDOM                              = 6
};

/// Proposal states
enum LfgProposalState
{
    LFG_PROPOSAL_UPDATE                          = 0,
    LFG_PROPOSAL_FAILED                          = 1,
    LFG_PROPOSAL_SUCCESS                         = 2,
    LFG_PROPOSAL_SHOW                            = 3
};

/// Teleport errors
enum LfgTeleportError
{
    // 7 = "You can't do that right now" | 5 = No client reaction
    LFG_TELEPORTERROR_OK                         = 0,      // Internal use
    LFG_TELEPORTERROR_PLAYER_DEAD                = 1,
    LFG_TELEPORTERROR_FALLING                    = 2,
    LFG_TELEPORTERROR_IN_VEHICLE                 = 3,
    LFG_TELEPORTERROR_FATIGUE                    = 4,
    LFG_TELEPORTERROR_INVALID_LOCATION           = 6,
    LFG_TELEPORTERROR_CHARMING                   = 8       // FIXME - It can be 7 or 8 (Need proper data)
};

/// Queue join results
enum LfgJoinResult
{
    // 3 = No client reaction | 18 = "Rolecheck failed"
    LFG_JOIN_OK                                  = 0x00,   // Joined (no client msg)
    LFG_JOIN_FAILED                              = 0x1B,   // RoleCheck Failed
    LFG_JOIN_GROUPFULL                           = 0x1C,   // Your group is full
    LFG_JOIN_INTERNAL_ERROR                      = 0x1E,   // Internal LFG Error
    LFG_JOIN_NOT_MEET_REQS                       = 0x1F,   // You do not meet the requirements for the chosen dungeons
    LFG_JOIN_PARTY_NOT_MEET_REQS                 = 6,      // One or more party members do not meet the requirements for the chosen dungeons (FIXME)
    LFG_JOIN_MIXED_RAID_DUNGEON                  = 0x20,   // You cannot mix dungeons, raids, and random when picking dungeons
    LFG_JOIN_MULTI_REALM                         = 0x21,   // The dungeon you chose does not support players from multiple realms
    LFG_JOIN_DISCONNECTED                        = 0x22,   // One or more party members are pending invites or disconnected
    LFG_JOIN_PARTY_INFO_FAILED                   = 0x23,   // Could not retrieve information about some party members
    LFG_JOIN_DUNGEON_INVALID                     = 0x24,   // One or more dungeons was not valid
    LFG_JOIN_DESERTER                            = 0x25,   // You can not queue for dungeons until your deserter debuff wears off
    LFG_JOIN_PARTY_DESERTER                      = 0x26,   // One or more party members has a deserter debuff
    LFG_JOIN_RANDOM_COOLDOWN                     = 0x27,   // You can not queue for random dungeons while on random dungeon cooldown
    LFG_JOIN_PARTY_RANDOM_COOLDOWN               = 0x28,   // One or more party members are on random dungeon cooldown
    LFG_JOIN_TOO_MUCH_MEMBERS                    = 0x29,   // You can not enter dungeons with more that 5 party members
    LFG_JOIN_USING_BG_SYSTEM                     = 0x2A,   // You can not use the dungeon system while in BG or arenas
    LFG_JOIN_ROLE_CHECK_FAILED                   = 0x2B    // Role check failed, client shows special error
};

/// Role check states
enum LfgRoleCheckState
{
    LFG_ROLECHECK_DEFAULT                        = 0,      // Internal use = Not initialized.
    LFG_ROLECHECK_FINISHED                       = 1,      // Role check finished
    LFG_ROLECHECK_INITIALITING                   = 2,      // Role check begins
    LFG_ROLECHECK_MISSING_ROLE                   = 3,      // Someone didn't selected a role after 2 mins
    LFG_ROLECHECK_WRONG_ROLES                    = 4,      // Can't form a group with that role selection
    LFG_ROLECHECK_ABORTED                        = 5,      // Someone leave the group
    LFG_ROLECHECK_NO_ROLE                        = 6       // Someone selected no role
};

struct LfgRideTicket
{
    LfgRideTicket(ObjectGuid _guid, uint32 _id = 0, uint32 _type = 0, time_t _time = time_t(0)) : Guid(_guid), Id(_id), Type(_type), Time(_time) { }

    ObjectGuid Guid;
    uint32 Id;
    uint32 Type;
    time_t Time;
};

typedef std::unordered_map<uint32, std::pair<LFGDungeonEntry, AreaTriggerStruct>> LfgTemplates;
typedef std::map<uint32, LFG> LfgContainer;

class LFGMgr
{
    private:
        LFGMgr();
        ~LFGMgr();

    public:
        static LFGMgr* instance()
        {
            static LFGMgr instance;
            return &instance;
        }

        void LoadTemplates();
        // load rewards
        //

        void JoinLfg(Player* player, uint32 roles, std::vector<uint32> set, std::string comment, uint8 partyIndex, uint32 needs[3]);
        void LeaveLfg();
        void CreateLfg();
        void AddToQueue();

        bool CanPlayerJoin(Player* player);

        LfgJoinResult GetPlayerJoinResult(Player* player) const;
        LFGDungeonEntry const* GetLfgDungeon(uint32 id) const;

    private:
        LfgTemplates _templates;
        // rewards
        // queues
        // active lfgs
};

class LFG
{
    
};

#define sLFGMgr LFGMgr::instance()
#endif
