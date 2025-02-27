#pragma once
#include "../actions/GenericActions.h"

namespace ai
{
    class CastJudgementOfLightAction : public CastMeleeSpellAction
    {
    public:
        CastJudgementOfLightAction(PlayerbotAI* ai) : CastMeleeSpellAction(ai, "judgement of light") {}
    };

    class CastJudgementOfWisdomAction : public CastMeleeSpellAction
    {
    public:
        CastJudgementOfWisdomAction(PlayerbotAI* ai) : CastMeleeSpellAction(ai, "judgement of wisdom") {}
    };   
	
	class CastJudgementAction : public CastMeleeSpellAction
    {
    public:
        CastJudgementAction(PlayerbotAI* ai) : CastMeleeSpellAction(ai, "judgement") {}
    };

    class CastJudgementOfJusticeAction : public CastMeleeSpellAction
    {
    public:
        CastJudgementOfJusticeAction(PlayerbotAI* ai) : CastMeleeSpellAction(ai, "judgement of justice") {}
    };

	class CastRighteousFuryAction : public CastBuffSpellAction
	{
	public:
		CastRighteousFuryAction(PlayerbotAI* ai) : CastBuffSpellAction(ai, "righteous fury") {}
	};

	class CastDevotionAuraAction : public CastBuffSpellAction
	{
	public:
		CastDevotionAuraAction(PlayerbotAI* ai) : CastBuffSpellAction(ai, "devotion aura") {}
	};

	class CastRetributionAuraAction : public CastBuffSpellAction
	{
	public:
		CastRetributionAuraAction(PlayerbotAI* ai) : CastBuffSpellAction(ai, "retribution aura") {}
	};

	class CastSanctityAuraAction : public CastBuffSpellAction
	{
	public:
		CastSanctityAuraAction(PlayerbotAI* ai) : CastBuffSpellAction(ai, "sanctity aura") {}
	};

	class CastConcentrationAuraAction : public CastBuffSpellAction
	{
	public:
		CastConcentrationAuraAction(PlayerbotAI* ai) : CastBuffSpellAction(ai, "concentration aura") {}
	};

	class CastDivineStormAction : public CastBuffSpellAction
	{
	public:
		CastDivineStormAction(PlayerbotAI* ai) : CastBuffSpellAction(ai, "divine storm") {}
	};

	class CastCrusaderStrikeAction : public CastMeleeSpellAction
	{
	public:
		CastCrusaderStrikeAction(PlayerbotAI* ai) : CastMeleeSpellAction(ai, "crusader strike") {}
	};

	class CastShadowResistanceAuraAction : public CastBuffSpellAction
	{
	public:
		CastShadowResistanceAuraAction(PlayerbotAI* ai) : CastBuffSpellAction(ai, "shadow resistance aura") {}
	};

	class CastFrostResistanceAuraAction : public CastBuffSpellAction
	{
	public:
		CastFrostResistanceAuraAction(PlayerbotAI* ai) : CastBuffSpellAction(ai, "frost resistance aura") {}
	};

	class CastFireResistanceAuraAction : public CastBuffSpellAction
	{
	public:
		CastFireResistanceAuraAction(PlayerbotAI* ai) : CastBuffSpellAction(ai, "fire resistance aura") {}
	};

	class CastCrusaderAuraAction : public CastBuffSpellAction
	{
	public:
		CastCrusaderAuraAction(PlayerbotAI* ai) : CastBuffSpellAction(ai, "crusader aura") {}
	};

    class CastSealSpellAction : public CastBuffSpellAction
    {
    public:
        CastSealSpellAction(PlayerbotAI* ai, string name) : CastBuffSpellAction(ai, name) {}
        virtual bool isUseful() { return AI_VALUE2(bool, "combat", "self target"); }
    };

    class CastSealOfRighteousnessAction : public CastSealSpellAction
	{
	public:
        CastSealOfRighteousnessAction(PlayerbotAI* ai) : CastSealSpellAction(ai, "seal of righteousness") {}
	};

	class CastSealOfJusticeAction : public CastSealSpellAction
	{
	public:
		CastSealOfJusticeAction(PlayerbotAI* ai) : CastSealSpellAction(ai, "seal of justice") {}
	};


	class CastSealOfLightAction : public CastSealSpellAction
	{
	public:
		CastSealOfLightAction(PlayerbotAI* ai) : CastSealSpellAction(ai, "seal of light") {}
	};

