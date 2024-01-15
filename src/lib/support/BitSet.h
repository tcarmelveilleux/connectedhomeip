/*
 *
 *    Copyright (c) 2024 Project CHIP Authors
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace chip {

/**
 * Compact set recording presence of up to N elements in range [0 .. size() - 1]
 */
class BitSet
{
public:
    virtual size_t size() const = 0;
    virtual bool empty() const = 0;
    virtual bool Has(size_t element_index) const = 0;
    virtual bool HasEveryElement() const = 0;

    virtual bool Set(size_t element_index) = 0;
    virtual bool Clear(size_t element_index) = 0;
    virtual void ClearAll() = 0;
};

/**
 * @brief Version of a compact set that statically stores its data as a linear array of bits.
 *
 * @tparam N - number of elements in the set.
 */
template <size_t N>
class CompactArrayBitSet : public BitSet
{
public:
    CompactArrayBitSet() : m_size(N) {
        memset(&m_data[0], 0, GetNumBytes());
    }

    size_t size() const override
    {
        return m_size;
    }

    bool empty() const override
    {
        for (size_t idx = 0; idx < GetNumBytes(); ++idx)
        {
            if (m_data[idx] != 0)
            {
              return false;
            }
        }

        return true;
    }

    bool Has(size_t element_index) const override
    {
        if (element_index >= m_size)
        {
            return false;
        }

        size_t byte_pos = 0;
        uint8_t mask = 0;
        GetBytePosAndMask(element_index, byte_pos, mask);
        return (m_data[byte_pos] & mask) != 0;
    }

    bool HasEveryElement() const override
    {
        for (size_t idx = 0; idx < m_size; ++idx)
        {
            if (!Has(idx))
            {
                return false
            }
        }

        return true;
    }

    bool Set(size_t element_index) override
    {
        if (element_index >= m_size)
        {
            return false;
        }
        size_t byte_pos = 0;
        uint8_t mask = 0;
        GetBytePosAndMask(element_index, byte_pos, mask);
        m_data[byte_pos] |= mask;

        return true;
    }

    bool Clear(size_t element_index) override
    {
        if (element_index >= m_size)
        {
            return false;
        }
        size_t byte_pos = 0;
        uint8_t mask = 0;
        GetBytePosAndMask(element_index, byte_pos, mask);

        m_data[byte_pos] &= ~mask;

        return true;
    }

    void ClearAll() override
    {
        memset(&m_data[0], 0, GetNumBytes());
    }

private:
    size_t GetNumBytes() {
        return (m_size + 7u) / 8u;
    }

    // Always called with element_index < N.
    void GetBytePosAndMask(size_t element_index, size_t & out_byte_pos, uint8_t & out_mask)
    {
        out_byte_pos = element_index / 8u;
        out_mask = static_cast<uint8_t>(1u << (element_index % 8u));
    }

    size_t m_size;
    uint8_t m_data[(N + 7u) / 8u];
};

} // namespace chip
