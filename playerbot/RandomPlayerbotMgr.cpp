#include "Config/Config.h"
#include "../botpch.h"
#include "playerbot.h"
#include "PlayerbotAIConfig.h"
#include "PlayerbotFactory.h"
#include "AccountMgr.h"
#include "ObjectMgr.h"
#include "DatabaseEnv.h"
#include "PlayerbotAI.h"
#include "Player.h"
#include "AiFactory.h"
#include "GuildTaskMgr.h"
#include "PlayerbotCommandServer.h"

#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include "FleeManager.h"
#include "ServerFacade.h"

#include "BattleGround.h"
#include "BattleGroundMgr.h"

#include "World/WorldState.h"

#ifndef MANGOSBOT_ZERO
#ifdef CMANGOS
#include "Arena/ArenaTeam.h"
#endif
#ifdef MANGOS
#include "ArenaTeam.h"
#endif
#endif

#include "TravelMgr.h"
#include <iomanip>

using namespace ai;
using namespace MaNGOS;

INSTANTIATE_SINGLETON_1(RandomPlayerbotMgr);

#ifdef CMANGOS
#include <boost/thread/thread.hpp>
#endif

#ifdef MANGOS
class PrintStatsThread: public ACE_Task <ACE_MT_SYNCH>
{
public:
    int svc(void) { sRandomPlayerbotMgr.PrintStats(); return 0; }
};
#endif
#ifdef CMANGOS
void PrintStatsThread()
{
    sRandomPlayerbotMgr.PrintStats();
}
#endif

void activatePrintStatsThread()
{
#ifdef MANGOS
    PrintStatsThread *thread = new PrintStatsThread();
    thread->activate();
#endif
#ifdef CMANGOS
    boost::thread t(PrintStatsThread);
    t.detach();
#endif
}

#ifdef MANGOS
class CheckBgQueueThread : public ACE_Task <ACE_MT_SYNCH>
{
public:
    int svc(void) { sRandomPlayerbotMgr.CheckBgQueue(); return 0; }
};
#endif
#ifdef CMANGOS
void CheckBgQueueThread()
{
    sRandomPlayerbotMgr.CheckBgQueue();
}
#endif

void activateCheckBgQueueThread()
{
#ifdef MANGOS
    CheckBgQueueThread *thread = new CheckBgQueueThread();
    thread->activate();
#endif
#ifdef CMANGOS
    boost::thread t(CheckBgQueueThread);
    t.detach();
#endif
}

#ifdef MANGOS
class CheckLfgQueueThread : public ACE_Task <ACE_MT_SYNCH>
{
public:
    int svc(void) { sRandomPlayerbotMgr.CheckLfgQueue(); return 0; }
};
#endif
#ifdef CMANGOS
void CheckLfgQueueThread()
{
    sRandomPlayerbotMgr.CheckLfgQueue();
}
#endif

void activateCheckLfgQueueThread()
{
#ifdef MANGOS
    CheckLfgQueueThread *thread = new CheckLfgQueueThread();
    thread->activate();
#endif
#ifdef CMANGOS
    boost::thread t(CheckLfgQueueThread);
    t.detach();
#endif
}

#ifdef MANGOS
class CheckPlayersThread : public ACE_Task <ACE_MT_SYNCH>
{
public:
    int svc(void) { sRandomPlayerbotMgr.CheckPlayers(); return 0; }
};
#endif
#ifdef CMANGOS
void CheckPlayersThread()
{
    sRandomPlayerbotMgr.CheckPlayers();
}
#endif

void activateCheckPlayersThread()
{
#ifdef MANGOS
    CheckPlayersThread *thread = new CheckPlayersThread();
    thread->activate();
#endif
#ifdef CMANGOS
    boost::thread t(CheckPlayersThread);
    t.detach();
#endif
}

RandomPlayerbotMgr::RandomPlayerbotMgr() : PlayerbotHolder(), processTicks(0), loginProgressBar(NULL)
{
    if (sPlayerbotAIConfig.enabled || sPlayerbotAIConfig.randomBotAutologin)
    {
        sPlayerbotCommandServer.Start();
        PrepareTeleportCache();
    }
}

RandomPlayerbotMgr::~RandomPlayerbotMgr()
{
}

int RandomPlayerbotMgr::GetMaxAllowedBotCount()
{
    return GetEventValue(0, "bot_count");
}

void RandomPlayerbotMgr::UpdateAIInternal(uint32 elapsed)
{
    if (sPlayerbotAIConfig.hasLog("player_location.csv"))
    {
        sPlayerbotAIConfig.openLog("player_location.csv", "w");
        if(sPlayerbotAIConfig.randomBotAutologin)
        for (auto i : GetAllBots())
        {
            Player* bot = i.second;
            if (!bot)
                continue;
            ostringstream out;
            out << sPlayerbotAIConfig.GetTimestampStr() << "+00,";
            out << "RND" << ",";
            out << bot->GetName() << ",";
            out << std::fixed << std::setprecision(2);
            WorldPosition(bot).printWKT(out);
            out << bot->GetOrientation() << ",";
            out << to_string(bot->getRace()) << ",";
            out << to_string(bot->getClass()) << ",";
            out << bot->GetMapId() << ",";
            out << bot->GetLevel() << ",";
            out << bot->GetHealth() << ",";
            out << bot->GetPowerPercent() << ",";
            out << bot->GetMoney() << ",";
            out << to_string(uint8(bot->GetPlayerbotAI()->GetGrouperType())) << ",";
            out << to_string(uint8(bot->GetPlayerbotAI()->GetGuilderType()));

            sPlayerbotAIConfig.log("player_location.csv", out.str().c_str());
        }
        for (auto i : GetPlayers())
        {
            Player* bot = i;
            if (!bot)
                continue;
            ostringstream out;
            out << sPlayerbotAIConfig.GetTimestampStr() << "+00,";
            out << "PLR" << ",";
            out << bot->GetName() << ",";
            out << std::fixed << std::setprecision(2);
            WorldPosition(bot).printWKT(out);
            out << bot->GetOrientation() << ",";
            out << to_string(bot->getRace()) << ",";
            out << to_string(bot->getClass()) << ",";
            out << bot->GetMapId() << ",";
            out << bot->GetLevel() << ",";
            out << bot->GetHealth() << ",";
            out << bot->GetPowerPercent() << ",";
            out << bot->GetMoney() << ",";
            out << 0 << "," << 0;

            sPlayerbotAIConfig.log("player_location.csv", out.str().c_str());
        }
    }

    if (!sPlayerbotAIConfig.randomBotAutologin || !sPlayerbotAIConfig.enabled)
        return;

    int maxAllowedBotCount = GetEventValue(0, "bot_count");
    if (!maxAllowedBotCount || ((uint32)maxAllowedBotCount < sPlayerbotAIConfig.minRandomBots || (uint32)maxAllowedBotCount > sPlayerbotAIConfig.maxRandomBots))
    {
        maxAllowedBotCount = urand(sPlayerbotAIConfig.minRandomBots, sPlayerbotAIConfig.maxRandomBots);
        SetEventValue(0, "bot_count", maxAllowedBotCount,
                urand(sPlayerbotAIConfig.randomBotCountChangeMinInterval, sPlayerbotAIConfig.randomBotCountChangeMaxInterval));
    }

   /*if (!loginProgressBar && playerBots.size() + 10 < maxAllowedBotCount)
    {
        sLog.outString("Logging in %d random bots in the background", maxAllowedBotCount);
        loginProgressBar = new BarGoLink(maxAllowedBotCount);
    }*/ // bar goes > 100%

    // Fix possible divide by zero if maxAllowedBotCount is smaller then sPlayerbotAIConfig.randomBotsPerInterval
    uint32 notDiv = 1;
    if (maxAllowedBotCount > (int)sPlayerbotAIConfig.randomBotsPerInterval)
        notDiv =  maxAllowedBotCount / sPlayerbotAIConfig.randomBotsPerInterval;

    //SetNextCheckDelay((uint32)max(1000, int(2000 * notDiv * sPlayerbotAIConfig.randomBotUpdateInterval) / 1000));

    list<uint32> bots = GetBots();
    int botCount = bots.size();

    PerformanceMonitorOperation *pmo = sPerformanceMonitor.start(PERF_MON_TOTAL,
            playerBots.size() < maxAllowedBotCount ? "RandomPlayerbotMgr::Login" : "RandomPlayerbotMgr::UpdateAIInternal");

    if (bots.size() < sPlayerbotAIConfig.minRandomBots)
    {
        AddRandomBots();
    }

    if (sPlayerbotAIConfig.syncLevelWithPlayers && players.size())
    {
        if (time(NULL) > (PlayersCheckTimer + 60))
            activateCheckPlayersThread();
    }

    if (sPlayerbotAIConfig.randomBotJoinLfg && players.size())
    {
        if (time(NULL) > (LfgCheckTimer + 30))
            activateCheckLfgQueueThread();
    }

    if (sPlayerbotAIConfig.randomBotJoinBG && players.size())
    {
        if (time(NULL) > (BgCheckTimer + 30))
            activateCheckBgQueueThread();
    }

    uint32 botProcessed = 0;
    for (list<uint32>::iterator i = bots.begin(); i != bots.end(); ++i)
    {
        uint32 bot = *i;
		if (ProcessBot(bot)) {
			botProcessed++;
		}

        if (botProcessed >= sPlayerbotAIConfig.randomBotsPerInterval)
            break;
    }   

    if (pmo) pmo->finish();
}