	class CastSealOfWisdomAction : public CastSealSpellAction
	{
	public:
		CastSealOfWisdomAction(PlayerbotAI* ai) : CastSealSpellAction(ai, "seal of wisdom") {}
	};

	class CastSealOfCommandAction : public CastSealSpellAction
	{
	public:
		CastSealOfCommandAction(PlayerbotAI* ai) : CastSealSpellAction(ai, "seal of command") {}
	};

	class CastSealOfVengeanceAction : public CastSealSpellAction
	{
	public:
	    CastSealOfVengeanceAction(PlayerbotAI* ai) : CastSealSpellAction(ai, "seal of vengeance") {}
	};


	class CastBlessingOfMightAction : public CastBuffSpellAction
	{
	public:
		CastBlessingOfMightAction(PlayerbotAI* ai) : CastBuffSpellAction(ai, "blessing of might") {}
        virtual bool Execute(Event event);
    };

    class CastBlessingOnPartyAction : public BuffOnPartyAction
    {
    public:
        CastBlessingOnPartyAction(PlayerbotAI* ai, string name) : BuffOnPartyAction(ai, name) {}
        virtual Value<Unit*>* GetTargetValue();
    };

    class CastBlessingOfMightOnPartyAction : public CastBlessingOnPartyAction
	{
	public:
        CastBlessingOfMightOnPartyAction(PlayerbotAI* ai) : CastBlessingOnPartyAction(ai, "blessing of might") {}
        virtual string getName() { return "blessing of might on party";}
        virtual bool Execute(Event event);
	};

	class CastBlessingOfWisdomAction : public CastBuffSpellAction
	{
	public:
		CastBlessingOfWisdomAction(PlayerbotAI* ai) : CastBuffSpellAction(ai, "blessing of wisdom") {}
        virtual bool Execute(Event event);
    };

    class CastBlessingOfWisdomOnPartyAction : public CastBlessingOnPartyAction
	{
	public:
        CastBlessingOfWisdomOnPartyAction(PlayerbotAI* ai) : CastBlessingOnPartyAction(ai, "blessing of wisdom") {}
        virtual string getName() { return "blessing of wisdom on party";}
        virtual bool Execute(Event event);
	};

	class CastBlessingOfKingsAction : public CastBuffSpellAction
	{
	public:
		CastBlessingOfKingsAction(PlayerbotAI* ai) : CastBuffSpellAction(ai, "blessing of kings") {}
	};

    class CastBlessingOfKingsOnPartyAction : public CastBlessingOnPartyAction
	{
	public:
        CastBlessingOfKingsOnPartyAction(PlayerbotAI* ai) : CastBlessingOnPartyAction(ai, "blessing of kings") {}
        virtual string getName() { return "blessing of kings on party";}
	};

	class CastBlessingOfSanctuaryAction : public CastBuffSpellAction
	{
	public:
		CastBlessingOfSanctuaryAction(PlayerbotAI* ai) : CastBuffSpellAction(ai, "blessing of sanctuary") {}
	};

    class CastBlessingOfSanctuaryOnPartyAction : public CastBlessingOnPartyAction
	{
	public:
        CastBlessingOfSanctuaryOnPartyAction(PlayerbotAI* ai) : CastBlessingOnPartyAction(ai, "blessing of sanctuary") {}
        virtual string getName() { return "blessing of sanctuary on party";}
	};

	class CastBlessingOfSalvationOnPartyAction : public BuffOnPartyAction
	{
	public:
		CastBlessingOfSalvationOnPartyAction(PlayerbotAI* ai) : BuffOnPartyAction(ai, "blessing of salvation") {}
		virtual string getName() { return "blessing of salvation on party"; }
		virtual bool isUseful() { 
			
			Player* target = (Player*)GetTarget();

			if (target && ai->IsTank(target))
				return false;

			return BuffOnPartyAction::isUseful();
		}
		//virtual Value<Unit*>* GetTargetValue();
	};

    class CastHolyLightAction : public CastHealingSpellAction
    {
    public:
        CastHolyLightAction(PlayerbotAI* ai) : CastHealingSpellAction(ai, "holy light") {}
    };

    class CastHolyLightOnPartyAction : public HealPartyMemberAction
    {
    public:
        CastHolyLightOnPartyAction(PlayerbotAI* ai) : HealPartyMemberAction(ai, "holy light") {}

