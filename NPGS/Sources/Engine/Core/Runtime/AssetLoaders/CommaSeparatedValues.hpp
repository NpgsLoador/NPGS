#pragma once

#include <cstddef>
#include <algorithm>
#include <charconv>
#include <concepts>
#include <functional>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include <fast-cpp-csv-parser/csv.h>

namespace Npgs
{
    template <typename BaseType, std::size_t ColSize>
    concept CValidFormat = std::is_nothrow_move_constructible_v<BaseType> && ColSize > 1;

    template <typename BaseType, std::size_t ColSize>
    requires CValidFormat<BaseType, ColSize>
    class TCommaSeparatedValues
    {
    public:
        using FRowArray = std::vector<BaseType>;

    public:
        TCommaSeparatedValues(std::string Filename, std::vector<std::string> ColNames)
            : Filename_(std::move(Filename))
            , ColNames_(std::move(ColNames))
        {
            InitializeHeaderMap();
            ReadData(io::ignore_extra_column);
        }

        FRowArray FindFirstDataArray(const std::string& DataHeader, const BaseType& DataValue) const
        {
            std::size_t DataIndex = GetHeaderIndex(DataHeader);
            for (const auto& Row : Data_)
            {
                if (Row[DataIndex] == DataValue)
                {
                    return Row;
                }
            }

            throw std::out_of_range("Data not found.");
        }

        BaseType FindMatchingValue(const std::string& DataHeader, const BaseType& DataValue, const std::string& TargetHeader) const
        {
            std::size_t DataIndex   = GetHeaderIndex(DataHeader);
            std::size_t TargetIndex = GetHeaderIndex(TargetHeader);
            for (const auto& Row : Data_)
            {
                if (Row[DataIndex] == DataValue)
                {
                    return Row[TargetIndex];
                }
            }

            throw std::out_of_range("Data not found.");
        }

        template <typename Func = std::less<>>
        requires std::predicate<Func, BaseType, BaseType>
        std::pair<FRowArray, FRowArray> FindSurroundingValues(const std::string& DataHeader, const BaseType& TargetValue,
                                                              bool bSorted = true, Func&& Pred = Func())
        {
            std::size_t DataIndex = GetHeaderIndex(DataHeader);

            std::function<bool(const BaseType&, const BaseType&)> Comparator = Pred;
            if constexpr (std::same_as<Func, std::less<>> && std::same_as<BaseType, std::string>)
            {
                Comparator = &TCommaSeparatedValues::StrLessThan;
            }

            if (!bSorted)
            {
                std::ranges::sort(Data_, [&](const FRowArray& Lhs, const FRowArray& Rhs) -> bool
                {
                    return Comparator(Lhs[DataIndex], Rhs[DataIndex]);
                });
            }

            auto it = std::ranges::lower_bound(Data_, TargetValue, Comparator, [&](const FRowArray& Row) -> const BaseType&
            {
                return Row[DataIndex];
            });

            if (it == Data_.end())
            {
                throw std::out_of_range("Target value is out of range of the data.");
            }

            typename std::vector<FRowArray>::iterator LowerRow;
            typename std::vector<FRowArray>::iterator UpperRow;

            if ((*it)[DataIndex] == TargetValue)
            {
                LowerRow = UpperRow = it;
            }
            else
            {
                LowerRow = it == Data_.begin() ? it : it - 1;
                UpperRow = it;
            }

            return { *LowerRow, *UpperRow };
        }

        std::vector<FRowArray>* Data()
        {
            return &Data_;
        }

        const std::vector<FRowArray>* Data() const
        {
            return &Data_;
        }

    private:
        void InitializeHeaderMap()
        {
            for (std::size_t i = 0; i < ColNames_.size(); ++i)
            {
                HeaderMap_[ColNames_[i]] = i;
            }
        }

        std::size_t GetHeaderIndex(const std::string& Header) const
        {
            auto it = HeaderMap_.find(Header);
            if (it != HeaderMap_.end())
            {
                return it->second;
            }

            throw std::out_of_range("Header not found.");
        }

        template <typename ReaderType>
        requires std::is_class_v<ReaderType>
        void ReadHeader(ReaderType& Reader, io::ignore_column IgnoreColumn)
        {
            std::apply([&](auto&&... Args) -> void
            {
                Reader.read_header(IgnoreColumn, std::forward<decltype(Args)>(Args)...);
            }, VectorToTuple(ColNames_));
        }

        void ReadData(io::ignore_column IgnoreColumn)
        {
            Data_.reserve(std::min<size_t>(1000, std::filesystem::file_size(Filename_) / (ColSize * sizeof(BaseType))));
            io::CSVReader<ColSize> Reader(Filename_);
            ReadHeader(Reader, IgnoreColumn);
            FRowArray Row(ColNames_.size());
            while (ReadRow(Reader, Row))
            {
                Data_.push_back(Row);
            }
        }

        template <typename ReaderType>
        requires std::is_class_v<ReaderType>
        bool ReadRow(ReaderType& Reader, FRowArray& Row)
        {
            return ReadRowImpl(Reader, Row, std::make_index_sequence<ColSize>{});
        }

        template <typename ReaderType, std::size_t... Indices>
        requires std::is_class_v<ReaderType>
        bool ReadRowImpl(ReaderType& Reader, FRowArray& Row, std::index_sequence<Indices...>)
        {
            return Reader.read_row(Row[Indices]...);
        }

        template <typename ElemType>
        auto VectorToTuple(const std::vector<ElemType>& Vector)
        {
            return VectorToTupleImpl(Vector, std::make_index_sequence<ColSize>{});
        }

        template <typename ElemType, std::size_t... Indices>
        auto VectorToTupleImpl(const std::vector<ElemType>& Vector, std::index_sequence<Indices...>)
        {
            return std::make_tuple(Vector[Indices]...);
        }

        static bool StrLessThan(const std::string& Str1, const std::string& Str2)
        {
            double StrValue1 = 0.0;
            double StrValue2 = 0.0;

            std::from_chars(Str1.data(), Str1.data() + Str1.size(), StrValue1);
            std::from_chars(Str2.data(), Str2.data() + Str2.size(), StrValue2);

            return StrValue1 < StrValue2;
        }

    private:
        std::unordered_map<std::string, std::size_t> HeaderMap_;
        std::string                                  Filename_;
        std::vector<std::string>                     ColNames_;
        std::vector<FRowArray>                       Data_;
    };
} // namespace Npgs
