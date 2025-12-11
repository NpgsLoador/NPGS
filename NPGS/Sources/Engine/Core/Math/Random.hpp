#pragma once

#include <random>
#include <memory>
#include <type_traits>

namespace Npgs::Math
{
    template <typename BaseType = float, typename RandomEngine = std::mt19937>
    requires std::is_class_v<RandomEngine>
    class TDistribution
    {
    public:
        virtual ~TDistribution()                             = default;
        virtual BaseType operator()(RandomEngine& Engine)    = 0;
        virtual BaseType Generate(RandomEngine& Engine)      = 0;
        virtual std::unique_ptr<TDistribution> Clone() const = 0;
    };

    template <typename BaseType = int, typename RandomEngine = std::mt19937>
    requires std::is_class_v<RandomEngine>
    class TUniformIntDistribution : public TDistribution<BaseType>
    {
    public:
        TUniformIntDistribution() = default;
        TUniformIntDistribution(BaseType Min, BaseType Max)
            : Distribution_(Min, Max)
        {
        }

        BaseType operator()(RandomEngine& Engine) override
        {
            return Distribution_(Engine);
        }

        BaseType Generate(RandomEngine& Engine) override
        {
            return operator()(Engine);
        }

        std::unique_ptr<TDistribution<BaseType, RandomEngine>> Clone() const override
        {
            return std::make_unique<TUniformIntDistribution<BaseType, RandomEngine>>(*this);
        }

    private:
        std::uniform_int_distribution<BaseType> Distribution_;
    };

    template <typename BaseType = float, typename RandomEngine = std::mt19937>
    requires std::is_class_v<RandomEngine>
    class TUniformRealDistribution : public TDistribution<BaseType, RandomEngine>
    {
    public:
        TUniformRealDistribution() = default;
        TUniformRealDistribution(BaseType Min, BaseType Max)
            : Distribution_(Min, Max)
        {
        }

        BaseType operator()(RandomEngine& Engine) override
        {
            return Distribution_(Engine);
        }

        BaseType Generate(RandomEngine& Engine) override
        {
            return operator()(Engine);
        }

        std::unique_ptr<TDistribution<BaseType, RandomEngine>> Clone() const override
        {
            return std::make_unique<TUniformRealDistribution<BaseType, RandomEngine>>(*this);
        }

    private:
        std::uniform_real_distribution<BaseType> Distribution_;
    };

    template <typename BaseType = float, typename RandomEngine = std::mt19937>
    requires std::is_class_v<RandomEngine>
    class TNormalDistribution : public TDistribution<BaseType, RandomEngine>
    {
    public:
        TNormalDistribution() = default;
        TNormalDistribution(BaseType Mean, BaseType Sigma)
            : Distribution_(Mean, Sigma)
        {
        }

        BaseType operator()(RandomEngine& Engine) override
        {
            return Distribution_(Engine);
        }

        BaseType Generate(RandomEngine& Engine) override
        {
            return operator()(Engine);
        }

        std::unique_ptr<TDistribution<BaseType, RandomEngine>> Clone() const override
        {
            return std::make_unique<TNormalDistribution<BaseType, RandomEngine>>(*this);
        }

    private:
        std::normal_distribution<BaseType> Distribution_;
    };

    template <typename BaseType = float, typename RandomEngine = std::mt19937>
    requires std::is_class_v<RandomEngine>
    class TLogNormalDistribution : public TDistribution<BaseType, RandomEngine>
    {
    public:
        TLogNormalDistribution() = default;
        TLogNormalDistribution(BaseType Mean, BaseType Sigma)
            : Distribution_(Mean, Sigma)
        {
        }

        BaseType operator()(RandomEngine& Engine) override
        {
            return Distribution_(Engine);
        }

        BaseType Generate(RandomEngine& Engine) override
        {
            return operator()(Engine);
        }

        std::unique_ptr<TDistribution<BaseType, RandomEngine>> Clone() const override
        {
            return std::make_unique<TLogNormalDistribution<BaseType, RandomEngine>>(*this);
        }

    private:
        std::lognormal_distribution<BaseType> Distribution_;
    };

    template <typename RandomEngine = std::mt19937>
    requires std::is_class_v<RandomEngine>
    class TBernoulliDistribution : public TDistribution<double, RandomEngine>
    {
    public:
        TBernoulliDistribution() = default;
        TBernoulliDistribution(double Probability)
            : Distribution_(Probability)
        {
        }

        double operator()(RandomEngine& Engine) override
        {
            return Distribution_(Engine);
        }

        double Generate(RandomEngine& Engine) override
        {
            return operator()(Engine);
        }

        std::unique_ptr<TDistribution<double, RandomEngine>> Clone() const override
        {
            return std::make_unique<TBernoulliDistribution<RandomEngine>>(*this);
        }

    private:
        std::bernoulli_distribution Distribution_;
    };
} // namespace Npgs::Utils
