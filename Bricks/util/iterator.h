/*******************************************************************************
 The MIT License (MIT)
 
 Copyright (c) 2016 Grigory Nikolaenko <nikolaenko.grigory@gmail.com>
 
 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:
 
 The above copyright notice and this permission notice shall be included in all
 copies or substantial portions of the Software.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 SOFTWARE.
 *******************************************************************************/

#ifndef BRICKS_UTIL_ITERATOR_H
#define BRICKS_UTIL_ITERATOR_H

#include "../template/pod.h"
#include "../template/is_unique_ptr.h"

namespace current {

template <typename MAP>
struct GenericMapIterator final {
	using iterator_t = typename MAP::const_iterator;
	using key_t = typename MAP::key_type;
	using mapped_t = typename std::remove_pointer<typename MAP::mapped_type>::type;
	using value_t = typename is_unique_ptr<mapped_t>::underlying_type;
	iterator_t iterator_;
	explicit GenericMapIterator(iterator_t iterator) : iterator_(iterator) {}
	void operator++() { ++iterator_; }
	bool operator==(const GenericMapIterator& rhs) const { return iterator_ == rhs.iterator_; }
	bool operator!=(const GenericMapIterator& rhs) const { return !operator==(rhs); }
	copy_free<key_t> key() const { return iterator_->first; }

	const value_t& operator*() const { return is_unique_ptr<mapped_t>::extract(iterator_->second); }
	const value_t* operator->() const { return is_unique_ptr<mapped_t>::pointer(iterator_->second); }
};

template <typename MAP>
struct GenericMapAccessor final {
	using iterator_t = GenericMapIterator<MAP>;
	using key_t = typename MAP::key_type;
	const MAP& map_;
	
	GenericMapAccessor(const MAP& map) : map_(map) {}
	
	bool Empty() const { return map_.empty(); }
	size_t Size() const { return map_.size(); }
	
	bool Has(const key_t& x) const { return map_.find(x) != map_.end(); }
	
	iterator_t begin() const { return iterator_t(map_.cbegin()); }
	iterator_t end() const { return iterator_t(map_.cend()); }
};

}

#endif  // BRICKS_UTIL_ITERATOR_H
