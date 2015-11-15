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

#include "LFGMgr.h"
#include "LFGPackets.h"
#include "ObjectMgr.h"
#include "Group.h"
#include "Player.h"
#include "Opcodes.h"
#include "WorldPacket.h"
#include "WorldSession.h"

/*void BuildLfgBlackListBlock(WorldPackets::LFG::LFGBlackList& blackList, Player* player, LfgBlackListMap const& lock)
{
    blackList.PlayerGuid = player->GetGUID();
    for (LfgBlackListMap::const_iterator iterator = lock.begin(); iterator != lock.end(); ++iterator)
    {
        WorldPackets::LFG::LFGBlackListSlot slot;
        
        slot.Slot = iterator->second.Slot;
        slot.Reason = iterator->second.Reason;
        slot.SubReason1 = iterator->second.SubReason1;
        slot.SubReason2 = iterator->second.SubReason2;
        blackList.Slot.push_back(slot);
    }
}*/

void BuildLfgBlackListBlock(WorldPackets::LFG::LFGBlackList& blackList, ObjectGuid guid, LfgLockMap const& lock)
{
    blackList.PlayerGuid = guid;
    for (LfgLockMap::const_iterator itr = lock.begin(); itr != lock.end(); ++itr)
    {
        WorldPackets::LFG::LFGBlackListSlot slot;

        slot.Slot = itr->first;
        slot.Reason = itr->second.lockStatus;
        slot.SubReason1 = 0;
        slot.SubReason2 = 0;
        blackList.Slot.push_back(slot);
    }
}

//
//void BuildPartyLockDungeonBlock(WorldPacket& data, LfgLockPartyMap const& lockMap)
//{
//    data << uint8(lockMap.size());
//    for (LfgLockPartyMap::const_iterator it = lockMap.begin(); it != lockMap.end(); ++it)
//    {
//        data << it->first;                              // Player guid
//        BuildPlayerLockDungeonBlock(data, it->second);
//    }
//}
//

// Not at all properly implemented. Just a PH
void BuildRideTicket(WorldPackets::LFG::RideTicket& ticket, Player* player, RideTicket const& rideTicket)
{
    ticket.RequesterGuid = rideTicket.Guid;
    ticket.Id = rideTicket.Id;
    ticket.Type = rideTicket.Type;
    ticket.Time = rideTicket.Time;
}

void BuildReward(WorldPackets::LFG::LfgPlayerQuestReward& reward, Quest const* quest, Player* player)
{
    uint8 rewCount = quest->GetRewItemsCount() + quest->GetRewCurrencyCount();

    reward.RewardMoney = player->GetQuestMoneyReward(quest);
    reward.RewardXP = player->GetQuestXPReward(quest);
    reward.Mask = rewCount;

    if (rewCount)
    {
        for (uint8 i = 0; i < QUEST_REWARD_CURRENCY_COUNT; ++i)
        {
            if (uint32 currencyId = quest->RewardCurrencyId[i])
            {
                WorldPackets::LFG::LfgPlayerQuestRewardCurrency currency;
                currency.CurrencyID = currencyId;
                currency.Quantity = quest->RewardCurrencyCount[i];
                reward.Currency.push_back(currency);
            }
        }

        for (uint8 i = 0; i < QUEST_REWARD_ITEM_COUNT; ++i)
        {
            if (uint32 itemId = quest->RewardItemId[i])
            {
                WorldPackets::LFG::LfgPlayerQuestRewardItem item;
                item.ItemID = itemId;
                item.Quantity = quest->RewardItemCount[i];
                reward.Item.push_back(item);
            }
        }

        /// @todo: bonus currency
    }
}

void WorldSession::HandleDfJoinOpcode(WorldPackets::LFG::DfJoin& packet)
{
    if (!sLFGMgr->isOptionEnabled(LFG_OPTION_ENABLE_DUNGEON_FINDER | LFG_OPTION_ENABLE_RAID_BROWSER) ||
        (GetPlayer()->GetGroup() && GetPlayer()->GetGroup()->GetLeaderGUID() != GetPlayer()->GetGUID() &&
        (GetPlayer()->GetGroup()->GetMembersCount() == MAX_GROUP_SIZE || !GetPlayer()->GetGroup()->isLFGGroup())))
    {
        return;
    }

    if (packet.Slots.empty())
        return;

    // some checks
    LfgDungeonSet newDungeons;
    for (uint32 i = 0; i < packet.Slots.size(); ++i)
    {
        uint32 dungeon = packet.Slots[i];
        dungeon &= 0x00FFFFFF;
        if (dungeon)
            newDungeons.insert(dungeon);
    }

    sLFGMgr->JoinLfg(GetPlayer(), packet.Roles, newDungeons, packet.Comment, packet.PartyIndex, packet.Needs);
}

