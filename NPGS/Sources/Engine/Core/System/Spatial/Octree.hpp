#pragma once

#include <cmath>
#include <algorithm>
#include <array>
#include <atomic>
#include <concepts>
#include <functional>
#include <future>
#include <mutex>
#include <type_traits>
#include <utility>
#include <vector>

#include <glm/glm.hpp>

#include "Engine/Core/Runtime/Pools/MemoryPool.hpp"
#include "Engine/Core/Runtime/Pools/ThreadPool.hpp"
#include "Engine/Core/System/Services/EngineServices.hpp"

#ifdef OCTREE_USE_MEMORY_POOL
#define GetNextNode(Node, i) Node->GetNext(i).Get()
#else
#define GetNextNode(Node, i) Node->GetNext(i).get()
#endif // OCTREE_USE_MEMORY_POOL

namespace Npgs
{
    namespace
    {
        std::size_t CalculateMemoryPoolCapacity(int MaxDepth)
        {
            if (MaxDepth <= 0)
            {
                return 0;
            }

            double Capacity = (8.0 * (std::pow(8.0, static_cast<double>(MaxDepth)) - 1.0)) / 7.0;
            return static_cast<std::size_t>(Capacity);
        }
    }

    template <typename LinkTargetType>
    class TOctreeNode
    {
    public:
        TOctreeNode(glm::vec3 Center, float Radius, TOctreeNode* Previous)
            : Center_(Center), Previous_(Previous), Radius_(Radius)
        {
        }

        bool Contains(glm::vec3 Point) const
        {
            return (Point.x >= Center_.x - Radius_ && Point.x <= Center_.x + Radius_ &&
                    Point.y >= Center_.y - Radius_ && Point.y <= Center_.y + Radius_ &&
                    Point.z >= Center_.z - Radius_ && Point.z <= Center_.z + Radius_);
        }

        int CalculateOctant(glm::vec3 Point) const
        {
            int Octant = 0;

            if (Point.z < Center_.z)
            {
                Octant |= 4;
            }

            if (Point.x >= Center_.x)
            {
                if (Point.y >= Center_.y)
                {
                    Octant |= 0; // (+x, +y)，第一象限
                }
                else
                {
                    Octant |= 3; // (+x, -y)，第四象限
                }
            }
            else
            {
                if (Point.y >= Center_.y)
                {
                    Octant |= 1; // (-x, +y)，第二象限
                }
                else
                {
                    Octant |= 2; // (-x, -y)，第三象限
                }
            }

            return Octant;
        }

        bool IntersectSphere(glm::vec3 Point, float Radius) const
        {
            glm::vec3 MinBound = Center_ - glm::vec3(Radius_);
            glm::vec3 MaxBound = Center_ + glm::vec3(Radius_);

            glm::vec3 ClosestPoint = glm::clamp(Point, MinBound, MaxBound);
            float Distance = glm::distance(Point, ClosestPoint);

            return Distance <= Radius;
        }

        const bool IsValid() const
        {
            return bIsValid_;
        }

        glm::vec3 GetCenter() const
        {
            return Center_;
        }

        const TOctreeNode* GetPrevious() const
        {
            return Previous_;
        }

        TOctreeNode* GetPrevious()
        {
            return Previous_;
        }

        float GetRadius() const
        {
            return Radius_;
        }

#ifdef OCTREE_USE_MEMORY_POOL
        TMemoryPool<TOctreeNode>::FMemoryGuard& GetNext(int Index)
        {
            return Next_[Index];
        }

        const TMemoryPool<TOctreeNode>::FMemoryGuard& GetNext(int Index) const
        {
            return Next_[Index];
        }
#else
        std::unique_ptr<TOctreeNode>& GetNext(int Index)
        {
            return Next_[Index];
        }

        const std::unique_ptr<TOctreeNode>& GetNext(int Index) const
        {
            return Next_[Index];
        }
#endif // OCTREE_USE_MEMORY_POOL

        void AddPoint(glm::vec3 Point)
        {
            Points_.push_back(Point);
        }

        void DeletePoint(glm::vec3 Point)
        {
            auto it = std::ranges::find(Points_, Point);
            if (it != Points_.end())
            {
                Points_.erase(it);
            }
        }

        void RemoveStorage()
        {
            Points_.clear();
        }