uint32 RandomPlayerbotMgr::AddRandomBots()
{
    set<uint32> bots;

   /* QueryResult* results = PlayerbotDatabase.PQuery(
        "select `bot` from ai_playerbot_random_bots where event = 'add'");

    if (results)
    {
        do
        {
            Field* fields = results->Fetch();
            uint32 bot = fields[0].GetUInt32();
            bots.insert(bot);
        } while (results->NextRow());
        delete results;
    }*/

    vector<uint32> guids;
    int maxAllowedBotCount = GetEventValue(0, "bot_count");
    int maxAllowedNewBotCount = max((int)(sPlayerbotAIConfig.randomBotsPerInterval / 4), (int)(maxAllowedBotCount - currentBots.size()));
    for (list<uint32>::iterator i = sPlayerbotAIConfig.randomBotAccounts.begin(); i != sPlayerbotAIConfig.randomBotAccounts.end(); i++)
    {
        uint32 accountId = *i;
        if (!sAccountMgr.GetCharactersCount(accountId))
            continue;

        if (urand(0, 100) > 10)  // more random selection
            continue;

        QueryResult* result = CharacterDatabase.PQuery("SELECT guid, race, name, level FROM characters WHERE account = '%u'", accountId);
        if (!result)
            continue;

        do
        {
            Field* fields = result->Fetch();
            uint32 guid = fields[0].GetUInt32();
            uint8 race = fields[1].GetUInt8();
            std::string name = fields[2].GetString();
            uint32 level = fields[3].GetUInt32();

            if (bots.find(guid) != bots.end())
                continue;
			// Although this code works it cuts the Maximum Bots setting in half. 
			// And, also doesn't see to be any reason to do it.
            //bool alliance = guids.size() % 2 == 0;
            //if (bots.find(guid) == bots.end() &&
            //        ((alliance && IsAlliance(race)) || ((!alliance && !IsAlliance(race))
            //)))
            //{
                guids.push_back(guid);
                uint32 bot = guid;
                SetEventValue(bot, "add", 1, urand(sPlayerbotAIConfig.minRandomBotInWorldTime, sPlayerbotAIConfig.maxRandomBotInWorldTime));
                bots.insert(bot);
                currentBots.push_back(bot);
                sLog.outDetail("Bot #%d %s:%d <%s>: log in", guid, IsAlliance(race) ? "A" : "H", level, name);
                if (guids.size() >= min((int)(sPlayerbotAIConfig.randomBotsPerInterval / 4), maxAllowedNewBotCount))
                {
                    delete result;
                    return guids.size();
                }
            //}
        } while (result->NextRow());
        delete result;
    }
    return guids.size();
}

void RandomPlayerbotMgr::LoadBattleMastersCache()
{
    BattleMastersCache.clear();

    sLog.outString("---------------------------------------");
    sLog.outString("          Loading BattleMasters Cache  ");
    sLog.outString("---------------------------------------");
    sLog.outString();

    QueryResult* result = WorldDatabase.Query("SELECT `entry`,`bg_template` FROM `battlemaster_entry`");

    uint32 count = 0;

    if (!result)
    {
        sLog.outString(">> Loaded 0 battlemaster entries - table is empty!");
        sLog.outString();
        return;
    }

    do
    {
        ++count;

        Field* fields = result->Fetch();

        uint32 entry = fields[0].GetUInt32();
        uint32 bgTypeId = fields[1].GetUInt32();

        CreatureInfo const* bmaster = sObjectMgr.GetCreatureTemplate(entry);
        if (!bmaster)
            continue;

#ifdef MANGOS
        FactionTemplateEntry const* bmFaction = sFactionTemplateStore.LookupEntry(bmaster->FactionAlliance);
#endif
#ifdef CMANGOS
        FactionTemplateEntry const* bmFaction = sFactionTemplateStore.LookupEntry(bmaster->Faction);
#endif
        uint32 bmFactionId = bmFaction->faction;
#ifdef MANGOS
        FactionEntry const* bmParentFaction = sFactionStore.LookupEntry(bmFactionId);
#endif
#ifdef CMANGOS
#ifdef MANGOSBOT_ONE
        FactionEntry const* bmParentFaction = sFactionStore.LookupEntry<FactionEntry>(bmFactionId);
#else
        FactionEntry const* bmParentFaction = sFactionStore.LookupEntry(bmFactionId);
#endif
#endif
        uint32 bmParentTeam = bmParentFaction->team;
        Team bmTeam = TEAM_BOTH_ALLOWED;
        if (bmParentTeam == 891)
            bmTeam = ALLIANCE;
        if (bmFactionId == 189)
            bmTeam = ALLIANCE;
        if (bmParentTeam == 892)
            bmTeam = HORDE;
        if (bmFactionId == 66)
            bmTeam = HORDE;

        BattleMastersCache[bmTeam][BattleGroundTypeId(bgTypeId)].insert(BattleMastersCache[bmTeam][BattleGroundTypeId(bgTypeId)].end(), entry);
        sLog.outDetail("Cached Battmemaster #%d for BG Type %d (%s)", entry, bgTypeId, bmTeam == ALLIANCE ? "Alliance" : bmTeam == HORDE ? "Horde" : "Neutral");

    } while (result->NextRow());

    delete result;

    sLog.outString(">> Loaded %u battlemaster entries", count);
    sLog.outString();
}

