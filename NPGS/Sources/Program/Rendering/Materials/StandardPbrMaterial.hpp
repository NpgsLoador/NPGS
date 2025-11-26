#pragma once

#include "Engine/Runtime/AssetLoaders/Texture.hpp"
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
        FTexture2D* AlbedoMap_{ nullptr };
        FTexture2D* NormalMap_{ nullptr };
        FTexture2D* ArmMap_{ nullptr };
    };
} // namespace Npgs