void WorldSession::HandleLfgLeaveOpcode(WorldPackets::LFG::DfLeave& packet)
{
    Group* group = GetPlayer()->GetGroup();
    ObjectGuid groupGuid = group ? group->GetGUID() : packet.Ticket.RequesterGuid;

    TC_LOG_DEBUG("lfg", "CMSG_DF_LEAVE %s in group: %u sent guid %s.", GetPlayerInfo().c_str(), group ? 1 : 0, groupGuid.ToString().c_str());

    // Check cheating - only leader can leave the queue
    if (!group || group->GetLeaderGUID() == packet.Ticket.RequesterGuid)
        sLFGMgr->LeaveLfg(groupGuid);
}

void WorldSession::HandleLfgProposalResultOpcode(WorldPackets::LFG::DfProposalResponse& packet)
{
    TC_LOG_DEBUG("lfg", "CMSG_LFG_PROPOSAL_RESULT %s proposal: %u accept: %u", GetPlayerInfo().c_str(), packet.ProposalID, packet.Accepted ? 1 : 0);

    sLFGMgr->UpdateProposal(packet.ProposalID, GetPlayer()->GetGUID(), packet.Accepted);
}

void WorldSession::HandleLfgSetRolesOpcode(WorldPackets::LFG::DfSetRoles& packet)
{
    Group* group = GetPlayer()->GetGroup();    
    if (!group)
    {
        TC_LOG_DEBUG("lfg", "CMSG_DF_SET_ROLES %s Not in group", GetPlayerInfo().c_str());
        return;
    }

    ObjectGuid groupGuid = group->GetGUID();

    TC_LOG_DEBUG("lfg", "CMSG_DF_SET_ROLES: Group %s, Player %s, Roles: %u", groupGuid.ToString().c_str(), GetPlayerInfo().c_str(), packet.RolesDesired);

    sLFGMgr->UpdateRoleCheck(groupGuid, GetPlayer()->GetGUID(), packet.RolesDesired);
}

void WorldSession::HandleLfgSetCommentOpcode(WorldPackets::LFG::DfSetComment& packet)
{
    TC_LOG_DEBUG("lfg", "CMSG_DF_SET_COMMENT %s comment: %s", GetPlayerInfo().c_str(), packet.Comment.c_str());

    sLFGMgr->SetComment(GetPlayer()->GetGUID(), packet.Comment);
}

void WorldSession::HandleLfgSetBootVoteOpcode(WorldPackets::LFG::DfBootPlayerVote& packet)
{
    ObjectGuid guid = GetPlayer()->GetGUID();
    TC_LOG_DEBUG("lfg", "CMSG_DF_BOOT_PLAYER_VOTE %s agree: %u", GetPlayerInfo().c_str(), packet.Vote ? 1 : 0);

    sLFGMgr->UpdateBoot(guid, packet.Vote);
}

void WorldSession::HandleLfgTeleportOpcode(WorldPackets::LFG::DfTeleport& packet)
{
    TC_LOG_DEBUG("lfg", "CMSG_DF_TELEPORT %s out: %u", GetPlayerInfo().c_str(), packet.TeleportOut ? 1 : 0);

    sLFGMgr->TeleportPlayer(GetPlayer(), packet.TeleportOut, true);
}

void WorldSession::HandleDFGetSystemInfo(WorldPackets::LFG::DfGetSystemInfo& packet)
{
    TC_LOG_DEBUG("lfg", "CMSG_DF_GET_SYSTEM_INFO %s for %s", GetPlayerInfo().c_str(), (packet.Player ? "player" : "party"));

    if (packet.Player)
        SendLfgPlayerInfo();
    else
        SendLfgPartyLockInfo();
}

