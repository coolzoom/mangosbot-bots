#include "botpch.h"
#include "../../playerbot.h"
#include "MoveToTravelTargetAction.h"
#include "../../PlayerbotAIConfig.h"
#include "../../ServerFacade.h"
#include "../../LootObjectStack.h"
#include "MotionGenerators/PathFinder.h"
#include "ChooseRpgTargetAction.h"

using namespace ai;

bool MoveToTravelTargetAction::Execute(Event event)
{
    TravelTarget* target = AI_VALUE(TravelTarget*, "travel target");

    WorldPosition botLocation(bot);
    WorldLocation location = target->getLocation();
    
    Group* group = bot->GetGroup();
    if (group && !urand(0, 1) && bot == ai->GetGroupMaster())
    {        
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            Player* member = ref->getSource();
            if (member == bot)
                continue;

            if (!member->IsAlive())
                continue;

            if (!member->IsMoving())
                continue;

            if (member->GetPlayerbotAI() && !member->GetPlayerbotAI()->HasStrategy("follow", BOT_STATE_NON_COMBAT))
                continue;

            WorldPosition memberPos(member);
            WorldPosition targetPos = *target->getPosition();

            float memberDistance = botLocation.distance(memberPos);

            if (memberDistance < 50.0f)
                continue;
            if (memberDistance > sPlayerbotAIConfig.reactDistance * 20)
                continue;

           // float memberAngle = botLocation.getAngleBetween(targetPos, memberPos);

           // if (botLocation.getMapId() == targetPos.getMapId() && botLocation.getMapId() == memberPos.getMapId() && memberAngle < M_PI_F / 2) //We are heading that direction anyway.
           //     continue;

            if (!urand(0, 5))
            {
                ostringstream out;
                if (ai->GetMaster() && !bot->GetGroup()->IsMember(ai->GetMaster()->GetObjectGuid()))
                    out << "Waiting a bit for ";
                else
                    out << "Please hurry up ";

                out << member->GetName();

                ai->TellMasterNoFacing(out);
            }

            target->setExpireIn(target->getTimeLeft() + sPlayerbotAIConfig.maxWaitForMove);           

            return true;
        }
    }

    float maxDistance = target->getDestination()->getRadiusMin();

    //Evenly distribute around the target.
    float angle = 2 * M_PI * urand(0, 100) / 100.0;

    float x = location.coord_x;
    float y = location.coord_y;
    float z = location.coord_z;
    float mapId = location.mapid;

    //Move between 0.5 and 1.0 times the maxDistance.
    float mod = urand(50, 100)/100.0;   

    x += cos(angle) * maxDistance * mod;
    y += sin(angle) * maxDistance * mod;

    bool canMove = false;

    if (bot->IsWithinLOS(x, y, z))
        canMove = MoveNear(mapId, x, y, z, 0);
    else
        canMove = MoveTo(mapId, x, y, z, false, false);


    if (!canMove)
    {
        target->incRetry(true);

        if (target->isMaxRetry(true))
            target->setStatus(TRAVEL_STATUS_COOLDOWN);
    }    
    else
    {
        //target->setRetry(true);
    }
     
    return canMove;
}

bool MoveToTravelTargetAction::isUseful()
{
    if (!context->GetValue<TravelTarget*>("travel target")->Get()->isTraveling())
        return false;

    TravelTarget* target = context->GetValue<TravelTarget*>("travel target")->Get();
    if (!target || !target->getPosition())
        return false;

    if (bot->IsTaxiFlying())
        return false;

    if (bot->IsFlying())
        return false;

    if (bot->IsMoving())
        return false;

    if (bot->IsInCombat())
        return false;

    if (bot->IsNonMeleeSpellCasted(false))
        return false;

    if (!ChooseRpgTargetAction::isFollowValid(bot, context->GetValue<TravelTarget*>("travel target")->Get()->getLocation()))
        return false;

    return true;
}

