/**
 Copyright (c) 2025 Stappler Team <admin@stappler.org>

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
 **/

#include "XLCoreDescriptorInfo.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::core {

DescriptorBinding::~DescriptorBinding() { data.clear(); }

DescriptorBinding::DescriptorBinding(DescriptorType type, uint32_t count) : type(type) {
	data.resize(count, DescriptorData{core::ObjectHandle::zero(), nullptr});
}

Rc<Ref> DescriptorBinding::write(uint32_t idx, DescriptorBufferInfo &&info) {
	auto ret = move(data[idx].data);
	data[idx] = DescriptorData{info.buffer->getObjectData().handle, move(info.buffer)};
	return ret;
}

Rc<Ref> DescriptorBinding::write(uint32_t idx, DescriptorImageInfo &&info) {
	auto ret = move(data[idx].data);
	data[idx] = DescriptorData{info.imageView->getObjectData().handle, move(info.imageView)};
	return ret;
}

Rc<Ref> DescriptorBinding::write(uint32_t idx, DescriptorBufferViewInfo &&info) {
	auto ret = move(data[idx].data);
	data[idx] = DescriptorData{info.buffer->getObjectData().handle, move(info.buffer)};
	return ret;
}

const DescriptorData &DescriptorBinding::get(uint32_t idx) const { return data[idx]; }

} // namespace stappler::xenolith::core