void WorldSession::SendLfgPlayerInfo()
{
    TC_LOG_DEBUG("lfg", "SMSG_LFG_PLAYER_INFO %s", GetPlayerInfo().c_str());

    WorldPackets::LFG::LFGPlayerInfo data;
    ObjectGuid guid = GetPlayer()->GetGUID();
    
    uint8 level = GetPlayer()->getLevel();
    LfgDungeonSet const& randomDungeons = sLFGMgr->GetRandomAndSeasonalDungeons(level, GetExpansion());

    LfgLockMap const& lock = sLFGMgr->GetLockedDungeons(guid); // update to BlackListMap
    BuildLfgBlackListBlock(data.BlackList, guid, lock);

    for (LfgDungeonSet::const_iterator itr = randomDungeons.begin(); itr != randomDungeons.end(); ++itr)
    {
        WorldPackets::LFG::LfgPlayerDungeonInfo dungeon;
        dungeon.Slot = *itr;

        /// @todo: implement these fields
        dungeon.CompletionQuantity = 0;
        dungeon.CompletionLimit = 0;
        dungeon.CompletionCurrencyID = 0;
        dungeon.SpecificQuantity = 0;
        dungeon.SpecificLimit = 0;
        dungeon.OverallQuantity = 0;
        dungeon.OverallLimit = 0;
        dungeon.PurseWeeklyQuantity = 0;
        dungeon.PurseWeeklyLimit = 0;
        dungeon.PurseQuantity = 0;
        dungeon.PurseLimit = 0;
        dungeon.Quantity = 0;
        dungeon.CompletedMask = 0;

        LfgReward const* reward = sLFGMgr->GetRandomDungeonReward(*itr, level);
        if (reward)
        {
            Quest const* quest = sObjectMgr->GetQuestTemplate(reward->firstQuest);
            if (quest)
            {
                if (!GetPlayer()->CanRewardQuest(quest, false))
                    quest = sObjectMgr->GetQuestTemplate(reward->otherQuest);
                else
                    dungeon.FirstReward = true;

                 BuildReward(dungeon.Rewards, quest, GetPlayer());       
            }
        }

        // ShortageReward

        dungeon.ShortageEligible = false;

        data.Dungeon.push_back(dungeon);
    }

    SendPacket(data.Write());
}

void WorldSession::SendLfgPartyLockInfo()
{
    TC_LOG_DEBUG("lfg", "SMSG_LFG_PARTY_INFO %s", GetPlayerInfo().c_str());

    Group* group = GetPlayer()->GetGroup();
    if (!group)
        return;
    
    WorldPackets::LFG::LFGPartyInfo data;
  
    Group::MemberSlotList members = group->GetMemberSlots();
    for (auto const& member : members)
    {
        WorldPackets::LFG::LFGBlackList blackList;
        LfgLockMap const& lock = sLFGMgr->GetLockedDungeons(member.guid);
        BuildLfgBlackListBlock(blackList, member.guid, lock);

        data.Player.push_back(blackList);
    }

    SendPacket(data.Write());
}

void WorldSession::HandleDFGetJoinStatus(WorldPackets::LFG::DfGetJoinStatus& /*packet*/)
{
    TC_LOG_DEBUG("lfg", "CMSG_DF_GET_JOIN_STATUS %s", GetPlayerInfo().c_str());

    if (!GetPlayer()->isUsingLfg())
        return;

    ObjectGuid guid = GetPlayer()->GetGUID();
    LfgUpdateData updateData = sLFGMgr->GetLfgStatus(guid);

    if (GetPlayer()->GetGroup())
    {
        SendLfgUpdateStatus(updateData, true);
        updateData.dungeons.clear();
        SendLfgUpdateStatus(updateData, false);
    }
    else
    {
        SendLfgUpdateStatus(updateData, false);
        updateData.dungeons.clear();
        SendLfgUpdateStatus(updateData, true);
    }
}

void WorldSession::HandleLfgListGetStatus(WorldPackets::LFG::LFGListGetStatus& /*packet*/)
{

}

void WorldSession::HandleLfgListJoin(WorldPackets::LFG::LFGListJoin& packet)
{
    //sLFGMgr->LFGListJoin(packet.Name, packet.Comment, packet.VoiceChat);
}

void WorldSession::HandleLfgListLeave(WorldPackets::LFG::LFGListLeave& packet)
{
    ObjectGuid Guid = GetPlayer()->GetGUID();

    //sLFGMgr->LFGListLeave();
}

