#include "botpch.h"
#include "../../playerbot.h"
#include "StayActions.h"

#include "../../ServerFacade.h"
#include "../values/LastMovementValue.h"

using namespace ai;

bool StayActionBase::Stay()
{
    AI_VALUE(LastMovement&, "last movement").Set(NULL);

    MotionMaster &mm = *bot->GetMotionMaster();
#ifdef CMANGOS
	if (mm.GetCurrentMovementGeneratorType() == TAXI_MOTION_TYPE || bot->IsFlying())
#endif
#ifdef MANGOS
	if (mm.GetCurrentMovementGeneratorType() == FLIGHT_MOTION_TYPE || bot->IsFlying())
#endif
	{
		if (verbose) ai->TellError("I can not stay, I'm flying!");
		return false;
	} 
    
    time_t stayTime = AI_VALUE(time_t, "stay time");
    time_t now = time(0);
    if (!stayTime)
    {
        stayTime = now - 1;
        context->GetValue<time_t>("stay time")->Set(stayTime);
    }

    if (!sServerFacade.isMoving(bot))
        return false;

    bot->StopMoving();
	bot->clearUnitState(UNIT_STAT_CHASE);
	bot->clearUnitState(UNIT_STAT_FOLLOW);

    return true;
}

bool StayAction::Execute(Event event)
{
    return Stay();
}

bool StayAction::isUseful()
{
    return !AI_VALUE2(bool, "moving", "self target");
}

bool SitAction::Execute(Event event)
{
    if (sServerFacade.isMoving(bot))
        return false;

    bot->SetStandState(UNIT_STAND_STATE_SIT);
    return true;
}

bool SitAction::isUseful()
{
    return !AI_VALUE2(bool, "moving", "self target");
}