void RandomPlayerbotMgr::CheckBgQueue()
{
    if (!BgCheckTimer)
        BgCheckTimer = time(NULL);

    uint32 count = 0;
    uint32 visual_count = 0;

    int check_time = count > 0 ? 120 : 30;

    if (time(NULL) < (BgCheckTimer + check_time))
    {
        return;
    }
    else
    {
        BgCheckTimer = time(NULL);
    }

    sLog.outBasic("Checking BG Queue...");

    for (int i = BG_BRACKET_ID_FIRST; i < MAX_BATTLEGROUND_BRACKETS; ++i)
    {
        for (int j = BATTLEGROUND_QUEUE_AV; j < MAX_BATTLEGROUND_QUEUE_TYPES; ++j)
        {
            BgPlayers[j][i][0] = 0;
            BgPlayers[j][i][1] = 0;
            BgBots[j][i][0] = 0;
            BgBots[j][i][1] = 0;
            ArenaBots[j][i][0][0] = 0;
            ArenaBots[j][i][0][1] = 0;
            ArenaBots[j][i][1][0] = 0;
            ArenaBots[j][i][1][1] = 0;
            NeedBots[j][i][0] = false;
            NeedBots[j][i][1] = false;
        }
    }

    for (vector<Player*>::iterator i = players.begin(); i != players.end(); ++i)
    {
        Player* player = *i;

        if (!player->InBattleGroundQueue())
            continue;

        if (player->InBattleGround() && player->GetBattleGround()->GetStatus() == STATUS_WAIT_LEAVE)
            continue;

        for (int i = 0; i < PLAYER_MAX_BATTLEGROUND_QUEUES; ++i)
        {
            BattleGroundQueueTypeId queueTypeId = player->GetBattleGroundQueueTypeId(i);
            if (queueTypeId == BATTLEGROUND_QUEUE_NONE)
                continue;

            uint32 TeamId = player->GetTeam() == ALLIANCE ? 0 : 1;

            BattleGroundTypeId bgTypeId = sServerFacade.BgTemplateId(queueTypeId);
#ifdef MANGOS
            BattleGroundBracketId bracketId = player->GetBattleGroundBracketIdFromLevel(bgTypeId);
#endif
#ifdef CMANGOS
#ifdef MANGOSBOT_TWO
            BattleGround* bg = sBattleGroundMgr.GetBattleGroundTemplate(bgTypeId);
            uint32 mapId = bg->GetMapId();
            PvPDifficultyEntry const* pvpDiff = GetBattlegroundBracketByLevel(mapId, player->GetLevel());
            if (!pvpDiff)
                continue;

            BattleGroundBracketId bracketId = pvpDiff->GetBracketId();
#else
            BattleGroundBracketId bracketId = player->GetBattleGroundBracketIdFromLevel(bgTypeId);
#endif
#endif
#ifndef MANGOSBOT_ZERO
            if (ArenaType arenaType = sServerFacade.BgArenaType(queueTypeId))
            {
#ifdef MANGOS
                BattleGroundQueue& bgQueue = sServerFacade.bgQueue(queueTypeId);
                GroupQueueInfo ginfo;
                uint32 tempT = TeamId;

                if (bgQueue.GetPlayerGroupInfoData(player->GetObjectGuid(), &ginfo))
                {
                    if (ginfo.IsRated)
                    {
                        for (uint32 arena_slot = 0; arena_slot < MAX_ARENA_SLOT; ++arena_slot)
                        {
                            uint32 arena_team_id = player->GetArenaTeamId(arena_slot);
                            ArenaTeam* arenateam = sObjectMgr.GetArenaTeamById(arena_team_id);
                            if (!arenateam)
                                continue;
                            if (arenateam->GetType() != arenaType)
                                continue;

                            Rating[queueTypeId][bracketId][1] = arenateam->GetRating();
                        }
                    }
                    TeamId = ginfo.IsRated ? 1 : 0;
                }
                if (player->InArena())
                {
                    if (player->GetBattleGround()->isRated() && (ginfo.IsRated && ginfo.ArenaTeamId && ginfo.ArenaTeamRating && ginfo.OpponentsTeamRating))
                        TeamId = 1;
                    else
                        TeamId = 0;
        }
#endif
#ifdef CMANGOS
                BattleGroundQueue& bgQueue = sServerFacade.bgQueue(queueTypeId);
                GroupQueueInfo ginfo;
                uint32 tempT = TeamId;

                if (bgQueue.GetPlayerGroupInfoData(player->GetObjectGuid(), &ginfo))
                {
                    if (ginfo.isRated)
                    {
                        for (uint32 arena_slot = 0; arena_slot < MAX_ARENA_SLOT; ++arena_slot)
                        {
                            uint32 arena_team_id = player->GetArenaTeamId(arena_slot);
                            ArenaTeam* arenateam = sObjectMgr.GetArenaTeamById(arena_team_id);
                            if (!arenateam)
                                continue;
                            if (arenateam->GetType() != arenaType)
                                continue;

                            Rating[queueTypeId][bracketId][1] = arenateam->GetRating();
                        }
                    }
                    TeamId = ginfo.isRated ? 1 : 0;
                }
                if (player->InArena())
                {
                    if (player->GetBattleGround()->IsRated()/* && (ginfo.isRated && ginfo.arenaTeamId && ginfo.arenaTeamRating && ginfo.opponentsTeamRating)*/)
                        TeamId = 1;
                    else
                        TeamId = 0;
                }
#endif
                ArenaBots[queueTypeId][bracketId][TeamId][tempT]++;
    }
#endif

            if (player->GetPlayerbotAI())
                BgBots[queueTypeId][bracketId][TeamId]++;
            else
                BgPlayers[queueTypeId][bracketId][TeamId]++;

            if (!player->IsInvitedForBattleGroundQueueType(queueTypeId) && (!player->InBattleGround() || player->GetBattleGround()->GetTypeId() != sServerFacade.BgTemplateId(queueTypeId)))
            {
                NeedBots[queueTypeId][bracketId][TeamId] = true;
            }
        }
    }

    for (PlayerBotMap::iterator i = playerBots.begin(); i != playerBots.end(); ++i)
    {
        Player* bot = i->second;

        if (!bot->InBattleGroundQueue())
            continue;

        if (!IsRandomBot(bot->GetGUIDLow()))
            continue;

        if (bot->InBattleGround() && bot->GetBattleGround()->GetStatus() == STATUS_WAIT_LEAVE)
            continue;

        for (int i = 0; i < PLAYER_MAX_BATTLEGROUND_QUEUES; ++i)
        {
            BattleGroundQueueTypeId queueTypeId = bot->GetBattleGroundQueueTypeId(i);
            if (queueTypeId == BATTLEGROUND_QUEUE_NONE)
                continue;

            uint32 TeamId = bot->GetTeam() == ALLIANCE ? 0 : 1;

            BattleGroundTypeId bgTypeId = sServerFacade.BgTemplateId(queueTypeId);
#ifdef MANGOS
            BattleGroundBracketId bracketId = bot->GetBattleGroundBracketIdFromLevel(bgTypeId);
#endif
#ifdef CMANGOS
#ifdef MANGOSBOT_TWO
            BattleGround* bg = sBattleGroundMgr.GetBattleGroundTemplate(bgTypeId);
            uint32 mapId = bg->GetMapId();
            PvPDifficultyEntry const* pvpDiff = GetBattlegroundBracketByLevel(mapId, bot->GetLevel());
            if (!pvpDiff)
                continue;

            BattleGroundBracketId bracketId = pvpDiff->GetBracketId();
#else
            BattleGroundBracketId bracketId = bot->GetBattleGroundBracketIdFromLevel(bgTypeId);
#endif
#endif

#ifndef MANGOSBOT_ZERO
            ArenaType arenaType = sServerFacade.BgArenaType(queueTypeId);
            if (arenaType != ARENA_TYPE_NONE)
            {
                BattleGroundQueue& bgQueue = sServerFacade.bgQueue(queueTypeId);
                GroupQueueInfo ginfo;
                uint32 tempT = TeamId;
#ifdef MANGOS
                if (bgQueue.GetPlayerGroupInfoData(bot->GetObjectGuid(), &ginfo))
                {
                    TeamId = ginfo.IsRated ? 1 : 0;
                }
                if (bot->InArena())
                {
                    if (bot->GetBattleGround()->isRated() && (ginfo.IsRated && ginfo.ArenaTeamId && ginfo.ArenaTeamRating && ginfo.OpponentsTeamRating))
                        TeamId = 1;
                    else
                        TeamId = 0;
                }
#endif
#ifdef CMANGOS
                if (bgQueue.GetPlayerGroupInfoData(bot->GetObjectGuid(), &ginfo))
                {
                    TeamId = ginfo.isRated ? 1 : 0;
                }
                if (bot->InArena())
                {
                    if (bot->GetBattleGround()->IsRated()/* && (ginfo.isRated && ginfo.arenaTeamId && ginfo.arenaTeamRating && ginfo.opponentsTeamRating)*/)
                        TeamId = 1;
                    else
                        TeamId = 0;
                }
#endif
                ArenaBots[queueTypeId][bracketId][TeamId][tempT]++;
            }
#endif
            BgBots[queueTypeId][bracketId][TeamId]++;
        }
    }

    for (int i = BG_BRACKET_ID_FIRST; i < MAX_BATTLEGROUND_BRACKETS; ++i)
    {
        for (int j = BATTLEGROUND_QUEUE_AV; j < MAX_BATTLEGROUND_QUEUE_TYPES; ++j)
        {
            BattleGroundQueueTypeId queueTypeId = BattleGroundQueueTypeId(j);

            if ((BgPlayers[j][i][0] + BgBots[j][i][0] + BgPlayers[j][i][1] + BgBots[j][i][1]) == 0)
                continue;

#ifndef MANGOSBOT_ZERO
            if (ArenaType type = sServerFacade.BgArenaType(queueTypeId))
            {
                sLog.outBasic("ARENA:%s %s: P (Skirmish:%d, Rated:%d) B (Skirmish:%d, Rated:%d) Total (Skirmish:%d Rated:%d)",
                    type == ARENA_TYPE_2v2 ? "2v2" : type == ARENA_TYPE_3v3 ? "3v3" : "5v5",
                    i == 0 ? "10-19" : i == 1 ? "20-29" : i == 2 ? "30-39" : i == 3 ? "40-49" : i == 4 ? "50-59" : (i == 5 && MAX_BATTLEGROUND_BRACKETS == 6) ? "60" : (i == 5 && MAX_BATTLEGROUND_BRACKETS == 7) ? "60-69" : i == 6 ? (i == 6 && MAX_BATTLEGROUND_BRACKETS == 16) ? "70-79" : "70" : "80",
                    BgPlayers[j][i][0],
                    BgPlayers[j][i][1],
                    BgBots[j][i][0],
                    BgBots[j][i][1],
                    BgPlayers[j][i][0] + BgBots[j][i][0],
                    BgPlayers[j][i][1] + BgBots[j][i][1]
                );
                continue;
            }
#endif
            BattleGroundTypeId bgTypeId = sServerFacade.BgTemplateId(queueTypeId);
            sLog.outBasic("BG:%s %s: P (%d:%d) B (%d:%d) Total (A:%d H:%d)",
                bgTypeId == BATTLEGROUND_AV ? "AV" : bgTypeId == BATTLEGROUND_WS ? "WSG" : bgTypeId == BATTLEGROUND_AB ? "AB" : "EoTS",
                i == 0 ? "10-19" : i == 1 ? "20-29" : i == 2 ? "30-39" : i == 3 ? "40-49" : i == 4 ? "50-59" : (i == 5 && MAX_BATTLEGROUND_BRACKETS == 6) ? "60" : (i == 5 && MAX_BATTLEGROUND_BRACKETS == 7) ? "60-69" : i == 6 ? (i == 6 && MAX_BATTLEGROUND_BRACKETS == 16) ? "70-79" : "70" : "80",
                BgPlayers[j][i][0],
                BgPlayers[j][i][1],
                BgBots[j][i][0],
                BgBots[j][i][1],
                BgPlayers[j][i][0] + BgBots[j][i][0],
                BgPlayers[j][i][1] + BgBots[j][i][1]
            );
        }
    }

    sLog.outBasic("BG Queue check finished");
    return;
}