void WorldSession::HandleLfgListUpdateRequest(WorldPackets::LFG::LFGListUpdateRequest& packet)
{
    WorldPackets::LFG::LFGListJoinRequest info;

    info.ActivityID = 0;
    info.RequiredItemLevel = 0;
    info.Name = nullptr;
    info.Comment = nullptr;
    info.VoiceChat = nullptr;

    //BuildRideTicket;
    //sLFGMgr->LFGListUpdate();
}

void WorldSession::SendLfgListJoinResult(LfgJoinResultData const& joinData)
{
    WorldPackets::LFG::LFGListJoinResult result;

    result.ResultDetail = 0;
    result.Result = 0;

    //BuildRideTicket();
    SendPacket(result.Write());
}

void WorldSession::SendLfgListUpdateStatus(LfgUpdateData const& updateData, bool party)
{
    WorldPackets::LFG::LFGListUpdateStatus update;

    update.Listed = false;
    update.Unk = 0;
    update.Reason = 0;
    // BuildRideTicket();
    update.Request;
    
    SendPacket(update.Write());
}

void WorldSession::SendLfgUpdateStatus(LfgUpdateData const& updateData, bool party)
{
    bool join = false;
    bool queued = false;
    uint8 size = uint8(updateData.dungeons.size());
    ObjectGuid guid = _player->GetGUID();
    time_t joinTime = sLFGMgr->GetQueueJoinTime(_player->GetGUID());
    uint32 queueId = sLFGMgr->GetQueueId(_player->GetGUID());

    switch (updateData.updateType)
    {
        case LFG_UPDATETYPE_JOIN_QUEUE_INITIAL:            // Joined queue outside the dungeon
            join = true;
            break;
        case LFG_UPDATETYPE_JOIN_QUEUE:
        case LFG_UPDATETYPE_ADDED_TO_QUEUE:                // Role check Success
            join = true;
            queued = true;
            break;
        case LFG_UPDATETYPE_PROPOSAL_BEGIN:
            join = true;
            break;
        case LFG_UPDATETYPE_UPDATE_STATUS:
            join = updateData.state != LFG_STATE_ROLECHECK && updateData.state != LFG_STATE_NONE;
            queued = updateData.state == LFG_STATE_QUEUED;
            break;
        default:
            break;
    }

    TC_LOG_DEBUG("lfg", "SMSG_LFG_UPDATE_STATUS %s updatetype: %u, party %s", GetPlayerInfo().c_str(), updateData.updateType, party ? "true" : "false");

    WorldPackets::LFG::LFGUpdateStatus data;

    data.IsParty = party;
    data.Joined = join;
    data.Queued = queued;
    data.Reason = updateData.updateType;
    data.SubType = 0; // NYI
    data.Comment = updateData.comment;
    for (uint32 slot : updateData.dungeons)
        data.Slots.push_back(slot);

    data.Ticket.RequesterGuid = guid;
    data.Ticket.Id = 0;
    data.Ticket.Type = 0;
    data.Ticket.Time = 0;

    //updateStatus.Needs;             // Obviously NYI   
    //updateStatus.SuspendedPlayers;  // Obviously NYI
    data.NotifyUI = false;  // Obviously NYI
    data.RequestedRoles = 0;// Obviously NYI

    SendPacket(data.Write());
}

void WorldSession::SendLfgRoleChosen(ObjectGuid guid, uint8 roles)
{
    TC_LOG_DEBUG("lfg", "SMSG_LFG_ROLE_CHOSEN %s guid: %s roles: %u", GetPlayerInfo().c_str(), guid.ToString().c_str(), roles);

    WorldPackets::LFG::RoleChosen data;
    data.Player = guid;
    data.RoleMask = roles;
    data.Accepted = roles > 0; // maybe could be wrong

    SendPacket(data.Write());
}

void WorldSession::SendLfgRoleCheckUpdate(LfgRoleCheck const& roleCheck)
{
    LfgDungeonSet dungeons;
    if (roleCheck.rDungeonId)
        dungeons.insert(roleCheck.rDungeonId);
    else
        dungeons = roleCheck.dungeons;

    WorldPackets::LFG::LFGRoleCheckUpdate update;

    update.RoleCheckStatus = roleCheck.state;
    update.PartyIndex = roleCheck.roles.size(); // I think this is wrong
    update.BgQueueID = 0;
    update.ActivityID = 0;
    update.IsBeginning = roleCheck.state == LFG_ROLECHECK_INITIALITING;

    if (!dungeons.empty())
        for (LfgDungeonSet::iterator it = dungeons.begin(); it != dungeons.end(); ++it)
            update.JoinSlots.push_back(sLFGMgr->GetLFGDungeonEntry(*it));

    if (!roleCheck.roles.empty())
    {
        for (LfgRolesMap::const_iterator it = roleCheck.roles.begin(); it != roleCheck.roles.end(); ++it)
        {
            if (Player* player = ObjectAccessor::FindConnectedPlayer(it->first))
            {
                WorldPackets::LFG::LFGRoleCheckUpdateMember member;
                member.Guid = it->first;
                member.Level = player->getLevel();
                member.RoleCheckComplete = false;
                member.RolesDesired = it->second;

                update.Members.push_back(member);
            }
        }
    }

    SendPacket(update.Write());
}

