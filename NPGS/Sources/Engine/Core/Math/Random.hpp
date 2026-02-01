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
        TDistribution()                         = default;
        TDistribution(const TDistribution&)     = default;
        TDistribution(TDistribution&&) noexcept = default;
        virtual ~TDistribution()                = default;

        TDistribution& operator=(const TDistribution&)     = default;
        TDistribution& operator=(TDistribution&&) noexcept = default;

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

    template <typename BaseType = float, typename RandomEngine = std::mt19937>
    requires std::is_class_v<RandomEngine>
    class TGammaDistribution : public TDistribution<BaseType, RandomEngine>
    {
    public:
        TGammaDistribution() = default;
        TGammaDistribution(BaseType Alpha, BaseType Beta)
            : Distribution_(Alpha, Beta)
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
            return std::make_unique<TGammaDistribution<BaseType, RandomEngine>>(*this);
        }

    private:
        std::gamma_distribution<BaseType> Distribution_;
    };

    template <typename BaseType = float, typename RandomEngine = std::mt19937>
    requires std::is_class_v<RandomEngine>
    class TExponentialDistribution : public TDistribution<BaseType, RandomEngine>
    {
    public:
        TExponentialDistribution() = default;
        TExponentialDistribution(BaseType Lambda)
            : Distribution_(Lambda)
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
            return std::make_unique<TExponentialDistribution<BaseType, RandomEngine>>(*this);
        }

    private:
        std::exponential_distribution<BaseType> Distribution_;
    };

    template <typename BaseType = float, typename RandomEngine = std::mt19937>
    requires std::is_class_v<RandomEngine>
    class TPoissonDistribution : public TDistribution<BaseType, RandomEngine>
    {
    public:
        TPoissonDistribution() = default;
        TPoissonDistribution(BaseType Mean)
            : Distribution_(Mean)
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
            return std::make_unique<TPoissonDistribution<BaseType, RandomEngine>>(*this);
        }

    private:
        std::poisson_distribution<BaseType> Distribution_;
    };

    template <typename BaseType = float, typename RandomEngine = std::mt19937>
    requires std::is_class_v<RandomEngine>
    class TWeibullDistribution : public TDistribution<BaseType, RandomEngine>
    {
    public:
        TWeibullDistribution() = default;
        TWeibullDistribution(BaseType Ax, BaseType Bx)
            : Distribution_(Ax, Bx)
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
            return std::make_unique<TWeibullDistribution<BaseType, RandomEngine>>(*this);
        }

    private:
        std::weibull_distribution<BaseType> Distribution_;
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