void RandomPlayerbotMgr::CheckLfgQueue()
{
    if (!LfgCheckTimer || time(NULL) > (LfgCheckTimer + 30))
        LfgCheckTimer = time(NULL);

    sLog.outBasic("Checking LFG Queue...");

    // Clear LFG list
    LfgDungeons[HORDE].clear();
    LfgDungeons[ALLIANCE].clear();

    for (vector<Player*>::iterator i = players.begin(); i != players.end(); ++i)
    {
        Player* player = *i;

        bool isLFG = false;

#ifdef MANGOSBOT_ZERO
        Group* group = player->GetGroup();
        if (group)
        {
            if (sLFGMgr.IsGroupInQueue(group->GetId()))
            {
                isLFG = true;
                LFGGroupQueueInfo lfgInfo;
                sLFGMgr.GetGroupQueueInfo(&lfgInfo, group->GetId());
                LfgDungeons[player->GetTeam()].push_back(lfgInfo.areaId);
            }
        }
        else
        {
            if (sLFGMgr.IsPlayerInQueue(player->GetObjectGuid()))
            {
                isLFG = true;
                LFGPlayerQueueInfo lfgInfo;
                sLFGMgr.GetPlayerQueueInfo(&lfgInfo, player->GetObjectGuid());

                LfgDungeons[player->GetTeam()].push_back(lfgInfo.areaId);
            }
        }
#endif
#ifdef MANGOSBOT_TWO
        Group* group = player->GetGroup();
        if (group)
        {
            if (sLFGMgr.GetQueueInfo(group->GetObjectGuid()))
            {
                isLFG = true;
                LFGGroupState* gState = sLFGMgr.GetLFGGroupState(group->GetObjectGuid());
                if (gState->GetState() != LFG_STATE_NONE && gState->GetState() < LFG_STATE_DUNGEON)
                {
                    LFGDungeonSet const* dList = gState->GetDungeons();
                    for (LFGDungeonSet::const_iterator itr = dList->begin(); itr != dList->end(); ++itr)
                    {
                        LFGDungeonEntry const* dungeon = *itr;

                        if (!dungeon)
                            continue;

                        LfgDungeons[player->GetTeam()].push_back(dungeon->ID);
                    }
                }
            }
        }
        else
        {
            if (sLFGMgr.GetQueueInfo(player->GetObjectGuid()))
            {
                isLFG = true;
                LFGPlayerState* pState = sLFGMgr.GetLFGPlayerState(player->GetObjectGuid());
                if (pState->GetState() != LFG_STATE_NONE && pState->GetState() < LFG_STATE_DUNGEON)
                {
                    LFGDungeonSet const* dList = pState->GetDungeons();
                    for (LFGDungeonSet::const_iterator itr = dList->begin(); itr != dList->end(); ++itr)
                    {
                        LFGDungeonEntry const* dungeon = *itr;

                        if (!dungeon)
                            continue;

                        LfgDungeons[player->GetTeam()].push_back(dungeon->ID);
                    }
                }
            }
        }
#endif
    }
    sLog.outBasic("LFG Queue check finished");
    return;
}

void RandomPlayerbotMgr::CheckPlayers()
{
    if (!PlayersCheckTimer || time(NULL) > (PlayersCheckTimer + 60))
        PlayersCheckTimer = time(NULL);

    sLog.outBasic("Checking Players...");

    if (!playersLevel)
        playersLevel = sPlayerbotAIConfig.randombotStartingLevel;


    for (vector<Player*>::iterator i = players.begin(); i != players.end(); ++i)
    {
        Player* player = *i;

        if (player->IsGameMaster())
            continue;

        //if (player->GetSession()->GetSecurity() > SEC_PLAYER)
        //    continue;

        if (player->GetLevel() > playersLevel)
            playersLevel = player->GetLevel() + 3;
    }
    sLog.outBasic("Max player level is %d, max bot level set to %d", playersLevel - 3, playersLevel);
    return;
}

bool RandomPlayerbotMgr::ProcessBot(uint32 bot)
{
    Player* player = GetPlayerBot(bot);
    PlayerbotAI* ai = player ? player->GetPlayerbotAI() : NULL;

    uint32 isValid = GetEventValue(bot, "add");
    if (!isValid)
    {
		if (!player || !player->GetGroup())
		{
            if (player)
                sLog.outDetail("Bot #%d %s:%d <%s>: log out", bot, IsAlliance(player->getRace()) ? "A" : "H", player->GetLevel(), player->GetName());
            else
                sLog.outDetail("Bot #%d: log out", bot);

			SetEventValue(bot, "add", 0, 0);
			currentBots.remove(bot);
			if (player) LogoutPlayerBot(bot);
		}
        return true;
    }

    uint32 isLogginIn = GetEventValue(bot, "login");
    if (isLogginIn)
        return false;

    if (!player)
    {
        if (urand(0, 100) > 10) // less lag during bots login
        {
            return true;
        }

        AddPlayerBot(bot, 0);
        SetEventValue(bot, "login", 1, sPlayerbotAIConfig.randomBotUpdateInterval);

        return true;
    }

    // Hotfix System
    /*if (player && !sServerFacade.UnitIsDead(player))
    {
        uint32 version = GetEventValue(bot, "version");
        if (!version)
        {
            version = 0;
        }
        if (version < VERSION)
        {
            Hotfix(player, version);
        }
    }*/

    SetEventValue(bot, "login", 0, 0);
    if (player->GetGroup() || player->IsTaxiFlying())
        return false;

    uint32 logout = GetEventValue(bot, "logout");
    if (player && !logout && !isValid)
    {
        sLog.outBasic("Bot #%d %s:%d <%s>: log out", bot, IsAlliance(player->getRace()) ? "A" : "H", player->GetLevel(), player->GetName());
        LogoutPlayerBot(bot);
        SetEventValue(bot, "logout", 1, urand(sPlayerbotAIConfig.minRandomBotInWorldTime, sPlayerbotAIConfig.maxRandomBotInWorldTime));
        return true;
    }

    return false;
}

void RandomPlayerbotMgr::RandomTeleport(Player* bot, vector<WorldLocation> &locs, bool hearth)
{
    if (bot->IsBeingTeleported())
        return;

    if (bot->InBattleGround())
        return;

    if (bot->InBattleGroundQueue())
        return;

	if (bot->GetLevel() < 5)
		return;

    if (sPlayerbotAIConfig.randomBotRpgChance < 0)
        return;

    if (locs.empty())
    {
        sLog.outError("Cannot teleport bot %s - no locations available", bot->GetName());
        return;
    }

    vector<WorldPosition> tlocs;

    for (auto& loc : locs)
        tlocs.push_back(WorldPosition(loc));

    //Do not teleport to maps disabled in config
    tlocs.erase(std::remove_if(tlocs.begin(), tlocs.end(), [bot](WorldPosition l) {vector<uint32>::iterator i = find(sPlayerbotAIConfig.randomBotMaps.begin(), sPlayerbotAIConfig.randomBotMaps.end(), l.getMapId()); return i == sPlayerbotAIConfig.randomBotMaps.end(); }), tlocs.end());

    //Random shuffle based on distance. Closer distances are more likely (but not exclusivly) to be at the begin of the list.
    tlocs = sTravelMgr.getNextPoint(WorldPosition(bot), tlocs, 0);

    //5% + 0.1% per level chance node on different map in selection.
    //tlocs.erase(std::remove_if(tlocs.begin(), tlocs.end(), [bot](WorldLocation const& l) {return l.mapid != bot->GetMapId() && urand(1, 100) > 0.5 * bot->GetLevel(); }), tlocs.end());

    //Continent is about 20.000 large
    //Bot will travel 0-5000 units + 75-150 units per level.
    //tlocs.erase(std::remove_if(tlocs.begin(), tlocs.end(), [bot](WorldLocation const& l) {return l.mapid == bot->GetMapId() && sServerFacade.GetDistance2d(bot, l.coord_x, l.coord_y) > urand(0, 5000) + bot->GetLevel() * 15 * urand(5, 10); }), tlocs.end());

    if (tlocs.empty())
    {
        sLog.outError("Cannot teleport bot %s - no locations available", bot->GetName());

        return;
    }

    PerformanceMonitorOperation *pmo = sPerformanceMonitor.start(PERF_MON_RNDBOT, "RandomTeleportByLocations");

    int index = 0;

    for (int i = 0; i < tlocs.size(); i++)
    {
        for (int attemtps = 0; attemtps < 3; ++attemtps)
        {

            WorldLocation loc = tlocs[i].getLocation();

#ifndef MANGOSBOT_ZERO
            // Teleport to Dark Portal area if event is in progress
            if (sWorldState.GetExpansion() == EXPANSION_NONE && bot->GetLevel() > 54 && urand(0, 100) > 20)
            {
                if (urand(0, 1))
                    loc = WorldLocation(uint32(0), -11772.43f, -3272.84f, -17.9f, 3.32447f);
                else
                    loc = WorldLocation(uint32(0), -11741.70f, -3130.3f, -11.7936f, 3.32447f);
            }
#endif

            float x = loc.coord_x + (attemtps > 0 ? urand(0, sPlayerbotAIConfig.grindDistance) - sPlayerbotAIConfig.grindDistance / 2 : 0);
            float y = loc.coord_y + (attemtps > 0 ? urand(0, sPlayerbotAIConfig.grindDistance) - sPlayerbotAIConfig.grindDistance / 2 : 0);
            float z = loc.coord_z;

            Map* map = sMapMgr.FindMap(loc.mapid, 0);
            if (!map)
                continue;

            const TerrainInfo* terrain = map->GetTerrain();
            if (!terrain)
                continue;

            AreaTableEntry const* area = GetAreaEntryByAreaID(terrain->GetAreaId(x, y, z));
            if (!area)
                continue;

#ifndef MANGOSBOT_ZERO
            // Do not teleport to outland before portal opening (allow new races zones)
            if (sWorldState.GetExpansion() == EXPANSION_NONE && loc.mapid == 530 && area->team != 2 && area->team != 4)
                continue;
#endif

            // Do not teleport to enemy zones if level is low
            if (area->team == 4 && bot->GetTeam() == ALLIANCE && bot->GetLevel() < 40)
                continue;
            if (area->team == 2 && bot->GetTeam() == HORDE && bot->GetLevel() < 40)
                continue;

            if (terrain->IsUnderWater(x, y, z) ||
                terrain->IsInWater(x, y, z))
                continue;

#ifdef MANGOSBOT_TWO
            float ground = map->GetHeight(bot->GetPhaseMask(), x, y, z + 0.5f);
#else
            float ground = map->GetHeight(x, y, z + 0.5f);
#endif
            if (ground <= INVALID_HEIGHT)
                continue;

            z = 0.05f + ground;
            sLog.outDetail("Random teleporting bot %s to %s %f,%f,%f (%u/%zu locations)",
                bot->GetName(), area->area_name[0], x, y, z, attemtps, tlocs.size());

            if (bot->IsTaxiFlying())
            {
                bot->GetMotionMaster()->MovementExpired();
#ifdef MANGOS
                bot->m_taxi.ClearTaxiDestinations();
#endif
            }
            if (hearth)
                bot->SetHomebindToLocation(loc, area->ID);

            bot->GetMotionMaster()->Clear();
            bot->TeleportTo(loc.mapid, x, y, z, 0);
            bot->SendHeartBeat();
            bot->GetPlayerbotAI()->ResetStrategies();
            bot->GetPlayerbotAI()->Reset();
            if (pmo) pmo->finish();
            return;
        }
    }

    if (pmo) pmo->finish();
    sLog.outError("Cannot teleport bot %s - no locations available", bot->GetName());
}