void WorldSession::SendLfgQueueStatus(LfgQueueStatusData const& queueData)
{
    TC_LOG_DEBUG("lfg", "SMSG_LFG_QUEUE_STATUS");

    WorldPackets::LFG::LFGQueueStatus data;

    data.Ticket.RequesterGuid = _player->GetGUID();
    data.Ticket.Id = queueData.queueId;
    data.Ticket.Type = 2;                    // update later with proper enum
    data.Ticket.Time = queueData.joinTime;

    data.Slot = queueData.dungeonId;
    data.QueuedTime = queueData.queuedTime;
    data.AvgWaitTimeByRole[3] = queueData.waitTimeByRole[3];
    data.AvgWaitTime = queueData.waitTimeAvg;
    data.AvgWaitTimeMe = queueData.waitTime;
    data.LastNeeded[3] = queueData.lastNeeded[3]; // Make an array and get avg time from the elements in queueData

    SendPacket(data.Write());
}

void WorldSession::SendLfgPlayerReward(LfgPlayerRewardData const& rewardData)
{
    if (!rewardData.rdungeonEntry || !rewardData.sdungeonEntry || !rewardData.quest)
        return;

    TC_LOG_DEBUG("lfg", "SMSG_LFG_PLAYER_REWARD %s rdungeonEntry: %u, sdungeonEntry: %u, done: %u", GetPlayerInfo().c_str(), rewardData.rdungeonEntry, rewardData.sdungeonEntry, rewardData.done);

    uint8 itemNum = rewardData.quest->GetRewItemsCount() + rewardData.quest->GetRewCurrencyCount();

    WorldPackets::LFG::LFGPlayerReward data;
    data.QueuedSlot = rewardData.rdungeonEntry;
    data.ActualSlot = rewardData.sdungeonEntry;

    data.RewardMoney = GetPlayer()->GetQuestMoneyReward(rewardData.quest);
    data.AddedXP = GetPlayer()->GetQuestXPReward(rewardData.quest);
    //BuildPlayerReward(rewardData.quest, GetPlayer()); // write method for player specific reward

    SendPacket(data.Write());
}

void WorldSession::SendLfgBootProposalUpdate(LfgPlayerBoot const& boot)
{
    ObjectGuid guid = GetPlayer()->GetGUID();
    LfgAnswer playerVote = boot.votes.find(guid)->second;
    uint8 votesNum = 0;
    uint8 agreeNum = 0;
    uint32 secsleft = uint8((boot.cancelTime - time(nullptr)) / 1000);
    WorldPackets::LFG::LFGBootPlayer data;

    for (LfgAnswerContainer::const_iterator it = boot.votes.begin(); it != boot.votes.end(); ++it)
    {
        if (it->second != LFG_ANSWER_PENDING)
        {
            ++votesNum;
            if (it->second == LFG_ANSWER_AGREE)
                ++agreeNum;
        }
    }

    TC_LOG_DEBUG("lfg", "SMSG_LFG_BOOT_PROPOSAL_UPDATE %s inProgress: %u - didVote: %u - agree: %u - victim: %s votes: %u - agrees: %u - left: %u - needed: %u - reason %s",
        GetPlayerInfo().c_str(), uint8(boot.inProgress), uint8(playerVote != LFG_ANSWER_PENDING),
        uint8(playerVote == LFG_ANSWER_AGREE), boot.victim.ToString().c_str(), votesNum, agreeNum,
        secsleft, LFG_GROUP_KICK_VOTES_NEEDED, boot.reason.c_str()); // Update this 

    data.Info.VoteInProgress = boot.inProgress;                          // Vote in progress
    data.Info.VotePassed = agreeNum >= LFG_GROUP_KICK_VOTES_NEEDED; // Did succeed
    data.Info.MyVote = playerVote != LFG_ANSWER_PENDING;            // Did Vote
    data.Info.MyVoteCompleted = playerVote == LFG_ANSWER_AGREE;     // Agree
    data.Info.Target = boot.victim;                                      // Victim GUID
    data.Info.TotalVotes = votesNum;                                     // Total Votes
    data.Info.BootVotes = agreeNum;                                      // Agree Count
    data.Info.TimeLeft = secsleft;                                       // Time Left
    data.Info.VotesNeeded = LFG_GROUP_KICK_VOTES_NEEDED;            // Needed Votes
    data.Info.Reason = boot.reason.c_str();                              // Kick reason
    
    SendPacket(data.Write());
}

