#include "ShaderManager.hpp"

#include <format>
#include <span>
#include <stdexcept>
#include <vulkan/vulkan_to_string.hpp>

#include "Engine/Core/Utils/Hash.hpp"
#include "Engine/Core/Logger.hpp"

namespace Npgs
{
    template <typename Ty>
    requires std::is_standard_layout_v<Ty> && std::is_trivial_v<Ty>
    inline void FSpecializationConstantBuffer::AddConstant(std::uint32_t Id, const Ty& Value)
    {
        vk::SpecializationMapEntry Entry(Id, static_cast<std::uint32_t>(DataBuffer.size()), sizeof(Ty));
        Entries.push_back(Entry);

        auto BytesView = std::as_bytes(std::span<const Ty>(&Value, 1));
        DataBuffer.append_range(BytesView);
    }

    NPGS_INLINE bool FShaderAcquireInfo::operator==(const FShaderAcquireInfo& Other) const
    {
        return ShaderInfos                   == Other.ShaderInfos &&
               SpecializationConstantBuffer_ == Other.SpecializationConstantBuffer_;
    }

    template <typename Ty>
    requires std::is_standard_layout_v<Ty> && std::is_trivial_v<Ty>
    void FShaderAcquireInfo::AddSpecializationConstant(vk::ShaderStageFlagBits Stage, std::string_view Name, const Ty& Value)
    {
        const auto& ShaderInfo = ShaderInfos.at(Stage);

        ConstantCommitList_.push_back([this, &ShaderInfo](FAssetManager* AssetManager) -> void
        {
            const auto& Shader = AssetManager->AcquireAsset<FShader>(ShaderInfo.Name);

            const std::uint32_t ConstantId = Shader->GetSpecializationConstantId(Name);
            auto it = UsedSpecializationConstantIds_.find(ConstantId);
            if (it != UsedSpecializationConstantIds_.end())
            {
                if (it->second != Name)
                {
                    throw std::runtime_error(std::format(
                        "Specialization constant with ID {} already used in stage {}.", ConstantId, vk::to_string(Stage)));
                }
                else
                {
                    return;
                }
            }

            UsedSpecializationConstantIds_.emplace(ConstantId, Name);
            SpecializationConstantBuffer_.AddConstant(ConstantId, Value);
        });
    }

    NPGS_INLINE std::uint32_t FShaderResource::GetPushConstantOffset(std::string_view Name) const
    {
        return PushConstantOffsetsMap_.at(Name);
    }
} // namespace Npgs