void RandomPlayerbotMgr::PrepareTeleportCache()
{
    uint32 maxLevel = sWorld.getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL);

    QueryResult* results = PlayerbotDatabase.PQuery("select map_id, x, y, z, level from ai_playerbot_tele_cache");
    if (results)
    {
        sLog.outString("Loading random teleport caches for %d levels...", maxLevel);
        do
        {
            Field* fields = results->Fetch();
            uint16 mapId = fields[0].GetUInt16();
            float x = fields[1].GetFloat();
            float y = fields[2].GetFloat();
            float z = fields[3].GetFloat();
            uint16 level = fields[4].GetUInt16();
            WorldLocation loc(mapId, x, y, z, 0);
            locsPerLevelCache[level].push_back(loc);
        } while (results->NextRow());
        delete results;
    }
    else
    {
        sLog.outString("Preparing random teleport caches for %d levels...", maxLevel);
        BarGoLink bar(maxLevel);
        for (uint8 level = 1; level <= maxLevel; level++)
        {
            QueryResult* results = WorldDatabase.PQuery("select map, position_x, position_y, position_z "
                "from (select map, position_x, position_y, position_z, avg(t.maxlevel), avg(t.minlevel), "
                "%u - (avg(t.maxlevel) + avg(t.minlevel)) / 2 delta "
                "from creature c inner join creature_template t on c.id = t.entry where t.NpcFlags = 0 and t.lootid != 0 and t.unitFlags != 768 group by t.entry having count(*) > 1) q "
                "where delta >= 0 and delta <= %u and map in (%s) and not exists ( "
                "select map, position_x, position_y, position_z from "
                "("
                "select map, c.position_x, c.position_y, c.position_z, avg(t.maxlevel), avg(t.minlevel), "
                "%u - (avg(t.maxlevel) + avg(t.minlevel)) / 2 delta "
                "from creature c "
                "inner join creature_template t on c.id = t.entry where t.NpcFlags = 0 and t.lootid != 0 group by t.entry "
                ") q1 "
                "where abs(delta) > %u and q1.map = q.map "
                "and sqrt("
                "(q1.position_x - q.position_x)*(q1.position_x - q.position_x) +"
                "(q1.position_y - q.position_y)*(q1.position_y - q.position_y) +"
                "(q1.position_z - q.position_z)*(q1.position_z - q.position_z)"
                ") < %u)",
                level,
                sPlayerbotAIConfig.randomBotTeleLevel,
                sPlayerbotAIConfig.randomBotMapsAsString.c_str(),
                level,
                sPlayerbotAIConfig.randomBotTeleLevel,
                (uint32)sPlayerbotAIConfig.sightDistance
                );
            if (results)
            {
                do
                {
                    Field* fields = results->Fetch();
                    uint16 mapId = fields[0].GetUInt16();
                    float x = fields[1].GetFloat();
                    float y = fields[2].GetFloat();
                    float z = fields[3].GetFloat();
                    WorldLocation loc(mapId, x, y, z, 0);
                    locsPerLevelCache[level].push_back(loc);

                    PlayerbotDatabase.PExecute("insert into ai_playerbot_tele_cache (level, map_id, x, y, z) values (%u, %u, %f, %f, %f)",
                            level, mapId, x, y, z);
                } while (results->NextRow());
                delete results;
            }
            bar.step();
        }
    }

    sLog.outString("Preparing RPG teleport caches for %d factions...", sFactionTemplateStore.GetNumRows());

		    results = WorldDatabase.PQuery("SELECT map, position_x, position_y, position_z, "
				"r.race, r.minl, r.maxl "
				"from creature c inner join ai_playerbot_rpg_races r on c.id = r.entry "
				"where r.race < 15");

	if (results)
	{
		do
		{
			for (uint32 level = 1; level < maxLevel + 1; level++)
			{
				Field* fields = results->Fetch();
				uint16 mapId = fields[0].GetUInt16();
				float x = fields[1].GetFloat();
				float y = fields[2].GetFloat();
				float z = fields[3].GetFloat();
				//uint32 faction = fields[4].GetUInt32();
				//string name = fields[5].GetCppString();
				uint32 race = fields[4].GetUInt32();
				uint32 minl = fields[5].GetUInt32();
				uint32 maxl = fields[6].GetUInt32();

				if (level > maxl || level < minl) continue;

				WorldLocation loc(mapId, x, y, z, 0);
				for (uint32 r = 1; r < MAX_RACES; r++)
				{
					if (race == r || race == 0) rpgLocsCacheLevel[r][level].push_back(loc);
				}
			}
			//bar.step();
		} while (results->NextRow());
		delete results;
	}
}

void RandomPlayerbotMgr::IncreaseLevel(Player* bot)
{
	PerformanceMonitorOperation *pmo = sPerformanceMonitor.start(PERF_MON_RNDBOT, "IncreaseLevel");
	uint32 lastLevel = GetValue(bot, "level");
	uint32 level = bot->GetLevel();
	if (lastLevel != level)
	{
        PlayerbotFactory factory(bot, level);
        factory.Randomize(true);        
	}

    if (pmo) pmo->finish();
}

void RandomPlayerbotMgr::RandomizeFirst(Player* bot)
{
	uint32 maxLevel = sPlayerbotAIConfig.randomBotMaxLevel;
	if (maxLevel > sWorld.getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL))
		maxLevel = sWorld.getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL);

    // if lvl sync is enabled, max level is limited by online players lvl
    if (sPlayerbotAIConfig.syncLevelWithPlayers)
        maxLevel = max(sPlayerbotAIConfig.randomBotMinLevel, min(playersLevel, sWorld.getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL)));

	PerformanceMonitorOperation *pmo = sPerformanceMonitor.start(PERF_MON_RNDBOT, "RandomizeFirst");
    uint32 level = urand(sPlayerbotAIConfig.randomBotMinLevel, maxLevel);

#ifdef MANGOSBOT_TWO
    if (bot->getClass() == CLASS_DEATH_KNIGHT)
        level = urand(sWorld.getConfig(CONFIG_UINT32_START_HEROIC_PLAYER_LEVEL), max(sWorld.getConfig(CONFIG_UINT32_START_HEROIC_PLAYER_LEVEL), maxLevel));
#endif

    if (urand(0, 100) < 100 * sPlayerbotAIConfig.randomBotMaxLevelChance)
        level = maxLevel;

#ifndef MANGOSBOT_ZERO
    if (sWorldState.GetExpansion() == EXPANSION_NONE && level > 60)
        level = 60;
#endif

    SetValue(bot, "level", level);
	
    uint32 randomTime = urand(sPlayerbotAIConfig.minRandomBotRandomizeTime, sPlayerbotAIConfig.maxRandomBotRandomizeTime);
	uint32 inworldTime = urand(sPlayerbotAIConfig.minRandomBotInWorldTime, sPlayerbotAIConfig.maxRandomBotInWorldTime);
    PlayerbotDatabase.PExecute("update ai_playerbot_random_bots set validIn = '%u' where event = 'randomize' and bot = '%u'",
            randomTime, bot->GetGUIDLow());
    PlayerbotDatabase.PExecute("update ai_playerbot_random_bots set validIn = '%u' where event = 'logout' and bot = '%u'",
			inworldTime, bot->GetGUIDLow());

    // teleport to a random inn for bot level
    bot->GetPlayerbotAI()->Reset(true);

	if (pmo) pmo->finish();
}

uint32 RandomPlayerbotMgr::GetZoneLevel(uint16 mapId, float teleX, float teleY, float teleZ)
{
	uint32 maxLevel = sWorld.getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL);

	uint32 level;
    QueryResult* results = WorldDatabase.PQuery("select avg(t.minlevel) minlevel, avg(t.maxlevel) maxlevel from creature c "
            "inner join creature_template t on c.id = t.entry "
            "where map = '%u' and minlevel > 1 and abs(position_x - '%f') < '%u' and abs(position_y - '%f') < '%u'",
            mapId, teleX, sPlayerbotAIConfig.randomBotTeleportDistance / 2, teleY, sPlayerbotAIConfig.randomBotTeleportDistance / 2);

    if (results)
    {
        Field* fields = results->Fetch();
        uint8 minLevel = fields[0].GetUInt8();
        uint8 maxLevel = fields[1].GetUInt8();
        level = urand(minLevel, maxLevel);
        if (level > maxLevel)
            level = maxLevel;
		delete results;
    }
    else
    {
        level = urand(1, maxLevel);
    }

    return level;
}

