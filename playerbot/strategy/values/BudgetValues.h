#pragma once

namespace ai
{
    enum class NeedMoneyFor : uint32
    {
        none = 0,
        repair = 1,
        ammo = 2,
        spells = 3,
        travel = 4,
        consumables = 5,
        gear = 6,
        guild = 7,
        anything = 8
    };

    class MaxGearRepairCostValue : public Uint32CalculatedValue
    {
    public:
        MaxGearRepairCostValue(PlayerbotAI* ai) : Uint32CalculatedValue(ai,"max repair cost",60) {}

        virtual uint32 Calculate();
    };

    class RepairCostValue : public Uint32CalculatedValue
    {
    public:
        RepairCostValue(PlayerbotAI* ai) : Uint32CalculatedValue(ai, "repair cost",60) {}
        
        virtual uint32 Calculate();
    };

    class TrainCostValue : public Uint32CalculatedValue
    {
    public:
        TrainCostValue(PlayerbotAI* ai) : Uint32CalculatedValue(ai, "train cost", 60) {}

        virtual uint32 Calculate();
    };

    class MoneyNeededForValue : public Uint32CalculatedValue, public Qualified
    {
    public:
        MoneyNeededForValue(PlayerbotAI* ai) : Uint32CalculatedValue(ai, "money needed for",60) {}
        virtual uint32 Calculate();
    };

    class TotalMoneyNeededForValue : public Uint32CalculatedValue, public Qualified
    {
    public:
        TotalMoneyNeededForValue(PlayerbotAI* ai) : Uint32CalculatedValue(ai, "total money needed for", 60) {}
        virtual uint32 Calculate();
    private:
        vector<NeedMoneyFor> saveMoneyFor = { NeedMoneyFor::guild,NeedMoneyFor::repair,NeedMoneyFor::ammo, NeedMoneyFor::spells, NeedMoneyFor::travel };
    };

    class FreeMoneyForValue : public Uint32CalculatedValue, public Qualified
    {
    public:
        FreeMoneyForValue(PlayerbotAI* ai) : Uint32CalculatedValue(ai, "free money for") {}
        virtual uint32 Calculate();
    };
    
    class ShouldGetMoneyValue : public BoolCalculatedValue
    {
    public:
        ShouldGetMoneyValue(PlayerbotAI* ai) : BoolCalculatedValue(ai, "should get money",2) {}
        virtual bool Calculate() { return !AI_VALUE2(uint32, "free money for", (uint32)NeedMoneyFor::anything); };
    };
}