        virtual string getName() { return "holy light on party"; }
    };

    class CastFlashOfLightAction : public CastHealingSpellAction
    {
    public:
        CastFlashOfLightAction(PlayerbotAI* ai) : CastHealingSpellAction(ai, "flash of light") {}
    };

    class CastFlashOfLightOnPartyAction : public HealPartyMemberAction
    {
    public:
        CastFlashOfLightOnPartyAction(PlayerbotAI* ai) : HealPartyMemberAction(ai, "flash of light") {}

        virtual string getName() { return "flash of light on party"; }
    };

    class CastLayOnHandsAction : public CastHealingSpellAction
    {
    public:
        CastLayOnHandsAction(PlayerbotAI* ai) : CastHealingSpellAction(ai, "lay on hands") {}
    };

    class CastLayOnHandsOnPartyAction : public HealPartyMemberAction
    {
    public:
        CastLayOnHandsOnPartyAction(PlayerbotAI* ai) : HealPartyMemberAction(ai, "lay on hands") {}

        virtual string getName() { return "lay on hands on party"; }
    };

	class CastDivineProtectionAction : public CastBuffSpellAction
	{
	public:
		CastDivineProtectionAction(PlayerbotAI* ai) : CastBuffSpellAction(ai, "divine protection") {}
	};

    class CastDivineProtectionOnPartyAction : public HealPartyMemberAction
    {
    public:
        CastDivineProtectionOnPartyAction(PlayerbotAI* ai) : HealPartyMemberAction(ai, "divine protection") {}

        virtual string getName() { return "divine protection on party"; }
    };

	class CastBlessingOfProtectionOnPartyAction : public HealPartyMemberAction
	{
	public:
		CastBlessingOfProtectionOnPartyAction(PlayerbotAI* ai) : HealPartyMemberAction(ai, "blessing of protection") {}

		virtual string getName() { return "blessing of protection on party"; }
	};

	class CastDivineShieldAction: public CastBuffSpellAction
	{
	public:
		CastDivineShieldAction(PlayerbotAI* ai) : CastBuffSpellAction(ai, "divine shield") {}
	};

    class CastConsecrationAction : public CastMeleeSpellAction
    {
    public:
	    CastConsecrationAction(PlayerbotAI* ai) : CastMeleeSpellAction(ai, "consecration") {}
    }; 
	
	class CastAvengingWrathAction : public CastBuffSpellAction
    {
    public:
		CastAvengingWrathAction(PlayerbotAI* ai) : CastBuffSpellAction(ai, "avenging wrath") {}
    };	
	
	class CastRighteousDefenseAction : public CastSpellAction
    {
    public:
		CastRighteousDefenseAction(PlayerbotAI* ai) : CastSpellAction(ai, "righteous defense") {}
    };

	class CastConsecration1Action : public CastMeleeSpellAction
	{
	public:
		CastConsecration1Action(PlayerbotAI* ai) : CastMeleeSpellAction(ai, "consecration rank 1") {}
	};

    class CastHolyWrathAction : public CastMeleeSpellAction
    {
    public:
        CastHolyWrathAction(PlayerbotAI* ai) : CastMeleeSpellAction(ai, "holy wrath") {}
    };

    class CastHammerOfJusticeAction : public CastMeleeSpellAction
    {
    public:
        CastHammerOfJusticeAction(PlayerbotAI* ai) : CastMeleeSpellAction(ai, "hammer of justice") {}
    };

	class CastHammerOfWrathAction : public CastMeleeSpellAction
	{
	public:
		CastHammerOfWrathAction(PlayerbotAI* ai) : CastMeleeSpellAction(ai, "hammer of wrath") {}
	};

	class CastHammerOfTheRighteousAction : public CastMeleeSpellAction
	{
	public:
		CastHammerOfTheRighteousAction(PlayerbotAI* ai) : CastMeleeSpellAction(ai, "hammer of the righteous") {}
	};

	class CastPurifyPoisonAction : public CastCureSpellAction
	{
	public:
		CastPurifyPoisonAction(PlayerbotAI* ai) : CastCureSpellAction(ai, "purify") {}
	};

	class CastPurifyDiseaseAction : public CastCureSpellAction
	{
	public:
		CastPurifyDiseaseAction(PlayerbotAI* ai) : CastCureSpellAction(ai, "purify") {}
	};

