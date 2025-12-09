#pragma once

#include "Engine/Runtime/AssetLoaders/Texture.hpp"
#include "Engine/Runtime/Managers/AssetManager.hpp"
#include "Engine/Runtime/Graphics/Renderer/Material.hpp"

namespace Npgs
{
    class FStandardPbrMaterial : public IMaterial
    {
    public:
        using Base = IMaterial;
        using Base::Base;

    private:
        void LoadAssets() override;
        void BindDescriptors() override;

    private:
        TAssetHandle<FTexture2D> AlbedoMap_;
        TAssetHandle<FTexture2D> NormalMap_;
        TAssetHandle<FTexture2D> ArmMap_;
    };
} // namespace Npgs