void WorldSession::SendLfgUpdateProposal(LfgProposal const& proposal)
{
    ObjectGuid guid = GetPlayer()->GetGUID();
    ObjectGuid groupGuid = proposal.players.find(guid)->second.group;
    bool silent = !proposal.isNew && groupGuid == proposal.group;
    uint32 dungeonEntry = proposal.dungeonId;
    uint32 queueId = sLFGMgr->GetQueueId(_player->GetGUID());
    time_t joinTime = sLFGMgr->GetQueueJoinTime(_player->GetGUID());

    TC_LOG_DEBUG("lfg", "SMSG_LFG_PROPOSAL_UPDATE %s state: %u", GetPlayerInfo().c_str(), proposal.state);

    // show random dungeon if player selected random dungeon and it's not lfg group
    if (!silent)
    {
        LfgDungeonSet const& playerDungeons = sLFGMgr->GetSelectedDungeons(guid);
        if (playerDungeons.find(proposal.dungeonId) == playerDungeons.end())
            dungeonEntry = (*playerDungeons.begin());
    }

    dungeonEntry = sLFGMgr->GetLFGDungeonEntry(dungeonEntry);

    WorldPackets::LFG::LFGProposalUpdate data;

    data.CompletedMask = proposal.encounters;            // Encounters done -- seems like it is the same? Maybe you could verify, Mitch.
    data.Slot = queueId;                                 // Queue Id -- maybe?
    data.InstanceID = dungeonEntry;                      // Dungeon
    data.ProposalID = proposal.id;                       // Proposal Id
    data.State = proposal.state;                         // State
    data.ProposalSilent = silent;

    for (LfgProposalPlayerContainer::const_iterator it = proposal.players.begin(); it != proposal.players.end(); ++it)
    {
        WorldPackets::LFG::LFGProposalUpdatePlayer playerUpdate;
        LfgProposalPlayer const& player = it->second;

        playerUpdate.Me = it->first == guid;
        playerUpdate.MyParty = player.group == proposal.group;            // Is group in dungeon
        playerUpdate.SameParty = player.group == groupGuid;               // Same group as the player
        playerUpdate.Accepted = player.accept == LFG_ANSWER_AGREE;
        playerUpdate.Responded = player.accept != LFG_ANSWER_PENDING;
        playerUpdate.Roles = player.role;

        data.Players.push_back(playerUpdate);
    }

    SendPacket(data.Write());
}

void WorldSession::SendLfgDisabled()
{
    TC_LOG_DEBUG("lfg", "SMSG_LFG_DISABLED %s", GetPlayerInfo().c_str());
    WorldPacket data(SMSG_LFG_DISABLED, 0);
    //SendPacket(/*packet.*/);
}

void WorldSession::SendLfgOfferContinue(uint32 dungeonEntry)
{
    TC_LOG_DEBUG("lfg", "SMSG_LFG_OFFER_CONTINUE %s dungeon entry: %u",
        GetPlayerInfo().c_str(), dungeonEntry);
    WorldPacket data(SMSG_LFG_OFFER_CONTINUE, 4);
    data << uint32(dungeonEntry);
    //SendPacket(/*packet.*/);
}

void WorldSession::SendLfgTeleportError(uint8 err)
{
    TC_LOG_DEBUG("lfg", "SMSG_LFG_TELEPORT_DENIED %s reason: %u",
        GetPlayerInfo().c_str(), err);
    WorldPacket data(SMSG_LFG_TELEPORT_DENIED, 4);
    data << uint32(err);                                   // Error
    //SendPacket(/*packet.*/);
}