        void AddLink(LinkTargetType* Target)
        {
            DataLink_.push_back(Target);
        }

        template <typename Func>
        requires std::predicate<Func, const LinkTargetType*> || std::predicate<Func, LinkTargetType*> || std::predicate<Func, void*>
        LinkTargetType* GetLink(Func&& Pred) const
        {
            for (LinkTargetType* Target : DataLink_)
            {
                if (Pred(Target))
                {
                    return Target;
                }
            }

            return nullptr;
        }

        void RemoveLinks()
        {
            DataLink_.clear();
        }

        std::vector<glm::vec3>& GetPoints()
        {
            return Points_;
        }

        const std::vector<glm::vec3>& GetPoints() const
        {
            return Points_;
        }

        void SetValidation(bool bValidation)
        {
            bIsValid_ = bValidation;
        }

        bool IsLeafNode() const
        {
            for (const auto& Next : Next_)
            {
#ifdef OCTREE_USE_MEMORY_POOL
                if (static_cast<bool>(Next))
#else
                if (Next != nullptr)
#endif // OCTREE_USE_MEMORY_POOL
                {
                    return false;
                }
            }

            return true;
        }

    private:
        glm::vec3    Center_;
        TOctreeNode* Previous_;
        float        Radius_;
        bool         bIsValid_{ true };

#ifdef OCTREE_USE_MEMORY_POOL
        std::array<typename TMemoryPool<TOctreeNode>::FMemoryGuard, 8> Next_;
#else
        std::array<std::unique_ptr<TOctreeNode>, 8> Next_;
#endif // OCTREE_USE_MEMORY_POOL
        std::vector<glm::vec3>       Points_;
        std::vector<LinkTargetType*> DataLink_;
    };

    template <typename LinkTargetType>
    class TOctree
    {
    public:
        using FNodeType = TOctreeNode<LinkTargetType>;

    public:
        TOctree(glm::vec3 Center, float Radius, int MaxDepth = 8)
            : ThreadPool_(EngineCoreServices->GetThreadPool())
#ifdef OCTREE_USE_MEMORY_POOL
            , MemoryPool_(CalculateMemoryPoolCapacity(MaxDepth))
#endif // OCTREE_USE_MEMORY_POOL
            , Root_(std::make_unique<FNodeType>(Center, Radius, nullptr))
            , MaxDepth_(MaxDepth)
        {
        }

        void BuildEmptyTree(float LeafRadius)
        {
            int Depth = static_cast<int>(std::ceil(std::log2(Root_->GetRadius() / LeafRadius)));
            BuildEmptyTreeImpl(Root_.get(), LeafRadius, Depth);
        }

        void Insert(glm::vec3 Point)
        {
            InsertImpl(Root_.get(), Point, 0);
        }

        void Delete(glm::vec3 Point)
        {
            DeleteImpl(Root_.get(), Point);
        }

        void Query(glm::vec3 Point, float Radius, std::vector<glm::vec3>& Results) const
        {
            QueryImpl(Root_.get(), Point, Radius, Results);
        }

        template <typename Func = std::function<bool(const FNodeType&)>>
        requires std::predicate<Func, const FNodeType&> || std::predicate<Func, FNodeType&>
        FNodeType* Find(glm::vec3 Point, Func&& Pred = [](const FNodeType&) -> bool { return true; }) const
        {
            return FindImpl(Root_.get(), Point, std::forward<Func>(Pred));
        }

        template <typename Func>
        requires std::is_invocable_r_v<void, Func, const FNodeType&> || std::is_invocable_r_v<void, Func, FNodeType&>
        void Traverse(Func&& Pred) const
        {
            std::mutex Mutex;
            bool bParallel = MaxDepth_ >= 10 ? true : false;
            TraverseImpl(Root_.get(), Mutex, bParallel, std::forward<Func>(Pred));
        }

        std::size_t GetCapacity() const
        {
            bool bParallel = MaxDepth_ >= 10 ? true : false;
            return GetCapacityImpl(Root_.get(), bParallel);
        }

        std::size_t GetSize() const
        {
            return GetSizeImpl(Root_.get());
        }

        const FNodeType* const GetRoot() const
        {
            return Root_.get();
        }