void RandomPlayerbotMgr::Refresh(Player* bot)
{
    if (sServerFacade.UnitIsDead(bot))
    {
        bot->ResurrectPlayer(1.0f);
        bot->SpawnCorpseBones();
        bot->GetPlayerbotAI()->ResetStrategies(false);
    }

    if (bot->InBattleGround())
        return;

    sLog.outDetail("Refreshing bot #%d <%s>", bot->GetGUIDLow(), bot->GetName());
    PerformanceMonitorOperation *pmo = sPerformanceMonitor.start(PERF_MON_RNDBOT, "Refresh");

    bot->GetPlayerbotAI()->Reset();

    bot->DurabilityRepairAll(false, 1.0f
#ifndef MANGOSBOT_ZERO
        , false
#endif
    );
	bot->SetHealthPercent(100);
	bot->SetPvP(true);

    PlayerbotFactory factory(bot, bot->GetLevel());
    factory.Refresh();

    if (bot->GetMaxPower(POWER_MANA) > 0)
        bot->SetPower(POWER_MANA, bot->GetMaxPower(POWER_MANA));

    if (bot->GetMaxPower(POWER_ENERGY) > 0)
        bot->SetPower(POWER_ENERGY, bot->GetMaxPower(POWER_ENERGY));

    uint32 money = bot->GetMoney();
    bot->SetMoney(money + 1000 * sqrt(urand(1, bot->GetLevel() * 5)));

    if (pmo) pmo->finish();
}


bool RandomPlayerbotMgr::IsRandomBot(Player* bot)
{
    if (bot)
        return IsRandomBot(bot->GetGUIDLow());

    return false;
}

bool RandomPlayerbotMgr::IsRandomBot(uint32 bot)
{
    return GetEventValue(bot, "add");
}

list<uint32> RandomPlayerbotMgr::GetBots()
{
    if (!currentBots.empty()) return currentBots;

   /* QueryResult* results = PlayerbotDatabase.Query(
            "select bot from ai_playerbot_random_bots where owner = 0 and event = 'add'");

    if (results)
    {
        do
        {
            Field* fields = results->Fetch();
            uint32 bot = fields[0].GetUInt32();
            currentBots.push_back(bot);
        } while (results->NextRow());
		delete results;
    }*/

    return currentBots;
}

list<uint32> RandomPlayerbotMgr::GetBgBots(uint32 bracket)
{
    //if (!currentBgBots.empty()) return currentBgBots;

    QueryResult* results = PlayerbotDatabase.PQuery(
        "select bot from ai_playerbot_random_bots where event = 'bg' AND value = '%d'", bracket);
    list<uint32> BgBots;
    if (results)
    {
        do
        {
            Field* fields = results->Fetch();
            uint32 bot = fields[0].GetUInt32();
            BgBots.push_back(bot);
        } while (results->NextRow());
        delete results;
    }

    return BgBots;
}

uint32 RandomPlayerbotMgr::GetEventValue(uint32 bot, string event)
{
    // load all events at once on first event load
    if (eventCache[bot].empty())
    {
        QueryResult* results = PlayerbotDatabase.PQuery("SELECT `event`, `value`, `time`, validIn, `data` from ai_playerbot_random_bots where owner = 0 and bot = '%u'", bot);
        if (results)
        {
            do
            {
                Field* fields = results->Fetch();
                string eventName = fields[0].GetString();
                CachedEvent e;
                e.value = fields[1].GetUInt32();
                e.lastChangeTime = fields[2].GetUInt32();
                e.validIn = fields[3].GetUInt32();
                e.data = fields[4].GetString();
                eventCache[bot][eventName] = e;
            } while (results->NextRow());

            delete results;
        }
    }
    CachedEvent e = eventCache[bot][event];
    if (e.IsEmpty())
    {
        QueryResult* results = PlayerbotDatabase.PQuery(
                "select `value`, `time`, validIn, `data` from ai_playerbot_random_bots where owner = 0 and bot = '%u' and event = '%s'",
                bot, event.c_str());

        if (results)
        {
            Field* fields = results->Fetch();
            e.value = fields[0].GetUInt32();
            e.lastChangeTime = fields[1].GetUInt32();
            e.validIn = fields[2].GetUInt32();
            e.data = fields[3].GetString();
            eventCache[bot][event] = e;
            delete results;
        }
    }

    if ((time(0) - e.lastChangeTime) >= e.validIn && (event == "add" || IsRandomBot(bot)) && event != "specNo" && event != "specLink")
        e.value = 0;

    return e.value;
}

string RandomPlayerbotMgr::GetEventData(uint32 bot, string event)
{
    string data = "";
    if (GetEventValue(bot, event))
    {
        CachedEvent e = eventCache[bot][event];
        data = e.data;
    }
    return data;
}

uint32 RandomPlayerbotMgr::SetEventValue(uint32 bot, string event, uint32 value, uint32 validIn, string data)
{
    PlayerbotDatabase.PExecute("delete from ai_playerbot_random_bots where owner = 0 and bot = '%u' and event = '%s'",
            bot, event.c_str());
    if (value)
    {
        if (data != "")
        {
            PlayerbotDatabase.PExecute(
                "insert into ai_playerbot_random_bots (owner, bot, `time`, validIn, event, `value`, `data`) values ('%u', '%u', '%u', '%u', '%s', '%u', '%s')",
                0, bot, (uint32)time(0), validIn, event.c_str(), value, data.c_str());
        }
        else
        {
            PlayerbotDatabase.PExecute(
                "insert into ai_playerbot_random_bots (owner, bot, `time`, validIn, event, `value`) values ('%u', '%u', '%u', '%u', '%s', '%u')",
                0, bot, (uint32)time(0), validIn, event.c_str(), value);
        }
    }

    CachedEvent e(value, (uint32)time(0), validIn, data);
    eventCache[bot][event] = e;
    return value;
}

uint32 RandomPlayerbotMgr::GetValue(uint32 bot, string type)
{
    return GetEventValue(bot, type);
}

uint32 RandomPlayerbotMgr::GetValue(Player* bot, string type)
{
    return GetValue(bot->GetObjectGuid().GetCounter(), type);
}

string RandomPlayerbotMgr::GetData(uint32 bot, string type)
{
    return GetEventData(bot, type);
}

void RandomPlayerbotMgr::SetValue(uint32 bot, string type, uint32 value, string data)
{
    SetEventValue(bot, type, value, sPlayerbotAIConfig.maxRandomBotInWorldTime, data);
}

void RandomPlayerbotMgr::SetValue(Player* bot, string type, uint32 value, string data)
{
    SetValue(bot->GetObjectGuid().GetCounter(), type, value, data);
}

bool RandomPlayerbotMgr::HandlePlayerbotConsoleCommand(ChatHandler* handler, char const* args)
{
    if (!sPlayerbotAIConfig.enabled)
    {
        sLog.outError("Playerbot system is currently disabled!");
        return false;
    }

    if (!args || !*args)
    {
        sLog.outError("Usage: rndbot stats/update/reset/init/refresh/add/remove");
        return false;
    }

    string cmd = args;

    if (cmd == "reset")
    {
        PlayerbotDatabase.PExecute("delete from ai_playerbot_random_bots");
        sRandomPlayerbotMgr.eventCache.clear();
        sLog.outString("Random bots were reset for all players. Please restart the Server.");
        return true;
    }

    if (cmd == "stats")
    {
        activatePrintStatsThread();
        return true;
    }

    if (cmd == "update")
    {
        sRandomPlayerbotMgr.UpdateAIInternal(0);
        return true;
    }

    map<string, ConsoleCommandHandler> handlers;

    for (map<string, ConsoleCommandHandler>::iterator j = handlers.begin(); j != handlers.end(); ++j)
    {
        string prefix = j->first;
        if (cmd.find(prefix) != 0) continue;
        string name = cmd.size() > prefix.size() + 1 ? cmd.substr(1 + prefix.size()) : "%";

        list<uint32> botIds;
        for (list<uint32>::iterator i = sPlayerbotAIConfig.randomBotAccounts.begin(); i != sPlayerbotAIConfig.randomBotAccounts.end(); ++i)
        {
            uint32 account = *i;
            if (QueryResult* results = CharacterDatabase.PQuery("SELECT guid FROM characters where account = '%u' and name like '%s'",
                    account, name.c_str()))
            {
                do
                {
                    Field* fields = results->Fetch();

                    uint32 botId = fields[0].GetUInt32();
                    ObjectGuid guid = ObjectGuid(HIGHGUID_PLAYER, botId);
                    Player* bot = sObjectMgr.GetPlayer(guid);
                    if (!bot)
                        continue;

                    botIds.push_back(botId);
                } while (results->NextRow());
				delete results;
			}
        }

        if (botIds.empty())
        {
            sLog.outString("Nothing to do");
            return false;
        }

        int processed = 0;
        for (list<uint32>::iterator i = botIds.begin(); i != botIds.end(); ++i)
        {
            ObjectGuid guid = ObjectGuid(HIGHGUID_PLAYER, *i);
            Player* bot = sObjectMgr.GetPlayer(guid);
            if (!bot)
                continue;

            sLog.outString("[%u/%zu] Processing command '%s' for bot '%s'",
                    processed++, botIds.size(), cmd.c_str(), bot->GetName());

            ConsoleCommandHandler handler = j->second;
            (sRandomPlayerbotMgr.*handler)(bot);
        }
        return true;
    }

    list<string> messages = sRandomPlayerbotMgr.HandlePlayerbotCommand(args, NULL);
    for (list<string>::iterator i = messages.begin(); i != messages.end(); ++i)
    {
        sLog.outString("%s",i->c_str());
    }
    return true;
}

