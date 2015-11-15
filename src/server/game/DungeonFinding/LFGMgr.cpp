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

#include "Common.h"
#include "SharedDefines.h"
#include "DBCStores.h"
#include "DisableMgr.h"
#include "ObjectMgr.h"
#include "SocialMgr.h"
#include "LFGMgr.h"
#include "LFGScripts.h"
#include "LFGQueue.h"
#include "Group.h"
#include "Player.h"
#include "RBAC.h"
#include "GroupMgr.h"
#include "GameEventMgr.h"
#include "WorldSession.h"
#include "InstanceSaveMgr.h"

void LFGMgr::LoadTemplates()
{
    uint32 oldMSTime = getMSTime();

    uint32 count = 0;

    for (LFGDungeonEntry const* dungeon : sLFGDungeonStore)
    {
        AreaTriggerStruct const* at = sObjectMgr->GetMapEntranceTrigger(dungeon->MapID);
        if (!at)
        {
            TC_LOG_ERROR("sql.sql", "Failed to load dungeon template %s (Id: %u), cant find areatrigger for map %u", dungeon->Name_lang, dungeon->ID, dungeon->MapID);
            continue;
        }

        _templates[dungeon->ID] = std::make_pair(dungeon, at);
    }

    TC_LOG_INFO("server.loading", ">> Loaded %u lfg dungeon templates in %u ms", count, GetMSTimeDiffToNow(oldMSTime));
}

void LFGMgr::JoinLfg(Player* player, uint32 roles, std::vector<uint32> set, std::string comment, uint8 partyIndex, uint32 needs[3])
{
    uint8 members = 0;

    WorldPackets::LFG::LFGJoinResult result;

    if (Group* group = player->GetGroup())
    {
        for (Group::MemberSlot ref : group->GetMemberSlots())
        {
            if (Player* member = ObjectAccessor::FindConnectedPlayer(ref.guid))
            {
                if (CanPlayerJoin(member))
                {
                    // check for sets
                    ++members;
                }
                else
                    result.Result = GetPlayerJoinResult(member);
   
                member->GetSession()->SendPacket(result.Write());
            }
        }
    }
    else
    {
        if (!CanPlayerJoin(player))
            result.Result = GetPlayerJoinResult(player);
        else
        {
            
        }

        player->GetSession()->SendPacket(result.Write());
    }
}

LFGDungeonEntry const* LFGMgr::GetLfgDungeon(uint32 id) const
{
    LfgTemplates::const_iterator itr = _templates.find(id);
    if (itr != _templates.end())
        return &(itr->second.first);

    return nullptr;
}