    private:
        void BuildEmptyTreeImpl(FNodeType* Node, float LeafRadius, int Depth)
        {
            if (Node->GetRadius() <= LeafRadius || Depth == 0)
            {
                return;
            }

            std::vector<std::future<void>> Futures;
            float NextRadius = Node->GetRadius() * 0.5f;
            for (int i = 0; i != 8; ++i)
            {
                glm::vec3 Offset(0.0f);
                switch (i & 3)
                {
                case 0: // (+x, +y)
                    Offset.x =  NextRadius;
                    Offset.y =  NextRadius;
                    break;
                case 1: // (-x, +y)
                    Offset.x = -NextRadius;
                    Offset.y =  NextRadius;
                    break;
                case 2: // (-x, -y)
                    Offset.x = -NextRadius;
                    Offset.y = -NextRadius;
                    break;
                case 3: // (+x, -y)
                    Offset.x =  NextRadius;
                    Offset.y = -NextRadius;
                    break;
                }
                Offset.z = (i & 4) ? -NextRadius : NextRadius;

#ifdef OCTREE_USE_MEMORY_POOL
                Node->GetNext(i) = std::move(MemoryPool_.Allocate(Node->GetCenter() + Offset, NextRadius, Node));
#else
                Node->GetNext(i) = std::make_unique<FNodeType>(Node->GetCenter() + Offset, NextRadius, Node);
#endif // OCTREE_USE_MEMORY_POOL
                if (Depth == static_cast<int>(std::ceil(std::log2(Root_->GetRadius() / LeafRadius))))
                {
                    Futures.push_back(ThreadPool_->Submit(&TOctree::BuildEmptyTreeImpl, this, GetNextNode(Node, i), LeafRadius, Depth - 1));
                }
                else
                {
                    BuildEmptyTreeImpl(GetNextNode(Node, i), LeafRadius, Depth - 1);
                }
            }

            for (auto& Future : Futures)
            {
                Future.get();
            }
        }

        void InsertImpl(FNodeType* Node, glm::vec3 Point, int Depth)
        {
            if (!Node->Contains(Point) || Depth > MaxDepth_)
            {
                return;
            }

            if (Node->IsLeafNode())
            {
                for (int i = 0; i != 8; ++i)
                {
                    glm::vec3 NewCenter = Node->GetCenter();
                    float Radius = Node->GetRadius();

                    switch (i & 3)
                    {
                    case 0: // (+x, +y)
                        NewCenter.x += Radius * 0.5f;
                        NewCenter.y += Radius * 0.5f;
                        break;
                    case 1: // (-x, +y)
                        NewCenter.x -= Radius * 0.5f;
                        NewCenter.y += Radius * 0.5f;
                        break;
                    case 2: // (-x, -y)
                        NewCenter.x -= Radius * 0.5f;
                        NewCenter.y -= Radius * 0.5f;
                        break;
                    case 3: // (+x, -y)
                        NewCenter.x += Radius * 0.5f;
                        NewCenter.y -= Radius * 0.5f;
                        break;
                    }
                    NewCenter.z += ((i & 4) ? -1 : 1) * Radius * 0.5f;

#ifdef OCTREE_USE_MEMORY_POOL
                    Node->GetNext(i) = std::move(MemoryPool_.Allocate(NewCenter, Radius * 0.5f, Node));
#else
                    Node->GetNext(i) = std::make_unique<FNodeType>(NewCenter, Radius * 0.5f, Node);
#endif // OCTREE_USE_MEMORY_POOL
                }
            }

            int Octant = Node->CalculateOctant(Point);
            if (Depth == MaxDepth_)
            {
                Node->AddPoint(Point);
            }
            else
            {
                InsertImpl(&(*Node->GetNext(Octant)), Point, Depth + 1);
            }
        }

        void DeleteImpl(FNodeType* Node, glm::vec3 Point)
        {
            if (Node == nullptr)
            {
                return;
            }

            if (Node->Contains(Point))
            {
                if (Node->IsLeafNode())
                {
                    auto& Points = Node->GetPoints();
                    auto it = std::ranges::find(Points, Point);
                    if (it != Points.end())
                    {
                        Points.erase(it);
                    }
                }
                else
                {
                    int Octant = Node->CalculateOctant(Point);
                    DeleteImpl(&(*Node->GetNext(Octant)), Point);
                }

                if (Node->IsLeafNode() && Node->GetPoints().empty())
                {
                    for (int i = 0; i != 8; ++i)
                    {
                        auto& Next = Node->GetNext(i);
#ifdef OCTREE_USE_MEMORY_POOL
                        Next.~FMemoryGuard();
#else
                        Next.reset();
#endif // OCTREE_USE_MEMORY_POOL
                    }
                }
            }
        }

