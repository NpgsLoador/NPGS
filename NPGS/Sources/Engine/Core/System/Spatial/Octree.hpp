#pragma once

#include <cmath>
#include <algorithm>
#include <array>
#include <concepts>
#include <functional>
#include <future>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

#include <glm/glm.hpp>
#include "Engine/Core/Runtime/Pools/ThreadPool.h"
#include "Engine/Core/System/Services/EngineServices.h"

namespace Npgs
{
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

        std::unique_ptr<TOctreeNode>& GetNext(int Index)
        {
            return Next_[Index];
        }

        const std::unique_ptr<TOctreeNode>& GetNext(int Index) const
        {
            return Next_[Index];
        }

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
                if (Next != nullptr)
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

        std::array<std::unique_ptr<TOctreeNode>, 8> Next_;
        std::vector<glm::vec3>                      Points_;
        std::vector<LinkTargetType*>                DataLink_;
    };

    template <typename LinkTargetType>
    class TOctree
    {
    public:
        using FNodeType = TOctreeNode<LinkTargetType>;

    public:
        TOctree(glm::vec3 Center, float Radius, int MaxDepth = 8)
            : Root_(std::make_unique<FNodeType>(Center, Radius, nullptr))
            , ThreadPool_(EngineCoreServices->GetThreadPool())
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
            TraverseImpl(Root_.get(), std::forward<Func>(Pred));
        }

        std::size_t GetCapacity() const
        {
            return GetCapacityImpl(Root_.get());
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

                Node->GetNext(i) = std::make_unique<FNodeType>(Node->GetCenter() + Offset, NextRadius, Node);
                if (Depth == static_cast<int>(std::ceil(std::log2(Root_->GetRadius() / LeafRadius))))
                {
                    Futures.push_back(ThreadPool_->Submit(&TOctree::BuildEmptyTreeImpl, this, Node->GetNext(i).get(), LeafRadius, Depth - 1));
                }
                else
                {
                    BuildEmptyTreeImpl(Node->GetNext(i).get(), LeafRadius, Depth - 1);
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

            if (Node->GetNext(0) == nullptr)
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

                    Node->GetNext(i) = std::make_unique<FNodeType>(NewCenter, Radius * 0.5f, Node);
                }
            }

            int Octant = Node->CalculateOctant(Point);
            if (Depth == MaxDepth_)
            {
                Node->AddPoint(Point);
            }
            else
            {
                InsertImpl(Node->GetNext(Octant).get(), Point, Depth + 1);
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
                    DeleteImpl(Node->GetNext(Octant).get(), Point);
                }

                if (Node->IsLeafNode() && Node->GetPoints().empty())
                {
                    for (int i = 0; i != 8; ++i)
                    {
                        auto& Next = Node->GetNext(i);
                        Next.reset();
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
                FNodeType* NextNode = Node->GetNext(i).get();
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
                FNodeType* ResultNode = FindImpl(Node->GetNext(i).get(), Point, Pred);
                if (ResultNode != nullptr)
                {
                    return ResultNode;
                }
            }

            return nullptr;
        }

        template <typename Func>
        void TraverseImpl(FNodeType* Node, Func&& Pred) const
        {
            if (Node == nullptr)
            {
                return;
            }

            Pred(*Node);

            for (int i = 0; i != 8; ++i)
            {
                TraverseImpl(Node->GetNext(i).get(), Pred);
            }
        }

        std::size_t GetCapacityImpl(const FNodeType* Node) const
        {
            if (Node == nullptr)
            {
                return 0;
            }

            if (Node->GetNext(0) == nullptr)
            {
                return Node->IsValid() ? 1 : 0;
            }

            std::size_t Capacity = 0;
            for (int i = 0; i != 8; ++i)
            {
                Capacity += GetCapacityImpl(Node->GetNext(i).get());
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
                Size += GetSizeImpl(Node->GetNext(i).get());
            }

            return Size;
        }

    private:
        std::unique_ptr<FNodeType> Root_;
        FThreadPool*               ThreadPool_;
        int                        MaxDepth_;
    };
} // namespace Npgs