void RandomPlayerbotMgr::HandleCommand(uint32 type, const string& text, Player& fromPlayer)
{
    for (PlayerBotMap::const_iterator it = GetPlayerBotsBegin(); it != GetPlayerBotsEnd(); ++it)
    {
        Player* const bot = it->second;
        bot->GetPlayerbotAI()->HandleCommand(type, text, fromPlayer);
    }
}

void RandomPlayerbotMgr::OnPlayerLogout(Player* player)
{
     DisablePlayerBot(player->GetObjectGuid().GetRawValue());

    for (PlayerBotMap::const_iterator it = GetPlayerBotsBegin(); it != GetPlayerBotsEnd(); ++it)
    {
        Player* const bot = it->second;
        PlayerbotAI* ai = bot->GetPlayerbotAI();
        if (player == ai->GetMaster())
        {
            ai->SetMaster(NULL);
            if (!bot->InBattleGround())
            {
                ai->ResetStrategies();
            }
        }
    }

    vector<Player*>::iterator i = find(players.begin(), players.end(), player);
    if (i != players.end())
        players.erase(i);
}

void RandomPlayerbotMgr::OnBotLoginInternal(Player * const bot)
{
    sLog.outDetail("%lu/%d Bot %s logged in", playerBots.size(), sRandomPlayerbotMgr.GetMaxAllowedBotCount(), bot->GetName());
	//if (loginProgressBar && playerBots.size() < sRandomPlayerbotMgr.GetMaxAllowedBotCount()) { loginProgressBar->step(); }
	//if (loginProgressBar && playerBots.size() >= sRandomPlayerbotMgr.GetMaxAllowedBotCount() - 1) {
    //if (loginProgressBar && playerBots.size() + 1 >= sRandomPlayerbotMgr.GetMaxAllowedBotCount()) {
	//	sLog.outString("All bots logged in");
    //    delete loginProgressBar;
	//}
}

void RandomPlayerbotMgr::OnPlayerLogin(Player* player)
{
    for (PlayerBotMap::const_iterator it = GetPlayerBotsBegin(); it != GetPlayerBotsEnd(); ++it)
    {
        Player* const bot = it->second;
        if (player == bot/* || player->GetPlayerbotAI()*/) // TEST
            continue;

        Group* group = bot->GetGroup();
        if (!group)
            continue;

        for (GroupReference *gref = group->GetFirstMember(); gref; gref = gref->next())
        {
            Player* member = gref->getSource();
            PlayerbotAI* ai = bot->GetPlayerbotAI();
            if (member == player && (!ai->GetMaster() || ai->GetMaster()->GetPlayerbotAI()))
            {
                if (!bot->InBattleGround())
                {
                    ai->SetMaster(player);
                    ai->ResetStrategies();
                    ai->TellMaster("Hello");
                }
                break;
            }
        }
    }

    if (IsRandomBot(player))
    {
        uint64 guid = player->GetObjectGuid().GetRawValue();
        SetEventValue((uint32)guid, "login", 0, 0);
    }
    else
    {
        players.push_back(player);
        sLog.outDebug("Including non-random bot player %s into random bot update", player->GetName());
    }
}

void RandomPlayerbotMgr::OnPlayerLoginError(uint32 bot)
{
    SetEventValue(bot, "add", 0, 0);
    currentBots.remove(bot);
}

Player* RandomPlayerbotMgr::GetRandomPlayer()
{
    if (players.empty())
        return NULL;

    uint32 index = urand(0, players.size() - 1);
    return players[index];
}

void RandomPlayerbotMgr::PrintStats()
{
    sLog.outString("%lu Random Bots online", playerBots.size());

    map<uint32, int> alliance, horde;
    for (uint32 i = 0; i < 10; ++i)
    {
        alliance[i] = 0;
        horde[i] = 0;
    }

    map<uint8, int> perRace, perClass;
    for (uint8 race = RACE_HUMAN; race < MAX_RACES; ++race)
    {
        perRace[race] = 0;
    }
    for (uint8 cls = CLASS_WARRIOR; cls < MAX_CLASSES; ++cls)
    {
        perClass[cls] = 0;
    }

    int dps = 0, heal = 0, tank = 0, active = 0, randomize = 0, teleport = 0, changeStrategy = 0, dead = 0, revive = 0;
    int stateCount[MAX_TRAVEL_STATE + 1] = { 0 };
    vector<pair<Quest const*, int32>> questCount;
    for (PlayerBotMap::iterator i = playerBots.begin(); i != playerBots.end(); ++i)
    {
        Player* bot = i->second;
        if (IsAlliance(bot->getRace()))
            alliance[bot->GetLevel() / 10]++;
        else
            horde[bot->GetLevel() / 10]++;

        perRace[bot->getRace()]++;
        perClass[bot->getClass()]++;

        active++;

        uint32 botId = (uint32)bot->GetObjectGuid().GetRawValue();
        if (!GetEventValue(botId, "randomize"))
            randomize++;

        if (!GetEventValue(botId, "teleport"))
            teleport++;

        if (!GetEventValue(botId, "change_strategy"))
            changeStrategy++;

        if (sServerFacade.UnitIsDead(bot))
        {
            dead++;
            if (!GetEventValue(botId, "dead"))
                revive++;
        }

        int spec = AiFactory::GetPlayerSpecTab(bot);
        switch (bot->getClass())
        {
        case CLASS_DRUID:
            if (spec == 2)
                heal++;
            else
                dps++;
            break;
        case CLASS_PALADIN:
            if (spec == 1)
                tank++;
            else if (spec == 0)
                heal++;
            else
                dps++;
            break;
        case CLASS_PRIEST:
            if (spec != 2)
                heal++;
            else
                dps++;
            break;
        case CLASS_SHAMAN:
            if (spec == 2)
                heal++;
            else
                dps++;
            break;
        case CLASS_WARRIOR:
            if (spec == 2)
                tank++;
            else
                dps++;
            break;
#ifdef MANGOSBOT_TWO
        case CLASS_DEATH_KNIGHT:
            if (spec == 0)
                tank++;
            else
                dps++;
            break;
#endif
        default:
            dps++;
            break;
        }

        TravelTarget* target = bot->GetPlayerbotAI()->GetAiObjectContext()->GetValue<TravelTarget*>("travel target")->Get();
        if (target)
        {
            TravelState state = target->getTravelState();
            stateCount[state]++;

            Quest const* quest;

            if (target->getDestination())
                quest = target->getDestination()->GetQuestTemplate();


            if (quest)
            {
                bool found = false;

                for (auto& q : questCount)
                {
                    if (q.first != quest)
                        continue;

                    q.second++;
                    found = true;
                }

                if (!found)
                    questCount.push_back(make_pair(quest, 1));
            }
        }
    }

    sLog.outString("Per level:");
	uint32 maxLevel = sWorld.getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL);
	for (uint32 i = 0; i < 10; ++i)
    {
        if (!alliance[i] && !horde[i])
            continue;

        uint32 from = i*10;
        uint32 to = min(from + 9, maxLevel);
        if (!from) from = 1;
        sLog.outString("    %d..%d: %d alliance, %d horde", from, to, alliance[i], horde[i]);
    }
    sLog.outString("Per race:");
    for (uint8 race = RACE_HUMAN; race < MAX_RACES; ++race)
    {
        if (perRace[race])
            sLog.outString("    %s: %d", ChatHelper::formatRace(race).c_str(), perRace[race]);
    }
    sLog.outString("Per class:");
    for (uint8 cls = CLASS_WARRIOR; cls < MAX_CLASSES; ++cls)
    {
        if (perClass[cls])
            sLog.outString("    %s: %d", ChatHelper::formatClass(cls).c_str(), perClass[cls]);
    }
    sLog.outString("Per role:");
    sLog.outString("    tank: %d", tank);
    sLog.outString("    heal: %d", heal);
    sLog.outString("    dps: %d", dps);

    sLog.outString("Active bots: %d", active);
    sLog.outString("Dead bots: %d", dead);
    sLog.outString("Bots to:");
    sLog.outString("    randomize: %d", randomize);
    sLog.outString("    teleport: %d", teleport);
    sLog.outString("    change_strategy: %d", changeStrategy);
    sLog.outString("    revive: %d", revive);

    sLog.outString("Bots travel:");
    sLog.outString("    travel to pick up quest: %d", stateCount[TRAVEL_STATE_TRAVEL_PICK_UP_QUEST]);
    sLog.outString("    try to pick up quest: %d", stateCount[TRAVEL_STATE_WORK_PICK_UP_QUEST]);
    sLog.outString("    travel to do quest: %d", stateCount[TRAVEL_STATE_TRAVEL_DO_QUEST]);
    sLog.outString("    try to do quest: %d", stateCount[TRAVEL_STATE_WORK_DO_QUEST]);
    sLog.outString("    travel to hand in quest: %d", stateCount[TRAVEL_STATE_TRAVEL_HAND_IN_QUEST]);
    sLog.outString("    try to hand in quest: %d", stateCount[TRAVEL_STATE_WORK_HAND_IN_QUEST]);
    sLog.outString("    idling: %d", stateCount[TRAVEL_STATE_IDLE]);


    sort(questCount.begin(), questCount.end(), [](pair<Quest const*, int32> i, pair<Quest const*, int32> j) {return i.second > j.second; });

    sLog.outString("Bots top quests:");

    int cnt = 0;
    for (auto& quest : questCount)
    {
        sLog.outString("    [%d]: %s (%d)", quest.second, quest.first->GetTitle().c_str(), quest.first->GetQuestLevel());
        cnt++;
        if (cnt > 25)
            break;
    }
}

