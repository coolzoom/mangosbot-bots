#pragma once

#include "../../PlayerbotAIConfig.h"
#include "../../RandomPlayerbotMgr.h"
#include "../../ServerFacade.h"
#include "../Action.h"

namespace ai
{
    class DelayAction : public Action
    {
    public:
        DelayAction(PlayerbotAI* ai) : Action(ai, "delay")
        {}

        virtual bool Execute(Event event)
        {
            uint32 delay = sPlayerbotAIConfig.passiveDelay + sPlayerbotAIConfig.globalCoolDown;

            return true;
        }

        virtual bool isUseful()
        {
            return false;
        }
    };

}
