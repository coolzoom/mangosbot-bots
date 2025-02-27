#include "botpch.h"
#include "../../playerbot.h"
#include "GenericActions.h"

using namespace ai;

bool CastSpellAction::Execute(Event event)
{
    if (spell == "conjure food" || spell == "conjure water")
    {
        //uint32 id = AI_VALUE2(uint32, "spell id", spell);
        //if (!id)
        //    return false;

        uint32 castId = 0;

        for (PlayerSpellMap::iterator itr = bot->GetSpellMap().begin(); itr != bot->GetSpellMap().end(); ++itr)
        {
            uint32 spellId = itr->first;

            const SpellEntry* pSpellInfo = sServerFacade.LookupSpellInfo(spellId);
            if (!pSpellInfo)
                continue;

            string namepart = pSpellInfo->SpellName[0];
            strToLower(namepart);

            if (namepart.find(spell) == string::npos)
                continue;

            if (pSpellInfo->Effect[0] != SPELL_EFFECT_CREATE_ITEM)
                continue;

            uint32 itemId = pSpellInfo->EffectItemType[0];
            ItemPrototype const *proto = sObjectMgr.GetItemPrototype(itemId);
            if (!proto)
                continue;

            if (bot->CanUseItem(proto) != EQUIP_ERR_OK)
                continue;

            if (pSpellInfo->Id > castId)
                castId = pSpellInfo->Id;
        }
        return ai->CastSpell(castId, bot);
    }

	return ai->CastSpell(spell, GetTarget());
}

bool CastSpellAction::isPossible()
{
    if (spell == "mount" && !bot->IsMounted() && !bot->IsInCombat())
        return true;
    if (spell == "mount" && bot->IsInCombat())
    {
        bot->Unmount();
        return false;
    }

    Spell* currentSpell = bot->GetCurrentSpell(CURRENT_GENERIC_SPELL);

	return ai->CanCastSpell(spell, GetTarget());
}

bool CastSpellAction::isUseful()
{
    if (spell == "mount" && !bot->IsMounted() && !bot->IsInCombat())
        return true;
    if (spell == "mount" && bot->IsInCombat())
    {
        bot->Unmount();
        return false;
    }

    Unit* target = GetTarget();

    if (!target)
        return false;

    float maxRange = range;

    uint32 spellid = AI_VALUE2(uint32, "spell id", spell);
    if (spellid)
    {
        if (SpellEntry const* spellInfo = sServerFacade.LookupSpellInfo(spellid))
        {
            if (SpellRangeEntry const* spellRange = sSpellRangeStore.LookupEntry(spellInfo->rangeIndex))
            {
                maxRange = spellRange->maxRange;

                if (Player* modOwner = bot->GetSpellModOwner())
                    modOwner->ApplySpellMod(spellInfo->Id, SPELLMOD_RANGE, maxRange);
            }
        }
    }

    return target && AI_VALUE2(bool, "spell cast useful", spell) && bot->GetDistance(target, true, DIST_CALC_COMBAT_REACH) <= maxRange;
    return GetTarget() && (GetTarget() != nullptr) && (GetTarget() != NULL) && AI_VALUE2(bool, "spell cast useful", spell) && sServerFacade.GetDistance2d(bot, GetTarget()) <= range;
}

bool CastAuraSpellAction::isUseful()
{
    return GetTarget() && (GetTarget() != nullptr) && (GetTarget() != NULL) && CastSpellAction::isUseful() && !ai->HasAura(spell, GetTarget(), true);
}

bool CastEnchantItemAction::isPossible()
{
    if (!CastSpellAction::isPossible())
        return false;

    uint32 spellId = AI_VALUE2(uint32, "spell id", spell);
    return spellId && AI_VALUE2(Item*, "item for spell", spellId);
}

bool CastHealingSpellAction::isUseful()
{
    return CastAuraSpellAction::isUseful();
}

bool CastAoeHealSpellAction::isUseful()
{
    return CastSpellAction::isUseful();
}

Value<Unit*>* CurePartyMemberAction::GetTargetValue()
{
    return context->GetValue<Unit*>("party member to dispel", dispelType);
}

Value<Unit*>* BuffOnPartyAction::GetTargetValue()
{
    return context->GetValue<Unit*>("party member without aura", spell);
}
