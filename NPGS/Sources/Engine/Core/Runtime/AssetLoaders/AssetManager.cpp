#include "AssetManager.h"

#include <cstdlib>
#include "Engine/Core/Base/Assert.h"

_NPGS_BEGIN
_RUNTIME_BEGIN
_ASSET_BEGIN

std::string GetAssetFullPath(EAssetType Type, const std::string& Filename)
{
    std::string RootFolderName = Type == EAssetType::kBinaryShader ? "" : "Assets/";
#ifdef _RELEASE
    RootFolderName = std::string("../") + RootFolderName;
#endif // _RELEASE

    std::string AssetFolderName;
    switch (Type)
    {
    case EAssetType::kBinaryShader:
        AssetFolderName = "Cache/Shaders/";
        break;
    case EAssetType::kDataTable:
        AssetFolderName = "DataTables/";
        break;
    case EAssetType::kFont:
        AssetFolderName = "Fonts/";
        break;
    case EAssetType::kModel:
        AssetFolderName = "Models/";
        break;
    case EAssetType::kShader:
        AssetFolderName = "Shaders/";
        break;
    case EAssetType::kTexture:
        AssetFolderName = "Textures/";
        break;
    default:
        NpgsAssert(false, "Invalid asset type");
    };

    return RootFolderName + AssetFolderName + Filename;
}

FAssetManager::~FAssetManager()
{
    ClearAssets();
}

FAssetManager* FAssetManager::GetInstance()
{
    static FAssetManager kInstance;
    return &kInstance;
}

_ASSET_END
_RUNTIME_END
_NPGS_END
