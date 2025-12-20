#pragma once

#include "Engine/Core/Base/Assert.hpp"
#include "Engine/Core/Base/Base.hpp"

#include "Engine/Core/Math/NumericConstants.hpp"
#include "Engine/Core/Math/Random.hpp"
#include "Engine/Core/Math/TangentSpaceTools.hpp"

#include "Engine/Runtime/AssetLoaders/CommaSeparatedValues.hpp"
#include "Engine/Runtime/AssetLoaders/Shader.hpp"
#include "Engine/Runtime/AssetLoaders/Texture.hpp"

#include "Engine/Runtime/Graphics/Buffers/BufferStructs.hpp"
#include "Engine/Runtime/Graphics/Resources/Attachment.hpp"
#include "Engine/Runtime/Graphics/Resources/DeviceLocalBuffer.hpp"
#include "Engine/Runtime/Graphics/Resources/StagingBuffer.hpp"
#include "Engine/Runtime/Graphics/Vulkan/Context.hpp"
#include "Engine/Runtime/Graphics/Vulkan/Core.hpp"
#include "Engine/Runtime/Graphics/Vulkan/Wrappers.hpp"
#include "Engine/Runtime/Managers/AssetManager.hpp"
#include "Engine/Runtime/Managers/PipelineManager.hpp"
#include "Engine/Runtime/Managers/ShaderBufferManager.hpp"
#include "Engine/Runtime/Pools/CommandPoolPool.hpp"
#include "Engine/Runtime/Pools/ResourcePool.hpp"
#include "Engine/Runtime/Pools/StagingBufferPool.hpp"
#include "Engine/Runtime/Pools/ThreadPool.hpp"

#include "Engine/System/Generators/CivilizationGenerator.hpp"
#include "Engine/System/Generators/OrbitalGenerator.hpp"
#include "Engine/System/Generators/StellarGenerator.hpp"

#include "Engine/System/Services/CoreServices.hpp"
#include "Engine/System/Services/EngineServices.hpp"
#include "Engine/System/Services/ResourceServices.hpp"

#include "Engine/System/Spatial/Camera.hpp"
#include "Engine/System/Spatial/Octree.hpp"

#include "Engine/Core/Types/Entries/Astro/CelestialObject.hpp"
#include "Engine/Core/Types/Entries/Astro/Planet.hpp"
#include "Engine/Core/Types/Entries/Astro/Star.hpp"
#include "Engine/Core/Types/Entries/Astro/OrbitalSystem.hpp"
#include "Engine/Core/Types/Entries/NpgsObject.hpp"

#include "Engine/Core/Types/Properties/Intelli/Artifact.hpp"
#include "Engine/Core/Types/Properties/Intelli/Civilization.hpp"
#include "Engine/Core/Types/Properties/StellarClass.hpp"

#include "Engine/Core/Utils/Utils.hpp"
#include "Engine/Core/Logger.hpp"
