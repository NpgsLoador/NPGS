#pragma once

#include <cstddef>
#include <algorithm>
#include <charconv>
#include <concepts>
#include <functional>
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
        TCommaSeparatedValues(const std::string& Filename, const std::vector<std::string>& ColNames)
            : _Filename(Filename), _ColNames(ColNames)
        {
            InitializeHeaderMap();
            ReadData(io::ignore_extra_column);
        }

        TCommaSeparatedValues(const TCommaSeparatedValues&)     = default;
        TCommaSeparatedValues(TCommaSeparatedValues&&) noexcept = default;
        ~TCommaSeparatedValues()                                = default;

        TCommaSeparatedValues& operator=(const TCommaSeparatedValues&)     = default;
        TCommaSeparatedValues& operator=(TCommaSeparatedValues&&) noexcept = default;

        FRowArray FindFirstDataArray(const std::string& DataHeader, const BaseType& DataValue) const
        {
            std::size_t DataIndex = GetHeaderIndex(DataHeader);
            for (const auto& Row : _Data)
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
            for (const auto& Row : _Data)
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
            if constexpr (std::is_same_v<Func, std::less<>> && std::is_same_v<BaseType, std::string>)
            {
                Comparator = &TCommaSeparatedValues::StrLessThan;
            }

            if (!bSorted)
            {
                std::sort(_Data.begin(), _Data.end(), [&](const FRowArray& Lhs, const FRowArray& Rhs) -> bool
                {
                    return Comparator(Lhs[DataIndex], Rhs[DataIndex]);
                });
            }

            auto it = std::lower_bound(_Data.begin(), _Data.end(), TargetValue,
            [&](const FRowArray& Row, const BaseType& Value) -> bool
            {
                return Comparator(Row[DataIndex], Value);
            });

            if (it == _Data.end())
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
                LowerRow = it == _Data.begin() ? it : it - 1;
                UpperRow = it;
            }

            return { *LowerRow, *UpperRow };
        }

        std::vector<FRowArray>* Data()
        {
            return &_Data;
        }

        const std::vector<FRowArray>* Data() const
        {
            return &_Data;
        }

    private:
        void InitializeHeaderMap()
        {
            for (std::size_t i = 0; i < _ColNames.size(); ++i)
            {
                _HeaderMap[_ColNames[i]] = i;
            }
        }

        std::size_t GetHeaderIndex(const std::string& Header) const
        {
            auto it = _HeaderMap.find(Header);
            if (it != _HeaderMap.end())
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
            }, VectorToTuple(_ColNames));
        }

        void ReadData(io::ignore_column IgnoreColumn)
        {
            _Data.reserve(std::min<size_t>(1000, std::filesystem::file_size(_Filename) / (ColSize * sizeof(BaseType))));
            io::CSVReader<ColSize> Reader(_Filename);
            ReadHeader(Reader, IgnoreColumn);
            FRowArray Row(_ColNames.size());
            while (ReadRow(Reader, Row))
            {
                _Data.push_back(Row);
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
        std::unordered_map<std::string, std::size_t> _HeaderMap;
        std::string                                  _Filename;
        std::vector<std::string>                     _ColNames;
        std::vector<FRowArray>                       _Data;
    };
} // namespace Npgs
