/** \copyright
 * Copyright (c) 2015, Balazs Racz
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are  permitted provided that the following conditions are met:
 *
 *  - Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 *  - Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * \file SortedListMap.hxx
 * Template class for maintaining a mapping by using a sorted list.
 *
 * @author Balazs Racz
 * @date 30 August 2015
 */

#ifndef _UTILS_SORTEDLISTMAP_HXX_
#define _UTILS_SORTEDLISTMAP_HXX_

#include <vector>
#include <algorithm>

/// An mostly std::set<> compatible class that stores the internal data in a
/// sorted vector. Has low memory overhead; isertion cost is pretty high and
/// lookup cost is logarithmic. Useful when a few insertions happen (for
/// example only during initialization) and then lots of lookups are made.
template <class D, class CMP> class SortedListSet
{
public:
    typedef D data_type;

private:
    typedef std::vector<data_type> container_type;

public:
    typedef typename container_type::iterator iterator;
    typedef typename container_type::const_iterator const_iterator;

    SortedListSet()
    {
    }

    iterator begin()
    {
        lazy_init();
        return container_.begin();
    }

    iterator end()
    {
        return container_.begin() + sortedCount_;
    }

    template<class key_type>
    iterator lower_bound(key_type key)
    {
        lazy_init();
        return std::lower_bound(container_.begin(), container_.end(), key,
                                CMP());
    }

    template<class key_type>
    iterator upper_bound(key_type key)
    {
        lazy_init();
        return std::upper_bound(container_.begin(), container_.end(), key,
                                CMP());
    }

    template<class key_type>
    pair<iterator, iterator> equal_range(key_type key)
    {
        lazy_init();
        return std::equal_range(container_.begin(), container_.end(), key,
                                CMP());
    }

    void insert(data_type &&d)
    {
        container_.push_back(d);
    }

    void erase(const iterator &it)
    {
        container_.erase(it);
    }

private:
    /// Reestablishes sorted order in case anything was inserted or removed.
    void lazy_init()
    {
        if (sortedCount_ != container_.size())
        {
            sort(container_.begin(), container_.end(), CMP());
            sortedCount_ = container_.size();
        }
    }

    /// Holds the actual data elements.
    container_type container_;

    /// The first this many elements in the container are already sorted.
    size_t sortedCount_{0};
};

#endif // _UTILS_SORTEDLISTMAP_HXX_