        void QueryImpl(FNodeType* Node, glm::vec3 Point, float Radius, std::vector<glm::vec3>& Results) const
        {
            if (Node == nullptr || Node->GetNext(0) == nullptr)
            {
                return;
            }

            for (const auto& StoredPoint : Node->GetPoints())
            {
                if (glm::distance(StoredPoint, Point) <= Radius && StoredPoint != Point)
                {
                    Results.push_back(StoredPoint);
                }
            }

            for (int i = 0; i != 8; ++i)
            {
                FNodeType* NextNode = GetNextNode(Node, i);
                if (NextNode != nullptr && NextNode->IntersectSphere(Point, Radius))
                {
                    QueryImpl(NextNode, Point, Radius, Results);
                }
            }
        }

        template <typename Func>
        FNodeType* FindImpl(FNodeType* Node, glm::vec3 Point, Func&& Pred) const
        {
            if (Node == nullptr)
            {
                return nullptr;
            }

            if (Node->Contains(Point))
            {
                if (Pred(*Node))
                {
                    return Node;
                }
            }

            for (int i = 0; i != 8; ++i)
            {
                FNodeType* ResultNode = FindImpl(GetNextNode(Node, i), Point, Pred);
                if (ResultNode != nullptr)
                {
                    return ResultNode;
                }
            }

            return nullptr;
        }

        template <typename Func>
        void TraverseImpl(FNodeType* Node, std::mutex& Mutex, bool bParallel, Func&& Pred) const
        {
            if (Node == nullptr)
            {
                return;
            }

            {
                std::lock_guard Lock(Mutex);
                Pred(*Node);
            }

            if (bParallel)
            {
                bParallel = false;
                std::vector<std::future<void>> Futures;
                for (int i = 0; i != 8; ++i)
                {
                    auto* NextNode = GetNextNode(Node, i);
                    Futures.push_back(ThreadPool_->Submit([this, NextNode, &Mutex, bParallel, Pred]() -> void
                    {
                        this->TraverseImpl(NextNode, Mutex, bParallel, Pred);
                    }));
                }

                for (auto& Future : Futures)
                {
                    Future.get();
                }
            }
            else
            {
                for (int i = 0; i != 8; ++i)
                {
                    TraverseImpl(GetNextNode(Node, i), Mutex, bParallel, Pred);
                }
            }
        }

        std::size_t GetCapacityImpl(const FNodeType* Node, bool bParallel) const
        {
            if (Node == nullptr)
            {
                return 0;
            }

            if (Node->GetNext(0) == nullptr)
            {
                return Node->IsValid() ? 1 : 0;
            }

            std::atomic<std::size_t> Capacity = 0;
            if (bParallel)
            {
                bParallel = false;
                std::vector<std::future<std::size_t>> Futures;
                for (int i = 0; i != 8; ++i)
                {
                    Futures.push_back(ThreadPool_->Submit(&TOctree::GetCapacityImpl, this, GetNextNode(Node, i), bParallel));
                }

                for (auto& Future : Futures)
                {
                    Capacity += Future.get();
                }
            }
            else
            {
                for (int i = 0; i != 8; ++i)
                {
                    Capacity += GetCapacityImpl(GetNextNode(Node, i), bParallel);
                }
            }

            return Capacity;
        }

        std::size_t GetSizeImpl(const FNodeType* Node) const
        {
            if (Node == nullptr)
            {
                return 0;
            }

            std::size_t Size = Node->GetPoints().size();
            for (int i = 0; i != 8; ++i)
            {
                Size += GetSizeImpl(GetNextNode(Node, i));
            }

            return Size;
        }

    private:
#ifdef OCTREE_USE_MEMORY_POOL
        TMemoryPool<FNodeType>     MemoryPool_;
#endif // OCTREE_USE_MEMORY_POOL
        FThreadPool*               ThreadPool_;
        std::unique_ptr<FNodeType> Root_;
        int                        MaxDepth_;
    };
} // namespace Npgs

#undef GetNextNode
