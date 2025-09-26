#pragma once

#include "Engine/Core/Base/Assert.hpp"
#include "Engine/Core/Base/Base.hpp"

#include "Engine/Core/Math/NumericConstants.hpp"
#include "Engine/Core/Math/TangentSpaceTools.hpp"

#include "Engine/Core/Runtime/AssetLoaders/CommaSeparatedValues.hpp"
#include "Engine/Core/Runtime/AssetLoaders/Shader.hpp"
#include "Engine/Core/Runtime/AssetLoaders/Texture.hpp"

#include "Engine/Core/Runtime/Graphics/Buffers/BufferStructs.hpp"
#include "Engine/Core/Runtime/Graphics/Resources/Attachment.hpp"
#include "Engine/Core/Runtime/Graphics/Resources/DeviceLocalBuffer.hpp"
#include "Engine/Core/Runtime/Graphics/Resources/StagingBuffer.hpp"
#include "Engine/Core/Runtime/Graphics/Vulkan/Context.hpp"
#include "Engine/Core/Runtime/Graphics/Vulkan/Core.hpp"
#include "Engine/Core/Runtime/Graphics/Vulkan/Wrappers.hpp"
#include "Engine/Core/Runtime/Managers/AssetManager.hpp"
#include "Engine/Core/Runtime/Managers/PipelineManager.hpp"
#include "Engine/Core/Runtime/Managers/ShaderBufferManager.hpp"
#include "Engine/Core/Runtime/Pools/CommandPoolPool.hpp"
#include "Engine/Core/Runtime/Pools/ResourcePool.hpp"
#include "Engine/Core/Runtime/Pools/StagingBufferPool.hpp"
#include "Engine/Core/Runtime/Pools/ThreadPool.hpp"

#include "Engine/Core/System/Generators/CivilizationGenerator.hpp"
#include "Engine/Core/System/Generators/OrbitalGenerator.hpp"
#include "Engine/Core/System/Generators/StellarGenerator.hpp"

#include "Engine/Core/System/Services/CoreServices.hpp"
#include "Engine/Core/System/Services/EngineServices.hpp"
#include "Engine/Core/System/Services/ResourceServices.hpp"

#include "Engine/Core/System/Spatial/Camera.hpp"
#include "Engine/Core/System/Spatial/Octree.hpp"

#include "Engine/Core/Types/Entries/Astro/CelestialObject.hpp"
#include "Engine/Core/Types/Entries/Astro/Planet.hpp"
#include "Engine/Core/Types/Entries/Astro/Star.hpp"
#include "Engine/Core/Types/Entries/Astro/StellarSystem.hpp"
#include "Engine/Core/Types/Entries/NpgsObject.hpp"

#include "Engine/Core/Types/Properties/Intelli/Artifact.hpp"
#include "Engine/Core/Types/Properties/Intelli/Civilization.hpp"
#include "Engine/Core/Types/Properties/StellarClass.hpp"

#include "Engine/Utils/Logger.hpp"
#include "Engine/Utils/Random.hpp"
#include "Engine/Utils/Utils.hpp"
