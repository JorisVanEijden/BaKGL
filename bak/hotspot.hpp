#pragma once

#include "bak/constants.hpp"
#include "bak/dialog.hpp"
#include "bak/resourceNames.hpp"
#include "bak/logger.hpp"
#include "bak/string.hpp"

#include "xbak/Exception.h"
#include "xbak/FileBuffer.h"

#include <glm/glm.hpp>

#include <string_view>

namespace BAK {

enum class HotspotAction
{
    DIALOG = 2,
    EXIT = 3,
    GOTO = 4,
    BARMAID = 5,
    SHOP = 6,
    INN = 7,
    CONTAINER = 8,
    LUTE = 9,
    TELEPORT = 11,
    TEMPLE = 13,
    NOT_SURE = 15,
    REPAIR = 16,
};

std::ostream& operator<<(std::ostream&, HotspotAction);

struct Hotspot
{
    std::uint16_t mHotspot;
    glm::vec<2, int> mTopLeft;
    glm::vec<2, int> mDimensions;
    std::uint16_t mKeyword;
    HotspotAction mAction;
    std::uint32_t mActionArg1;
    std::uint32_t mActionArg2;
    KeyTarget mTooltip;
    KeyTarget mDialog;
};

std::ostream& operator<<(std::ostream&, const Hotspot&);

// Loaded from a GDS File
class SceneHotspots
{
public:
    std::string mSceneTTM;
    std::string mSceneADS;

    std::uint16_t mSceneIndex1;
    std::uint16_t mSceneIndex2;

    std::vector<Hotspot> mHotspots;

    void Load(FileBuffer& fb)
    {
        BAK::DialogStore dialogStore{};
        dialogStore.Load();

        const auto GetText = [&](auto& tgt) -> std::string_view
        {
            try
            {
                return dialogStore.GetSnippet(KeyTarget{tgt}).GetText();
            }
            catch (const std::runtime_error& e)
            {
                return e.what();
            }
        };

        auto length = fb.GetUint16LE();
        std::cout << "Length: " << length << std::endl;
        const auto resource = fb.GetString(6);
        mSceneTTM = ToUpper(resource + ".TTM");
        mSceneADS = ToUpper(resource + ".ADS");
        std::cout << "Scene Ref: " << mSceneTTM << std::endl;

        fb.DumpAndSkip(4);
        fb.DumpAndSkip(2);
        fb.DumpAndSkip(4);
        fb.DumpAndSkip(5); // Some kind of addr?
        // For all towns, scene index1.
        mSceneIndex1 = fb.GetUint16LE();
        std::cout << "Scene index1: " << mSceneIndex1 << "\n";
        fb.DumpAndSkip(2); // Not sure
        mSceneIndex2 = fb.GetUint16LE();
        std::cout << "Scene index2: " << mSceneIndex2 << "\n";
        auto numHotSpots = fb.GetUint16LE(); 
        std::uint32_t flavourText = fb.GetUint32LE(); 
        std::cout << "Hotspots: " << std::dec << numHotSpots << std::endl;
        std::cout << "Flavour Text: " << std::hex << flavourText << std::endl;
        std::cout << GetText(flavourText) << std::endl;

        fb.DumpAndSkip(4);
        fb.DumpAndSkip(4);

        std::vector<Hotspot> hotspots;

        for (unsigned i = 0; i < numHotSpots; i++)
        {
            auto x = fb.GetUint16LE();
            auto y = fb.GetUint16LE();
            auto w = fb.GetUint16LE();
            auto h = fb.GetUint16LE();
            std::cout << "Hotspot #" << std::dec << i << std::endl;
            //std::cout << "coords: " << std::dec << x << " " << y
            //    << " " << w << " " << h << std::endl;
            fb.DumpAndSkip(2); // Seems to have some effect...
            auto keyword = fb.GetUint16LE();
            //std::cout << "Kw: " << keyword << std::endl;
            auto action = static_cast<HotspotAction>(fb.GetUint16LE());
            std::cout << "Action: " << action << std::endl;
            auto actionArg1 = fb.GetUint32LE();
            auto actionArg2 = fb.GetUint32LE();
            std::cout << "A1: " << actionArg1 << std::hex << " A2: " << actionArg2 << std::dec << "\n";
            std::uint32_t tooltip = fb.GetUint32LE(); 
            std::cout << "RightClick: " << std::hex << tooltip << GetText(tooltip) << std::endl;
            fb.DumpAndSkip(4); // Seems to have some effect...
            std::uint32_t dialog = fb.GetUint32LE(); 
            //std::cout << "LeftClick: " << std::hex << dialog << std::endl;
            // std::cout << GetText(dialog) << std::endl;
            fb.DumpAndSkip(2);

            hotspots.emplace_back(
                i,
                glm::vec<2, int>{x, y},
                glm::vec<2, int>{w, h},
                keyword,
                static_cast<HotspotAction>(action),
                actionArg1,
                actionArg2,
                KeyTarget{tooltip},
                KeyTarget{dialog});
        }

        for (auto& hs : hotspots)
        {
            std::cout << hs << "\n";
        }

        mHotspots = hotspots;
    }
};

}