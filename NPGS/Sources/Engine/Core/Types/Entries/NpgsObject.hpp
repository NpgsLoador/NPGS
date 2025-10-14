#pragma once

namespace Npgs
{
    class INpgsObject
    {
    public:
        INpgsObject()                   = default;
		INpgsObject(const INpgsObject&) = default;
		INpgsObject(INpgsObject&&)      = default;
        virtual ~INpgsObject()          = default;

		INpgsObject& operator=(const INpgsObject&) = default;
		INpgsObject& operator=(INpgsObject&&)      = default;
    };
} // namespace Npgs
