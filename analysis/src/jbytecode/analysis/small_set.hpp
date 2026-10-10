#pragma once

#include <vector>
#include <cstddef>

namespace jbytecode::analysis {

template<typename T>
class SmallSet
{
public:
    SmallSet() = default;

    explicit SmallSet(const T& value) { elements_.push_back(value); }

    SmallSet(const T& value1, const T& value2)
    {
        if (value1 == value2) {
            elements_.push_back(value1);
        }
        else {
            elements_.push_back(value1);
            elements_.push_back(value2);
        }
    }

    T getSingleValue() const
    {
        if (elements_.size() == 1) return elements_[0];
        return T{};
    }

    size_t size() const { return elements_.size(); }
    bool   empty() const { return elements_.empty(); }

    const T& operator[](size_t i) const { return elements_[i]; }

    SmallSet union_(const SmallSet& other) const
    {
        if (other.elements_.empty()) return *this;
        if (elements_.empty()) return other;

        SmallSet result;
        for (const auto& v : elements_) {
            if (!result.contains(v)) result.elements_.push_back(v);
        }
        for (const auto& v : other.elements_) {
            if (!result.contains(v)) result.elements_.push_back(v);
        }
        return result;
    }

    bool contains(const T& value) const
    {
        for (const auto& v : elements_)
            if (v == value) return true;
        return false;
    }

    bool operator==(const SmallSet& other) const
    {
        if (elements_.size() != other.elements_.size()) return false;
        for (const auto& v : elements_)
            if (!other.contains(v)) return false;
        return true;
    }

    bool operator!=(const SmallSet& other) const { return !(*this == other); }

private:
    std::vector<T> elements_;
};

}   // namespace jbytecode::analysis
