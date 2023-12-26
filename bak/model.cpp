#include "bak/dataTags.hpp"
#include "bak/model.hpp"

#include "com/ostream.hpp"
#include "graphics/glm.hpp"

namespace BAK {

inline std::ostream& operator<<(std::ostream& os, const FaceOption& m)
{
    os << "      FaceOption[FT: " << m.mFaceType << " EC: " << m.mEdgeCount << " FCs: " << m.mFaceColors.size() << " PALs: " 
        << m.mPalettes.size() << " Faces " << m.mFaces.size() << " -- ";
    for (const auto& f: m.mFaces)
    {
        os << " " << f.size() << ",";
    }
    os << "]";

    return os;
}
inline std::ostream& operator<<(std::ostream& os, const Mesh& m)
{
    os << "      FaceOptions: " << m.mFaceOptions.size() << "\n";
    for (const auto& f: m.mFaceOptions)
    {
        os << " " << f << "\n";
    }

    return os;
}
inline std::ostream& operator<<(std::ostream& os, const Component& m)
{
    os << "  Meshes: " << m.mMeshes.size() << "\n";
    for (unsigned i = 0; i < m.mMeshes.size(); i++)
    {
        os << "    Mesh #" << i << "\n";
        os << m.mMeshes[i] << "";
    }

    return os;
}
inline std::ostream& operator<<(std::ostream& os, const Model& m)
{
    os << m.mName << " EF: " << m.mEntityFlags << " ET: " << m.mEntityType 
        << " TT: " << m.mTerrainType << " Scale: " << m.mScale << " Sprite: " << m.mSprite
        << "\n";
    os << " NumVertices: " << m.mVertices.size() << "\n";
    os << " Components: " << m.mComponents.size() << "\n";
    for (unsigned i = 0; i < m.mComponents.size(); i++)
    {
        os << "  Component: " << i << "\n" << m.mComponents[i] << "\n";
    }

    return os;
}


std::vector<std::string> LoadModelNames(FileBuffer& fb)
{
    const auto& logger = Logging::LogState::GetLogger(__FUNCTION__);
    
    fb.Skip(2);
    const auto numItems = fb.GetUint16LE();

    std::vector<unsigned> offsets{};
    std::vector<std::string> mapItems{};
    offsets.reserve(numItems);
    mapItems.reserve(numItems);

    for (unsigned i = 0; i < numItems; i++)
    {
        offsets.emplace_back(fb.GetUint16LE());
    }

    fb.Skip(2);
    const auto dataStart = fb.GetBytesDone();

    for (auto offset : offsets)
    {
        fb.Seek(dataStart + offset);
        mapItems.emplace_back(fb.GetString());
    }

    return mapItems;
}

unsigned CalculateOffset(unsigned upper, unsigned lower)
{
    return (upper << 4) + (lower & 0xf);
}

std::vector<ModelClip> LoadModelClip(FileBuffer& fb, unsigned numItems)
{
    const auto& logger = Logging::LogState::GetLogger(__FUNCTION__);

    std::vector<unsigned> offsets{};
    offsets.reserve(numItems);
    std::vector<ModelClip> modelClips{};
    modelClips.reserve(numItems);

    for (unsigned i = 0; i < numItems; i++)
    {
        const auto lower = fb.GetUint16LE();
        const auto upper = fb.GetUint16LE();
        
        offsets.emplace_back(CalculateOffset(upper, lower));
    }

    for (unsigned int i = 0; i < numItems; i++)
    {
        fb.Seek(offsets[i]); 
        std::vector<glm::ivec2> textureCoords{};
        std::vector<glm::ivec2> otherCoords{};

        const auto xradius = fb.GetUint16LE();
        const auto yradius = fb.GetUint16LE();
        logger.Debug() << "HitRad: (" << xradius << "," << yradius << ")\n";

        const bool more = fb.GetUint16LE() > 0;
        const auto flags = fb.GetUint16LE();

        if (more)
        {
            // This is clearly wrong...
            fb.Skip(2);
            const auto n = fb.GetUint16LE();
            fb.Skip(2);
            for (unsigned component = 0; component < n; component++)
            {
                const auto u = fb.GetSint8();
                const auto v = fb.GetSint8();
                const auto x = fb.GetSint16LE();
                const auto y = fb.GetSint16LE();
                logger.Debug() << " uvxy " << u << " " << v << " " << x << " " << y << std::endl; 
                textureCoords.emplace_back(u, v);
                otherCoords.emplace_back(x, y);
            }
        }
        modelClips.emplace_back(
            ModelClip{
                xradius, yradius,
                flags, 0,
                0,
                std::move(textureCoords),
                std::move(otherCoords)}
        );
    }

    return modelClips;
}

struct FaceData
{
    unsigned mFaceType;
    unsigned mEdgeCount;
    unsigned mEdgeOffset;
};

struct MeshOffsetData
{
    unsigned mVertexCount;
    unsigned mVertexOffset;
    unsigned mVertexIndexTransform;
    unsigned mFaceCount;
    unsigned mFaceOffset;
};

struct ComponentData 
{
    unsigned mMeshCount;
    unsigned mMeshOffset;
    std::vector<MeshOffsetData> mMeshOffsetDatas;
};

std::vector<Model> LoadModels(FileBuffer& fb, std::vector<std::string> itemNames)
{
    const auto& logger = Logging::LogState::GetLogger(__FUNCTION__);

    const unsigned numItems = itemNames.size();
    std::vector<unsigned> offsets{};
    offsets.reserve(numItems);

    std::vector<Model> newModels{};
    newModels.reserve(numItems);

    for (unsigned i = 0; i < numItems; i++)
    {
        const auto lower = fb.GetUint16LE();
        const auto upper = fb.GetUint16LE();
        offsets.emplace_back(CalculateOffset(upper, lower));
    }

    for (unsigned i = 0; i < numItems; i++)
    {
        Model newModel{};

        fb.Seek(offsets[i]);
        auto offset = offsets[i];
        newModel.mName = itemNames[i];
        newModel.mEntityFlags = fb.GetUint8();
        newModel.mEntityType = fb.GetUint8();
        newModel.mTerrainType = fb.GetUint8();
        newModel.mScale = fb.GetUint8();

        // uncertain what these are for...
        // animCount seems to be related to the number of face
        // options, e.g. the catapult
        auto animCount = fb.GetUint16LE();
        auto animOffset = fb.GetUint16LE();
        if (newModel.mName == "boom")
        {
            // to keep the indices aligned - not sure what to do with this
            newModels.emplace_back(newModels.back());
            continue;
        }

        auto componentCount = fb.GetUint16LE();
        auto baseOffset = fb.GetUint16LE();
        fb.Skip(2);

        bool bounded = !(newModel.mEntityFlags & EF_UNBOUNDED);
        if (bounded)
        {
            newModel.mMin = fb.LoadVector<std::int16_t, 3>();
            newModel.mMax = fb.LoadVector<std::int16_t, 3>();
        }

        std::vector<ComponentData> componentOffsetDatas;
        for (auto componentI = 0; componentI < componentCount; componentI++)
        {
            fb.Skip(2);
            unsigned meshCount = fb.GetUint16LE();
            unsigned meshOffset = fb.GetUint16LE();
            componentOffsetDatas.emplace_back(ComponentData{meshCount, meshOffset, {}});
        }

        offset += 14 + (bounded ? 12 : 0);

        std::vector<std::pair<unsigned, unsigned>> vertexSets{};
        std::vector<unsigned> vertexSums{};
        for (auto componentI = 0; componentI < componentCount; componentI++)
        {

            auto& component = componentOffsetDatas[componentI];
            fb.Seek(offset + component.mMeshOffset - baseOffset);
            for (unsigned meshI = 0; meshI < component.mMeshCount; meshI++)
            {
                fb.Skip(3);
                unsigned vertices = fb.GetUint8();
                unsigned vertexOffset = fb.GetUint16LE();
                unsigned faceCount = fb.GetUint16LE();
                unsigned faceOffset = fb.GetUint16LE();
                if (vertexSets.empty())
                {
                    vertexSets.emplace_back(vertices, vertexOffset);
                    vertexSums.emplace_back(0);
                }
                else
                {
                    if (std::make_pair(vertices, vertexOffset) != vertexSets.back())
                    {
                        vertexSums.emplace_back(vertexSums.back() + vertexSets.back().first);
                        vertexSets.emplace_back(vertices, vertexOffset);
                    }
                }

                component.mMeshOffsetDatas.emplace_back(MeshOffsetData{vertices, vertexOffset, vertexSums.back(), faceCount, faceOffset});
                fb.Skip(4);
            }
        }

        std::vector<glm::i32vec3> vertices{};
        unsigned vs = 0;
        for (auto [vertexCount, vertexOffset] : vertexSets)
        {
            fb.Seek(offset - baseOffset + vertexOffset);
            for (unsigned vv = 0; vv < vertexCount; vv++)
            {
                vertices.emplace_back(fb.LoadVector<std::int16_t, 3>());
            }
        }

        newModel.mVertices = vertices;

        for (unsigned componentI = 0; componentI < componentCount; componentI++)
        {
            auto& component = newModel.mComponents.emplace_back(Component{{}});
            auto& componentOffsetData = componentOffsetDatas[componentI];
            for (unsigned meshI = 0; meshI < componentOffsetData.mMeshCount; meshI++)
            {
                auto& meshOffsetData = componentOffsetData.mMeshOffsetDatas[meshI];
                auto fo = offset - baseOffset + meshOffsetData.mFaceOffset;
                fb.Seek(fo);
                auto& newMesh = component.mMeshes.emplace_back(Mesh{{}});
                newMesh.mFaceOptions.reserve(meshOffsetData.mFaceCount);
                std::vector<FaceData> faceOffsetDatas;
                for (unsigned faceI = 0; faceI < meshOffsetData.mFaceCount; faceI++)
                {
                    auto& faceOption = newMesh.mFaceOptions.emplace_back(FaceOption{});
                    auto faceType = fb.GetUint16LE();
                    auto edgeCount = fb.GetUint16LE();
                    faceOption.mFaceType = faceType;
                    faceOption.mEdgeCount = edgeCount;
                    auto edgeOffset = fb.GetUint16LE();
                    fb.Skip(2);
                    faceOffsetDatas.emplace_back(FaceData{faceType, edgeCount, edgeOffset});

                    if (faceType == 2)
                    {
                        newModel.mSprite = edgeCount;
                        continue;
                    }
                }

                assert(newMesh.mFaceOptions.size() == meshOffsetData.mFaceCount);
                for (unsigned faceI = 0; faceI < meshOffsetData.mFaceCount; faceI++)
                {
                    auto& faceOption = newMesh.mFaceOptions[faceI];
                    auto& faceOffsetData = faceOffsetDatas[faceI];
                    if (faceOffsetData.mFaceType == 2) continue;
                    fb.Seek(offset - baseOffset + faceOffsetData.mEdgeOffset);
                    unsigned edgeSeekOffset = offset - baseOffset + faceOffsetData.mEdgeOffset;
                    for (unsigned edgeI = 0; edgeI < faceOffsetData.mEdgeCount; edgeI++)
                    {
                        fb.Seek(edgeSeekOffset);
                        auto palette = fb.GetUint8();
                        auto color = fb.LoadVector<std::uint8_t, 4>();
                        auto group = fb.GetUint8();
                        auto vertexOffset = fb.GetUint16LE();
                        edgeSeekOffset = fb.Tell();

                        fb.Seek(offset - baseOffset + vertexOffset);
                        std::uint8_t vertexIndex;
                        std::vector<std::uint16_t> vertexIndices{};
                        while ((vertexIndex = fb.GetUint8()) != 0xff)
                        {
                            vertexIndices.emplace_back(static_cast<std::uint16_t>(vertexIndex) + meshOffsetData.mVertexIndexTransform);
                        }
                        faceOption.mFaceColors.emplace_back(color);
                        faceOption.mPalettes.emplace_back(palette);
                        faceOption.mFaces.emplace_back(vertexIndices);
                    }
                }
            }
        }

        newModels.emplace_back(std::move(newModel));
    }

    for (auto& model : newModels)
    {
        logger.Spam() << "Model: " << model << "\n";
    }

    return newModels;
}

std::vector<Model> LoadTBL(FileBuffer& fb)
{
    const auto& logger = Logging::LogState::GetLogger(__FUNCTION__);

    auto mapbuf = fb.Find(DataTag::MAP);
    auto appbuf = fb.Find(DataTag::APP);
    auto gidbuf = fb.Find(DataTag::GID);
    auto datbuf = fb.Find(DataTag::DAT);

    const auto names = LoadModelNames(mapbuf);
    std::stringstream ss{};
    for (const auto& name : names)
    {
        ss << " " << name << ",";
    }
    logger.Info() << "Loading models: " << ss.str() << "\n";
    return LoadModels(datbuf, names);
}

}