double RandomPlayerbotMgr::GetBuyMultiplier(Player* bot)
{
    uint32 id = bot->GetGUIDLow();
    uint32 value = GetEventValue(id, "buymultiplier");
    if (!value)
    {
        value = urand(50, 120);
        uint32 validIn = urand(sPlayerbotAIConfig.minRandomBotsPriceChangeInterval, sPlayerbotAIConfig.maxRandomBotsPriceChangeInterval);
        SetEventValue(id, "buymultiplier", value, validIn);
    }

    return (double)value / 100.0;
}

double RandomPlayerbotMgr::GetSellMultiplier(Player* bot)
{
    uint32 id = bot->GetGUIDLow();
    uint32 value = GetEventValue(id, "sellmultiplier");
    if (!value)
    {
        value = urand(80, 250);
        uint32 validIn = urand(sPlayerbotAIConfig.minRandomBotsPriceChangeInterval, sPlayerbotAIConfig.maxRandomBotsPriceChangeInterval);
        SetEventValue(id, "sellmultiplier", value, validIn);
    }

    return (double)value / 100.0;
}

void RandomPlayerbotMgr::AddTradeDiscount(Player* bot, Player* master, int32 value)
{
    if (!master) return;
    uint32 discount = GetTradeDiscount(bot, master);
    int32 result = (int32)discount + value;
    discount = (result < 0 ? 0 : result);

    SetTradeDiscount(bot, master, discount);
}

void RandomPlayerbotMgr::SetTradeDiscount(Player* bot, Player* master, uint32 value)
{
    if (!master) return;
    uint32 botId =  bot->GetGUIDLow();
    uint32 masterId =  master->GetGUIDLow();
    ostringstream name; name << "trade_discount_" << masterId;
    SetEventValue(botId, name.str(), value, sPlayerbotAIConfig.maxRandomBotInWorldTime);
}

uint32 RandomPlayerbotMgr::GetTradeDiscount(Player* bot, Player* master)
{
    if (!master) return 0;
    uint32 botId =  bot->GetGUIDLow();
    uint32 masterId = master->GetGUIDLow();
    ostringstream name; name << "trade_discount_" << masterId;
    return GetEventValue(botId, name.str());
}

string RandomPlayerbotMgr::HandleRemoteCommand(string request)
{
    string::iterator pos = find(request.begin(), request.end(), ',');
    if (pos == request.end())
    {
        ostringstream out; out << "invalid request: " << request;
        return out.str();
    }

    string command = string(request.begin(), pos);
    uint64 guid = atoi(string(pos + 1, request.end()).c_str());
    Player* bot = GetPlayerBot(guid);
    if (!bot)
        return "invalid guid";

    PlayerbotAI *ai = bot->GetPlayerbotAI();
    if (!ai)
        return "invalid guid";

    return ai->HandleRemoteCommand(command);
}

void RandomPlayerbotMgr::RandomTeleportForRpg(Player* bot)
{
    uint32 race = bot->getRace();
	uint32 level = bot->GetLevel();
    sLog.outDetail("Random teleporting bot %s for RPG (%zu locations available)", bot->GetName(), rpgLocsCacheLevel[race][level].size());
    RandomTeleport(bot, rpgLocsCacheLevel[race][level], true);
	Refresh(bot);
}

void RandomPlayerbotMgr::Remove(Player* bot)
{
    uint64 owner = bot->GetObjectGuid().GetRawValue();
    PlayerbotDatabase.PExecute("delete from ai_playerbot_random_bots where owner = 0 and bot = '%lu'", owner);
    eventCache[owner].clear();

    LogoutPlayerBot(owner);
}

const CreatureDataPair* RandomPlayerbotMgr::GetCreatureDataByEntry(uint32 entry)
{
    if (entry != 0 && ObjectMgr::GetCreatureTemplate(entry))
    {
        FindCreatureData worker(entry, NULL);
        sObjectMgr.DoCreatureData(worker);
        CreatureDataPair const* dataPair = worker.GetResult();
        return dataPair;
    }
    return NULL;
}

uint32 RandomPlayerbotMgr::GetCreatureGuidByEntry(uint32 entry)
{
    uint32 guid = 0;

    CreatureDataPair const* dataPair = sRandomPlayerbotMgr.GetCreatureDataByEntry(entry);
    guid = dataPair->first;

    return guid;
}

uint32 RandomPlayerbotMgr::GetBattleMasterEntry(Player* bot, BattleGroundTypeId bgTypeId, bool fake)
{
    Team team = bot->GetTeam();
    uint32 entry = 0;
    vector<uint32> Bms;

    for (auto i = begin(BattleMastersCache[team][bgTypeId]); i != end(BattleMastersCache[team][bgTypeId]); ++i)
    {
        Bms.insert(Bms.end(), *i);
    }

    for (auto i = begin(BattleMastersCache[TEAM_BOTH_ALLOWED][bgTypeId]); i != end(BattleMastersCache[TEAM_BOTH_ALLOWED][bgTypeId]); ++i)
    {
        Bms.insert(Bms.end(), *i);
    }

    if (Bms.empty())
        return entry;

    float dist1 = FLT_MAX;

    for (auto i = begin(Bms); i != end(Bms); ++i)
    {
        CreatureDataPair const* dataPair = sRandomPlayerbotMgr.GetCreatureDataByEntry(*i);
        if (!dataPair)
            continue;

        CreatureData const* data = &dataPair->second;

        Unit* Bm = sMapMgr.FindMap((uint32)data->mapid)->GetUnit(ObjectGuid(HIGHGUID_UNIT, *i, dataPair->first));
        if (!Bm)
            continue;

        if (bot->GetMapId() != Bm->GetMapId())
            continue;

        // return first available guid on map if queue from anywhere
        if (fake)
        {
            entry = *i;
            break;
        }

        AreaTableEntry const* area = GetAreaEntryByAreaID(Bm->GetAreaId());
        if (!area)
            continue;

        if (area->team == 4 && bot->GetTeam() == ALLIANCE)
            continue;
        if (area->team == 2 && bot->GetTeam() == HORDE)
            continue;

        if (Bm->GetDeathState() == DEAD)
            continue;

        float dist2 = sServerFacade.GetDistance2d(bot, data->posX, data->posY);
        if (dist2 < dist1)
        {
            dist1 = dist2;
            entry = *i;
        }
    }

    return entry;
}

void RandomPlayerbotMgr::Hotfix(Player* bot, uint32 version)
{
    PlayerbotFactory factory(bot, bot->GetLevel());
    uint32 exp = bot->GetUInt32Value(PLAYER_XP);
    uint32 level = bot->GetLevel();
    uint32 id = bot->GetGUIDLow();

    for (int fix = version; fix <= MANGOSBOT_VERSION; fix++)
    {
        int count = 0;
        switch (fix)
        {
            case 1: // Apply class quests to previously made random bots

                if (level < 10)
                {
                    break;
                }

                for (list<uint32>::iterator i = factory.classQuestIds.begin(); i != factory.classQuestIds.end(); ++i)
                {
                    uint32 questId = *i;
                    Quest const *quest = sObjectMgr.GetQuestTemplate(questId);

                    if (!bot->SatisfyQuestClass(quest, false) ||
                        quest->GetMinLevel() > bot->GetLevel() ||
                        !bot->SatisfyQuestRace(quest, false) || bot->GetQuestStatus(questId) == QUEST_STATUS_COMPLETE)
                        continue;

                    bot->SetQuestStatus(questId, QUEST_STATUS_COMPLETE);
                    bot->RewardQuest(quest, 0, bot, false);
                    bot->SetLevel(level);
                    bot->SetUInt32Value(PLAYER_XP, exp);
                    sLog.outDetail("Bot %d rewarded quest %d",
                        bot->GetGUIDLow(), questId);
                    count++;
                }

                if (count > 0)
                {
                    sLog.outDetail("Bot %d hotfix (Class Quests), %d quests rewarded",
                        bot->GetGUIDLow(), count);
                    count = 0;
                }
                break;
            case 2: // Init Riding skill fix

                if (level < 20)
                {
                    break;
                }
                factory.InitSkills();
                sLog.outDetail("Bot %d hotfix (Riding Skill) applied",
                    bot->GetGUIDLow());
                break;

            default:
                break;
        }
    }
    SetValue(bot, "version", MANGOSBOT_VERSION);
    sLog.outBasic("Bot %d hotfix v%d applied",
        bot->GetGUIDLow(), MANGOSBOT_VERSION);
}
