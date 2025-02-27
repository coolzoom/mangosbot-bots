#include "botpch.h"
#include "../../playerbot.h"
#include "WorldPacketHandlerStrategy.h"

using namespace ai;

void WorldPacketHandlerStrategy::InitTriggers(std::list<TriggerNode*> &triggers)
{
    PassTroughStrategy::InitTriggers(triggers);

    triggers.push_back(new TriggerNode(
        "group invite",
        NextAction::array(0, new NextAction("accept invitation", relevance), NULL)));

    triggers.push_back(new TriggerNode(
        "uninvite",
        NextAction::array(0, new NextAction("uninvite", relevance), NULL)));

    triggers.push_back(new TriggerNode(
        "uninvite guid",
        NextAction::array(0, new NextAction("uninvite", relevance), NULL)));

    triggers.push_back(new TriggerNode(
        "group set leader",
        NextAction::array(0, new NextAction("leader", relevance), NULL)));

    triggers.push_back(new TriggerNode(
        "not enough money",
        NextAction::array(0, new NextAction("tell not enough money", relevance), NULL)));

    triggers.push_back(new TriggerNode(
        "not enough reputation",
        NextAction::array(0, new NextAction("tell not enough reputation", relevance), NULL)));

    triggers.push_back(new TriggerNode(
        "cannot equip",
        NextAction::array(0, new NextAction("tell cannot equip", relevance), NULL)));

    triggers.push_back(new TriggerNode(
        "use game object",
        NextAction::array(0,
            new NextAction("add loot", relevance),
            new NextAction("use meeting stone", relevance), NULL)));

    triggers.push_back(new TriggerNode(
        "gossip hello",
        NextAction::array(0,
            new NextAction("trainer", relevance), NULL)));

    triggers.push_back(new TriggerNode(
        "activate taxi",
        NextAction::array(0, new NextAction("remember taxi", relevance), new NextAction("taxi", relevance), NULL)));

    triggers.push_back(new TriggerNode(
        "taxi done",
        NextAction::array(0, new NextAction("taxi", relevance), NULL)));

    triggers.push_back(new TriggerNode(
        "trade status",
        NextAction::array(0, new NextAction("accept trade", relevance), new NextAction("equip upgrades", relevance), NULL)));

    triggers.push_back(new TriggerNode(
        "area trigger",
        NextAction::array(0, new NextAction("reach area trigger", relevance), NULL)));

    triggers.push_back(new TriggerNode(
        "within area trigger",
        NextAction::array(0, new NextAction("area trigger", relevance), NULL)));

    triggers.push_back(new TriggerNode(
        "loot response",
        NextAction::array(0, new NextAction("store loot", relevance), NULL)));

    triggers.push_back(new TriggerNode(
        "item push result",
        NextAction::array(0, new NextAction("query item usage", relevance), new NextAction("equip upgrades", relevance), NULL)));

    triggers.push_back(new TriggerNode(
        "ready check finished",
        NextAction::array(0, new NextAction("finish ready check", relevance), NULL)));

    triggers.push_back(new TriggerNode(
        "often",
        NextAction::array(0, new NextAction("check mail", relevance), NULL)));

    triggers.push_back(new TriggerNode(
        "guild invite",
        NextAction::array(0, new NextAction("guild accept", relevance), NULL)));

    triggers.push_back(new TriggerNode(
        "petition offer",
        NextAction::array(0, new NextAction("petition sign", relevance), NULL)));

    triggers.push_back(new TriggerNode(
        "lfg proposal",
        NextAction::array(0, new NextAction("lfg accept", relevance), NULL)));

    triggers.push_back(new TriggerNode(
        "lfg proposal active",
        NextAction::array(0, new NextAction("lfg accept", relevance), NULL)));

    triggers.push_back(new TriggerNode(
        "arena team invite",
        NextAction::array(0, new NextAction("arena team accept", relevance), NULL)));

    /*triggers.push_back(new TriggerNode(
        "no non bot players around",
        NextAction::array(0, new NextAction("delay", relevance), NULL)));*/

    triggers.push_back(new TriggerNode(
        "bg status",
        NextAction::array(0, new NextAction("bg status", relevance), NULL)));

    triggers.push_back(new TriggerNode(
        "xpgain",
        NextAction::array(0, new NextAction("xp gain", relevance), NULL)));

    triggers.push_back(new TriggerNode(
        "levelup",
        NextAction::array(0, new NextAction("increase level", relevance), new NextAction("auto talents", relevance), new NextAction("auto learn spell", relevance), NULL)));

    triggers.push_back(new TriggerNode(
        "see spell",
        NextAction::array(0, new NextAction("see spell", relevance), NULL)));

    triggers.push_back(new TriggerNode(
        "release spirit",
        NextAction::array(0, new NextAction("release", relevance), NULL)));

    triggers.push_back(new TriggerNode(
        "revive from corpse",
        NextAction::array(0, new NextAction("revive from corpse", relevance), NULL)));

    triggers.push_back(new TriggerNode(
        "master loot roll",
        NextAction::array(0, new NextAction("master loot roll", relevance), NULL)));
}

WorldPacketHandlerStrategy::WorldPacketHandlerStrategy(PlayerbotAI* ai) : PassTroughStrategy(ai)
{
    supported.push_back("loot roll");
    supported.push_back("check mount state");
    supported.push_back("quest objective completed");
    supported.push_back("party command");
    supported.push_back("ready check");
    supported.push_back("uninvite");
    supported.push_back("lfg role check");
    supported.push_back("lfg teleport");
    supported.push_back("inventory change failure");
    supported.push_back("bg status");
}


void ReadyCheckStrategy::InitTriggers(std::list<TriggerNode*> &triggers)
{
    triggers.push_back(new TriggerNode(
        "timer",
        NextAction::array(0, new NextAction("ready check", relevance), NULL)));
}