    class CastPurifyPoisonOnPartyAction : public CurePartyMemberAction
    {
    public:
        CastPurifyPoisonOnPartyAction(PlayerbotAI* ai) : CurePartyMemberAction(ai, "purify", DISPEL_POISON) {}

        virtual string getName() { return "purify poison on party"; }
    };

	class CastPurifyDiseaseOnPartyAction : public CurePartyMemberAction
	{
	public:
		CastPurifyDiseaseOnPartyAction(PlayerbotAI* ai) : CurePartyMemberAction(ai, "purify", DISPEL_DISEASE) {}

		virtual string getName() { return "purify disease on party"; }
	};

	class CastHandOfReckoningAction : public CastSpellAction
	{
	public:
		CastHandOfReckoningAction(PlayerbotAI* ai) : CastSpellAction(ai, "hand of reckoning") {}
	};

	class CastCleansePoisonAction : public CastCureSpellAction
	{
	public:
		CastCleansePoisonAction(PlayerbotAI* ai) : CastCureSpellAction(ai, "cleanse") {}
	};

	class CastCleanseDiseaseAction : public CastCureSpellAction
	{
	public:
		CastCleanseDiseaseAction(PlayerbotAI* ai) : CastCureSpellAction(ai, "cleanse") {}
	};

	class CastCleanseMagicAction : public CastCureSpellAction
	{
	public:
		CastCleanseMagicAction(PlayerbotAI* ai) : CastCureSpellAction(ai, "cleanse") {}
	};

    class CastCleansePoisonOnPartyAction : public CurePartyMemberAction
    {
    public:
        CastCleansePoisonOnPartyAction(PlayerbotAI* ai) : CurePartyMemberAction(ai, "cleanse", DISPEL_POISON) {}

        virtual string getName() { return "cleanse poison on party"; }
    };

	class CastCleanseDiseaseOnPartyAction : public CurePartyMemberAction
	{
	public:
		CastCleanseDiseaseOnPartyAction(PlayerbotAI* ai) : CurePartyMemberAction(ai, "cleanse", DISPEL_DISEASE) {}

		virtual string getName() { return "cleanse disease on party"; }
	};

	class CastCleanseMagicOnPartyAction : public CurePartyMemberAction
	{
	public:
		CastCleanseMagicOnPartyAction(PlayerbotAI* ai) : CurePartyMemberAction(ai, "cleanse", DISPEL_MAGIC) {}

		virtual string getName() { return "cleanse magic on party"; }
	};

    BEGIN_SPELL_ACTION(CastAvengersShieldAction, "avenger's shield")
    END_SPELL_ACTION()

	BEGIN_SPELL_ACTION(CastExorcismAction, "exorcism")
	END_SPELL_ACTION()

	class CastHolyShieldAction : public CastBuffSpellAction
	{
	public:
		CastHolyShieldAction(PlayerbotAI* ai) : CastBuffSpellAction(ai, "holy shield") {}
	};

	class CastRedemptionAction : public ResurrectPartyMemberAction
	{
	public:
		CastRedemptionAction(PlayerbotAI* ai) : ResurrectPartyMemberAction(ai, "redemption") {}
	};

    class CastHammerOfJusticeOnEnemyHealerAction : public CastSpellOnEnemyHealerAction
    {
    public:
        CastHammerOfJusticeOnEnemyHealerAction(PlayerbotAI* ai) : CastSpellOnEnemyHealerAction(ai, "hammer of justice") {}
    };

    class CastHammerOfJusticeSnareAction : public CastSnareSpellAction
    {
    public:
        CastHammerOfJusticeSnareAction(PlayerbotAI* ai) : CastSnareSpellAction(ai, "hammer of justice") {}
    };

    class CastDivineFavorAction : public CastBuffSpellAction
    {
    public:
        CastDivineFavorAction(PlayerbotAI* ai) : CastBuffSpellAction(ai, "divine favor") {}
    };

    class CastTurnUndeadAction : public CastBuffSpellAction
    {
    public:
        CastTurnUndeadAction(PlayerbotAI* ai) : CastBuffSpellAction(ai, "turn undead") {}
        virtual Value<Unit*>* GetTargetValue() { return context->GetValue<Unit*>("cc target", getName()); }
    };

    PROTECT_ACTION(CastBlessingOfProtectionProtectAction, "blessing of protection");
}
