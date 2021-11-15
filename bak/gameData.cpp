#include "bak/gameData.hpp"

namespace BAK {

GameData::GameData(const std::string& save)
:
    mBuffer{FileBufferFactory::CreateFileBuffer(save)},
    mLogger{Logging::LogState::GetLogger("GameData")},
    mName{LoadSaveName()},
    mObjects{},
    mChapter{LoadChapter()},
    mLocation{LoadLocation()},
    mTime{LoadWorldTime()},
    mParty{LoadParty()}
{

    mLogger.Info() << "Loading save: " << mBuffer.GetString() << std::endl;
    //mLogger.Info() << mParty << "\n";
    mLogger.Info() << mTime << "\n";
    LoadContainers(0x1);
    LoadContainers(0x2);
    LoadContainers(0x3);
    LoadContainers(0x4);
    LoadContainers(0x5);
    LoadContainers(0x6);
    LoadContainers(0x7);
    LoadContainers(0x8);
    LoadContainers(0x9);
    LoadContainers(0xa);
    LoadContainers(0xb);
    LoadContainers(0xc);
    mLogger.Debug() << "Loaded Z12 Cont: " << std::hex 
        << mBuffer.Tell() << std::dec << "\n";
    LoadShops();
    //LoadCombatEntityLists();
    //LoadCombatInventories(
    //    sCombatInventoryOffset,
    //    sCombatInventoryCount);
    //LoadCombatStats(0x914b, 1698);
}


std::pair<unsigned, unsigned> GameData::CalculateComplexEventOffset(unsigned eventPtr) const
{
    const auto source = (eventPtr + 0x2540) & 0xffff;
    const auto byteOffset = source / 10;
    const auto bitOffset = source % 10 != 0
        ? (source % 10) - 1
        : 0;
        
    mLogger.Spam() << __FUNCTION__ << std::hex << " " << eventPtr << " ("
        << byteOffset + sGameComplexEventRecordOffset << ", " 
        << bitOffset << ")\n" << std::dec;
    return std::make_pair(
        byteOffset + sGameComplexEventRecordOffset,
        bitOffset);
}

std::pair<unsigned, unsigned> GameData::CalculateEventOffset(unsigned eventPtr) const
{
    const unsigned startOffset = sGameEventRecordOffset;
    const unsigned bitOffset = eventPtr & 0xf;
    const unsigned byteOffset = (0xfffe & (eventPtr >> 3)) + startOffset;
    mLogger.Spam() << __FUNCTION__ << std::hex << " " << eventPtr << " ("
        << byteOffset << ", " << bitOffset << ")\n" << std::dec;
    return std::make_pair(byteOffset, bitOffset);
}

void GameData::SetBitValueAt(unsigned byteOffset, unsigned bitOffset, unsigned value)
{
    mBuffer.Seek(byteOffset);
    const auto originalData = mBuffer.GetUint16LE();
    auto data = originalData;
    if (value)
        data = data | (1 << bitOffset);
    else
        data = data & (~(1 << bitOffset));

    mBuffer.Seek(byteOffset);
    mBuffer.PutUint16LE(data);

    mLogger.Debug() << __FUNCTION__ << std::hex << 
        " " << byteOffset << " " << bitOffset 
        << " original[" << +originalData << "] new[" << +data  <<"]\n";
}

void GameData::SetEventFlag(unsigned eventPtr, unsigned value)
{
    mLogger.Debug() << __FUNCTION__ << " " << std::hex << eventPtr 
        << " to: " << value << std::dec << "\n";
    if (eventPtr >= 0xdac0)
    {
        const auto [byteOffset, bitOffset] = CalculateComplexEventOffset(eventPtr);
        SetBitValueAt(byteOffset, bitOffset, value);
    }
    else
    {
        const auto [byteOffset, bitOffset] = CalculateEventOffset(eventPtr);
        SetBitValueAt(byteOffset, bitOffset, value);
    }
}

void GameData::SetEventFlagTrue (unsigned eventPtr)
{
    SetEventFlag(eventPtr, 1);
}

void GameData::SetEventFlagFalse(unsigned eventPtr)
{
    SetEventFlag(eventPtr, 0);
}

void GameData::SetEventDialogAction(const SetFlag& setFlag)
{
    if (setFlag.mEventPointer >= 0xdac0
        && setFlag.mEventPointer % 10 == 0)
    {
        const auto offset = std::get<0>(CalculateComplexEventOffset(setFlag.mEventPointer));
        mBuffer.Seek(offset);
        const auto data = mBuffer.GetUint8();
        const auto newData = ((data & setFlag.mEventMask) 
            | setFlag.mEventData)
            ^ setFlag.mAlwaysZero;
        mBuffer.Seek(offset);

        mLogger.Debug() << __FUNCTION__ << std::hex << 
            " " << setFlag << " offset: " << offset 
            << " data[" << +data << "] new[" << +newData <<"]\n";
        mBuffer.PutUint8(newData);
    }
    else
    {
        if (setFlag.mEventPointer != 0)
            SetEventFlag(setFlag.mEventPointer, setFlag.mEventValue);

        // TREAT THIS AS UINT16_T !!! EVENT MASK + EVENT DATA
        if (setFlag.mEventMask != 0)
            SetEventFlag(setFlag.mEventMask, setFlag.mEventValue);

        if (setFlag.mAlwaysZero != 0)
            SetEventFlag(setFlag.mAlwaysZero, setFlag.mEventValue);
    }
}

unsigned GameData::ReadBitValueAt(unsigned byteOffset, unsigned bitOffset) const
{
    mBuffer.Seek(byteOffset);
    const unsigned eventData = mBuffer.GetUint16LE();
    const unsigned bitValue = eventData >> bitOffset;
    mLogger.Spam() << __FUNCTION__ << std::hex << 
        " " << byteOffset << " " << bitOffset 
        << " [" << +bitValue << "]\n";
    return bitValue;
}

unsigned GameData::ReadEvent(unsigned eventPtr) const
{
    if (eventPtr >= 0xdac0)
    {
        const auto [byteOffset, bitOffset] = CalculateComplexEventOffset(eventPtr);
        return ReadBitValueAt(byteOffset, bitOffset);
    }
    else
    {
        const auto [byteOffset, bitOffset] = CalculateEventOffset(eventPtr);
        return ReadBitValueAt(byteOffset, bitOffset);
    }
}

bool GameData::ReadEventBool(unsigned eventPtr) const
{
    return (ReadEvent(eventPtr) & 0x1) == 1;
}

bool GameData::ReadSkillSelected(unsigned character, unsigned skill) const
{
    constexpr auto maxSkills = 0x11;
    return ReadEventBool(
        sSkillSelectedEventFlag
        + (character * maxSkills)
        + skill);
}

bool GameData::ReadSkillUnseenImprovement(unsigned character, unsigned skill) const
{
    constexpr auto maxSkills = 0x11;
    return ReadEventBool(
        sSkillImprovementEventFlag
        + (character * maxSkills)
        + skill);
}

void GameData::ClearUnseenImprovements(unsigned character)
{
    constexpr auto maxSkills = 0x11;
    for (unsigned i = 0; i < maxSkills; i++)
    {
        const auto flag = 
            sSkillImprovementEventFlag
            + (character * maxSkills)
            + i;

        SetEventFlagFalse(flag);
    }
}

// Called by checkBlockTriggered, checkTownTriggered, checkBackgroundTriggered, checkZoneTriggered,
// doEnableEncounter, doDialogEncounter, doDisableEncounter, doSoundEncounter
bool GameData::CheckActive(
    const Encounter::Encounter& encounter,
    ZoneNumber zone) const
{
    const auto encounterIndex = encounter.GetIndex().mValue;
    const bool alreadyEncountered = ReadEventBool(
        CalculateUniqueEncounterStateFlagOffset(
            zone,
            encounter.GetTileIndex(),
            encounterIndex));
    const bool encounterFlag1450 = ReadEventBool(
        CalculateRecentEncounterStateFlag(encounterIndex));
    // event flag 1 - this flag must be set to encounter the event
    const bool eventFlag1 = encounter.mSaveAddress != 0
        ? (ReadEventBool(encounter.mSaveAddress) == 1)
        : false;
    // event flag 2 - this flag must _not_ be set to encounter this event
    const bool eventFlag2 = encounter.mSaveAddress2 != 0
        ? ReadEventBool(encounter.mSaveAddress2)
        : false;
    return !(alreadyEncountered
        || encounterFlag1450
        || eventFlag1
        || eventFlag2);
}

void GameData::SetPostDialogEventFlags(const Encounter::Encounter& encounter)
{
    constexpr auto zone = ZoneNumber{1};
    const auto tileIndex = encounter.GetTileIndex();
    const auto encounterIndex = encounter.GetIndex().mValue;

    if (encounter.mSaveAddress3 != 0)
    {
        SetEventFlagTrue(encounter.mSaveAddress3);
    }

    // Unknown 3 flag is associated with events like the sleeping glade and 
    // timirianya danger zone (effectively, always encounter this encounter)
    if (encounter.mUnknown3 == 0)
    {
        // FIXME use actual values
        if (encounter.mUnknown2 != 0) // Inhibit for this chapter
        {
            SetEventFlagTrue(CalculateUniqueEncounterStateFlagOffset(
                zone, tileIndex, encounterIndex));
        }

        // Inhibit for this tile
        SetEventFlagTrue(CalculateRecentEncounterStateFlag(encounterIndex));
    }

}

// Background and Town
void GameData::SetPostGDSEventFlags(const Encounter::Encounter& encounter)
{
    if (encounter.mSaveAddress3 != 0)
        SetEventFlagTrue(encounter.mSaveAddress3);
}

// Used by Block, Disable, Enable, Sound, Zone
void GameData::SetPostEnableOrDisableEventFlags(
    const Encounter::Encounter& encounter,
    ZoneNumber zone)
{
    if (encounter.mSaveAddress3 != 0)
    {
        SetEventFlagTrue(encounter.mSaveAddress3);
    }
    // FIXME use actual values
    if (encounter.mUnknown2 != 0)
    {
        SetEventFlagTrue(
            CalculateUniqueEncounterStateFlagOffset(
                zone,
                encounter.GetTileIndex(),
                encounter.GetIndex().mValue));
    }
}


// For each encounter in every zone there is a unique enabled/disabled flag.
// This is reset every time a new chapter is loaded (I think);
unsigned GameData::CalculateUniqueEncounterStateFlagOffset(
    ZoneNumber zone, 
    std::uint8_t tileIndex,
    std::uint8_t encounterIndex) const
{
    constexpr auto encounterStateOffset = 0x190;
    constexpr auto maxEncountersPerTile = 0xa;
    const auto zoneOffset = (zone.mValue - 1) * encounterStateOffset;
    const auto tileOffset = tileIndex * maxEncountersPerTile;
    const auto offset = zoneOffset + tileOffset + encounterIndex;
    return offset + encounterStateOffset;
}

// 1450 is "recently encountered this encounter"
// should be cleared when we move to a new tile
// (or it will inhibit the events of the new tile)
unsigned GameData::CalculateRecentEncounterStateFlag(
    std::uint8_t encounterIndex) const
{
    // Refer readEncounterEventState1450 in IDA
    // These get cleared when we load a new tile
    constexpr auto offset = 0x1450;
    return offset + encounterIndex;
}

bool GameData::ReadConversationItemClicked(unsigned eventPtr) const
{
    return ReadEventBool(sConversationChoiceMarkedFlag + eventPtr);
}

void GameData::SetConversationItemClicked(unsigned eventPtr)
{
    return SetEventFlagTrue(sConversationChoiceMarkedFlag + eventPtr);
}

bool GameData::CheckConversationOptionInhibited(unsigned eventPtr)
{
    return ReadEventBool(sConversationOptionInhibitedFlag + eventPtr);
}

void GameData::SetLockHasBeenSeen(unsigned lockIndex)
{
    SetEventFlagTrue(sLockHasBeenSeenFlag + lockIndex);
}

bool GameData::CheckLockHasBeenSeen(unsigned lockIndex)
{
    return ReadEventBool(sLockHasBeenSeenFlag + lockIndex);
}

void GameData::ClearTileRecentEncounters()
{
    for (unsigned i = 0; i < 10; i++)
    {
        SetEventFlagFalse(CalculateRecentEncounterStateFlag(i));
    }
}

/* ************* LOAD Game STATE ***************** */
Party GameData::LoadParty()
{
    auto characters = LoadCharacters();
    auto active = LoadActiveCharacters();
    auto gold = LoadGold();
    auto keys = LoadInventory(sPartyKeyInventoryOffset);
    return Party{
        gold,
        std::move(keys),
        characters,
        active};
}

    
std::vector<Character> GameData::LoadCharacters()
{
    unsigned characters = sCharacterCount;
    const auto nameOffset = sCharacterNameOffset;
    const auto nameLength = 10;
    const auto skillLength = 5 * 16 + 8 + 7;
    const auto skillOffset = sCharacterSkillOffset;
    const auto inventoryOffset = sCharacterInventoryOffset;
    const auto inventoryLength = 0x70;

    std::vector<Character> chars;

    for (unsigned character = 0; character < characters; character++)
    {
        mBuffer.Seek(nameOffset + nameLength * character);
        auto name = mBuffer.GetString(nameLength);

        mBuffer.Seek(skillOffset + skillLength * character);
        mLogger.Debug() << "Name: " << name << "@" 
            << std::hex << mBuffer.Tell() << std::dec << "\n";

        auto unknown = mBuffer.GetArray<2>();
        auto spells = mBuffer.GetArray<6>();

        auto skills = Skills{};

        for (unsigned i = 0; i < Skills::sSkills; i++)
        {
            const auto max        = mBuffer.GetUint8();
            const auto current    = mBuffer.GetUint8();
            const auto limit      = mBuffer.GetUint8();
            const auto experience = mBuffer.GetUint8();
            const auto modifier   = mBuffer.GetSint8();

            const auto pos = mBuffer.Tell();

            const auto selected = ReadSkillSelected(character, i);
            const auto unseenIprovement = ReadSkillUnseenImprovement(character, i);

            skills.mSkills[i] = Skill{
                max,
                current,
                limit,
                experience,
                modifier,
                selected,
                unseenIprovement
            };

            mBuffer.Seek(pos);
        }

        auto unknown2 = mBuffer.GetArray<7>();
        mLogger.Info() << " Finished loading : " << name << std::hex << mBuffer.Tell() << std::dec << "\n";
        // Load inventory
        auto inventory = LoadInventory(
            inventoryOffset + inventoryLength * character);

        auto conditions = LoadConditions(character);

        chars.emplace_back(
            character,
            name,
            skills,
            spells,
            unknown,
            unknown2,
            conditions,
            std::move(inventory));
    }
    
    return chars;
}

Conditions GameData::LoadConditions(unsigned character)
{
    ASSERT(character < sCharacterCount);
    mBuffer.Seek(sCharacterStatusOffset 
        + character * Conditions::sNumConditions);

    auto conditions = Conditions{};
    for (unsigned i = 0; i < Conditions::sNumConditions; i++)
        conditions.mConditions[i] = mBuffer.GetUint8();
    return conditions;
}

unsigned GameData::LoadChapter()
{
    mBuffer.Seek(sChapterOffset);
    return mBuffer.GetUint16LE();
}

Royals GameData::LoadGold()
{
    mBuffer.Seek(sGoldOffset);
    return Royals{mBuffer.GetUint32LE()};
}

std::vector<CharIndex> GameData::LoadActiveCharacters()
{
    mBuffer.Seek(sActiveCharactersOffset);
    const auto activeCharacters = mBuffer.GetUint8();

    auto active = std::vector<CharIndex>{};
    for (unsigned i = 0; i < activeCharacters; i++)
    {
        const auto c = mBuffer.GetUint8();
        active.emplace_back(c);
    }

    return active;
}

Location GameData::LoadLocation()
{
    mBuffer.Seek(sLocationOffset);

    unsigned zone = mBuffer.GetUint8();
    ASSERT(zone < 12);
    mLogger.Info() << "Zone:" << zone << std::endl;

    unsigned xtile = mBuffer.GetUint8();
    unsigned ytile = mBuffer.GetUint8();
    unsigned xpos = mBuffer.GetUint32LE();
    unsigned ypos = mBuffer.GetUint32LE();

    mBuffer.DumpAndSkip(5);
    std::uint16_t heading = mBuffer.GetUint16LE();

    mLogger.Info() << "Tile: " << xtile << "," << ytile << std::endl;
    mLogger.Info() << "Pos: " << xpos << "," << ypos << std::endl;
    mLogger.Info() << "Heading: " << heading << std::endl;
    
    return Location{
        zone,
        {xtile, ytile},
        GamePositionAndHeading{
            GamePosition{xpos, ypos},
            heading}
    };
}

WorldClock GameData::LoadWorldTime()
{
    mBuffer.Seek(sTimeOffset);
    return WorldClock{
        Time{mBuffer.GetUint32LE()},
        Time{mBuffer.GetUint32LE()}};
}


Inventory GameData::LoadInventory(unsigned offset)
{
    mBuffer.Seek(offset);

    const auto itemCount = mBuffer.GetUint8();
    const auto capacity = mBuffer.GetUint16LE();
    mLogger.Info() << " Items: " << +itemCount << " cap: " << capacity << "\n";
    return Inventory{capacity, LoadItems(itemCount, capacity)};
}

std::vector<InventoryItem> GameData::LoadItems(unsigned itemCount, unsigned capacity)
{
    std::vector<InventoryItem> items{};
    unsigned i;
    for (i = 0; i < itemCount; i++)
    {
        const auto item = ItemIndex{mBuffer.GetUint8()};
        const auto& object = mObjects.GetObject(item);
        const auto condition = mBuffer.GetUint8();
        const auto status = mBuffer.GetUint8();
        const auto modifiers = mBuffer.GetUint8();

        items.emplace_back(
            InventoryItemFactory::MakeItem(
                item,
                condition,
                status,
                modifiers));
    }

    for (; i < capacity; i++)
        mBuffer.Skip(4);

    return items;
}

LockStats GameData::LoadLock()
{
    const auto lockFlag = mBuffer.GetUint8();
    const auto picklock = mBuffer.GetUint8();
    const auto fairyChestIndex = mBuffer.GetUint8();
    const auto damage = mBuffer.GetUint8();
    return LockStats{lockFlag, picklock, fairyChestIndex, damage};
}

ShopStats GameData::LoadShop()
{
    const auto templeNumber = mBuffer.GetUint8();
    const auto sellFactor = mBuffer.GetUint8();
    const auto maxDiscount = mBuffer.GetUint8();
    const auto buyFactor = mBuffer.GetUint8();
    const auto haggle = mBuffer.GetUint16LE();
    const auto bardingSkill = mBuffer.GetUint8();
    const auto bardingReward = mBuffer.GetUint8();
    const auto bardingMaxReward = mBuffer.GetUint8();
    const auto unknown = mBuffer.GetArray<3>();
    const auto repairTypes = mBuffer.GetUint8();
    const auto repairFactor = mBuffer.GetUint8();
    const auto categories = mBuffer.GetUint16LE();

    return ShopStats{
        templeNumber,
        sellFactor,
        maxDiscount,
        buyFactor,
        haggle,
        bardingSkill,
        bardingReward,
        bardingMaxReward,
        unknown,
        repairTypes,
        repairFactor,
        categories
    };
}

std::vector<GDSContainer> GameData::LoadShops()
{
    mBuffer.Seek(sShopsOffset);
    auto shops = std::vector<GDSContainer>{};

    for (unsigned i = 0; i < sShopsCount; i++)
    {
        const auto header = ContainerHeader{
            ContainerGDSLocationTag{},
            mBuffer};
        mLogger.Debug() << std::hex << mBuffer.Tell() << std::dec << "\n";
        mLogger.Debug() << "Shop #: " << i << " " << header << "\n";
        auto inventory = Inventory{
            header.mCapacity,
            LoadItems(header.mItems, header.mCapacity)};
        auto shopData = std::optional<ShopStats>{};
        if (static_cast<ContainerType>(header.mContainerType) == ContainerType::Shop)
            shopData = LoadShop();

        shops.emplace_back(
            header.GetHotspotRef(),
            header.mLocationType,
            header.mItems,
            header.mCapacity,
            static_cast<ContainerType>(header.mContainerType),
            shopData,
            LockStats{},
            std::move(inventory));
        mLogger.Debug() << shops.back() << "\n";
    }

    return shops;
}

std::vector<Container> GameData::LoadContainers(unsigned zone)
{
    const auto& logger = Logging::LogState::GetLogger("GameData");
    logger.Info() << "Loading containers for Z: " << zone << "\n";
    std::vector<Container> containers{};

    unsigned count = 0;
    // TombStone
    switch (zone)
    {
    case 1:
        mBuffer.Seek(0x3b621); // 36 items 1
        count = 36;
        break;
    case 2:
        mBuffer.Seek(0x3be55); // 25 2
        count = 25;
        break;
    case 3:
        mBuffer.Seek(0x3c55f); // 54 3
        count = 54;
        break;
    case 4:
        mBuffer.Seek(0x3d0b4); // 65 4
        count = 65;
        break;
    case 5:
        mBuffer.Seek(0x3dc07); // 63 5
        count = 63;
        break;
    case 6:
        mBuffer.Seek(0x3e708); // 131 6
        count = 131;
        break;
    case 7:
        mBuffer.Seek(0x3f8b2); // 115 7
        count = 115;
        break;
    case 8:
        mBuffer.Seek(0x40c97); // 67 8
        count = 67;
        break;
    case 9:
        mBuffer.Seek(0x416b7); // 110 9
        count = 110;
        break;
    case 10:
        mBuffer.Seek(0x42868); // 25 A
        count = 25;
        break;
    case 11:
        mBuffer.Seek(0x43012); // 30 B
        count = 30;
        break;
    case 12:
        mBuffer.Seek(0x4378f); // 60 C
        count = 60;
        break;
    default:
        throw std::runtime_error("Zone not supported");
    }

    mBuffer.Dump(8);
    for (unsigned j = 0; j < count; j++)
    {
        unsigned address = mBuffer.Tell();
        logger.Info() << " Container: " << j
            << " addr: " << std::hex << address << std::dec << std::endl;

        auto header = ContainerHeader(ContainerWorldLocationTag{}, mBuffer);
        ASSERT(header.mLocationType == 6 
            || header.mLocationType == 9
            || header.mCapacity > 0);
        const auto containerType = static_cast<ContainerType>(header.mContainerType);

        auto shopData = std::optional<ShopStats>{};
        auto lockData = LockStats{0,0,0,0};

        logger.Info() << header
            << " Tp: " << ToString(containerType) << std::endl;
        
        auto inventory = Inventory{
            header.mCapacity,
            LoadItems(header.mItems, header.mCapacity)};

        auto picklockSkill  = 0;
        auto dialog = KeyTarget{0};

        if (containerType == ContainerType::Shop
            || containerType == ContainerType::Inn)
        {
            mBuffer.DumpAndSkip(2);
            dialog = KeyTarget{mBuffer.GetUint32LE()};
            shopData = LoadShop();
        }
        else if (containerType == ContainerType::Gravestone)
        {
            mBuffer.DumpAndSkip(2);
            dialog = KeyTarget{mBuffer.GetUint32LE()};
        }
        else if (containerType == ContainerType::Bag)
        {
        }
        else if (containerType == ContainerType::CT1)
        {
            lockData = LoadLock();
        }
        else if (containerType == ContainerType::Building)
        {
            lockData = LoadLock();
            mBuffer.DumpAndSkip(2);
            const auto postDialog = KeyTarget{mBuffer.GetUint32LE()};
            mLogger.Debug() << "PostLockDialog:  " << Target{postDialog} << "\n";
            dialog = postDialog;
        }
        else if (containerType == ContainerType::Chest)
        {
            lockData = LoadLock();
        }
        // Locked chest.....
        else if (containerType == ContainerType::FairyChest)
        {
            lockData = LoadLock();
            dialog = KeyTarget{mBuffer.GetUint32LE()};
        }
        else if (containerType == ContainerType::EventChest)
        {
            lockData = LoadLock();
            // ?? 2 bytes
            // set this event flag 2 bytes
            // 9 bytes
            mBuffer.DumpAndSkip(13);
        }
        else if (containerType == ContainerType::TimirianyaHut)
        {
            mBuffer.DumpAndSkip(9);
        }
        else if (containerType == ContainerType::Combat)
        {
            mBuffer.DumpAndSkip(2);
            dialog = KeyTarget{mBuffer.GetUint32LE()};
            mBuffer.DumpAndSkip(9);
        }
        else
        {
            ASSERT(false);
        }

        containers.emplace_back(
            address,
            header.mLocationType,
            header.mItems,
            header.mCapacity,
            static_cast<ContainerType>(header.mContainerType),
            dialog,
            header.GetPosition(),
            shopData,
            lockData,
            std::move(inventory));
        logger.Info() << containers.back() << std::endl;
    }

    return containers;
}

void GameData::LoadChapterOffsetP()
{
    // I have no idea what these mean
    constexpr unsigned chapterOffsetsStart = 0x11a3;
    mBuffer.Seek(chapterOffsetsStart);

    mLogger.Info() << "Chapter Offsets Start @" 
        << std::hex << chapterOffsetsStart << std::dec << std::endl;

    for (unsigned i = 0; i < 10; i++)
    {
        std::stringstream ss{};
        ss << "Chapter #" << i << " : " << mBuffer.GetUint16LE();
        for (unsigned i = 0; i < 5; i++)
        {
            unsigned addr = mBuffer.GetUint32LE();
            ss << " a: " << std::hex << addr;
        }
        mLogger.Info() << ss.str() << std::endl;
    }

    mLogger.Info() << "Chapter Offsets End @" 
        << std::hex << mBuffer.Tell() << std::dec << std::endl;
}

void GameData::LoadCombatEntityLists()
{
    mBuffer.Seek(sCombatEntityListOffset);

    mLogger.Info() << "Combat Entity Lists Start @" 
        << std::hex << sCombatEntityListOffset << std::dec << std::endl;

    for (int i = 0; i < sCombatEntityListCount; i++)
    {
        std::stringstream ss{};
        ss << " Combat #" << i;
        constexpr unsigned maxCombatants = 7;
        auto sep = ' ';
        for (unsigned i = 0; i < maxCombatants; i++)
        {
            auto combatant = mBuffer.GetUint16LE();
            if (combatant != 0xffff)
                ss << sep << combatant;
            sep = ',';
        }
        mLogger.Info() << ss.str() << std::endl;
    }

    mLogger.Info() << "Combat Entity Lists End @" 
        << std::hex << mBuffer.Tell() << std::dec << std::endl;
}

void GameData::LoadCombatStats(unsigned offset, unsigned num)
{
    unsigned combatStatsStart = offset;
    mBuffer.Seek(combatStatsStart);
    mLogger.Info() << "Combat Stats Start @" 
        << std::hex << combatStatsStart << std::dec << std::endl;

    // ends at 3070a
    for (unsigned i = 0; i < num; i++)
    {
        mLogger.Info() << "Combat #" << std::dec << i 
            << " " << std::hex << mBuffer.Tell() << std::endl;
        mLogger.Info() << std::hex << mBuffer.GetUint16LE() << std::endl << std::dec;
        mBuffer.Dump(6);
        mBuffer.Skip(6);

        std::stringstream ss{""};
        for (const auto& stat : {
            "Health", "Stamina", "Speed", "Strength", 
            "Defense", "Crossbow", "Melee", "Cast",
            "Assess", "Armor", "Weapon", "Bard",
            "Haggle", "Lockpick", "Scout", "Stealth"})
        {
            ss << std::dec << stat << ": " << +mBuffer.GetUint8() << " " 
                << +mBuffer.GetUint8() << " " << +mBuffer.GetUint8() << " ";
            mBuffer.Skip(2);
        }
        mBuffer.Dump(7);
        mBuffer.Skip(7);
        mLogger.Info() << ss.str() << std::endl;
    }
    mLogger.Info() << "Combat Stats End @" 
        << std::hex << mBuffer.Tell() << std::dec << std::endl;
}

void GameData::LoadCombatInventories(unsigned offset, unsigned number)
{
    mLogger.Info() << "Loading Combat Inventories" << std::endl;
    auto combatInventoryLocation = offset;
    auto numberCombatInventories = number;
    mBuffer.Seek(combatInventoryLocation);
    for (unsigned i = 0; i < numberCombatInventories; i++)
    {
        mBuffer.Dump(13);
        auto x = mBuffer.Tell();
        //ASSERT(mBuffer.GetUint8() == 0x64);
        mBuffer.GetUint8(); // always 0x64 for combats
        mBuffer.GetUint8();// == 0x0a);
        mBuffer.Skip(2);
        auto combatantNo = mBuffer.GetUint16LE();
        mBuffer.Skip(2);
        auto combatNo = mBuffer.GetUint16LE();
        mBuffer.Skip(2);

        int unknown = mBuffer.GetUint8();
        mLogger.Info() << "CombatInventory #" << i << " "
            << std::hex << x << std::dec << " CBT: " << +combatNo 
            << " PER: " << +combatantNo << " Unk: " << unknown << std::endl;
        const auto inventory = LoadInventory(mBuffer.Tell());
        mLogger.Info() << inventory << "\n";
    }
}

std::string GameData::LoadSaveName()
{
    mBuffer.Seek(0);
    return mBuffer.GetString(30);
}

}