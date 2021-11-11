#pragma once

#include "bak/money.hpp"
#include "bak/inventoryItem.hpp"

namespace BAK {

struct ShopStats
{
    std::uint8_t mTempleNumber;
    std::uint8_t mSellFactor;
    std::uint8_t mMaxDiscount;
    std::uint8_t mBuyFactor;
    std::uint16_t mHaggle;
    std::uint8_t mBardingSkill;
    std::uint8_t mBardingReward;
    std::uint8_t mBardingMaxReward;
    std::array<std::uint8_t, 3> mUnknown;
    std::uint8_t mRepairTypes;
    std::uint8_t mRepairFactor;
    std::uint16_t mCategories;
};


std::ostream& operator<<(std::ostream&, const ShopStats&);

}

namespace BAK::Shop {

Royals GetSellPrice(const InventoryItem&, const ShopStats&);
Royals GetBuyPrice (const InventoryItem&, const ShopStats&);
Royals CalculateRepairCost(const InventoryItem&, const ShopStats&);

double GetItemQuantityMultiple(const InventoryItem&);

}
