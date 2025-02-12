/**
 Copyright (c) 2023 Stappler LLC <admin@stappler.dev>

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

#include "XLCoreDevice.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::core {

Device::Device() { }

Device::~Device() {
	invalidateObjects();
}

bool Device::init(const Instance *instance) {
	_glInstance = instance;
	_samplersInfo.emplace_back(SamplerInfo{ .magFilter = Filter::Nearest, .minFilter = Filter::Nearest,
		.addressModeU = SamplerAddressMode::Repeat, .addressModeV = SamplerAddressMode::Repeat, .addressModeW = SamplerAddressMode::Repeat});
	_samplersInfo.emplace_back(SamplerInfo{ .magFilter = Filter::Linear, .minFilter = Filter::Linear,
		.addressModeU = SamplerAddressMode::Repeat, .addressModeV = SamplerAddressMode::Repeat, .addressModeW = SamplerAddressMode::Repeat});
	_samplersInfo.emplace_back(SamplerInfo{ .magFilter = Filter::Linear, .minFilter = Filter::Linear,
		.addressModeU = SamplerAddressMode::ClampToEdge, .addressModeV = SamplerAddressMode::ClampToEdge, .addressModeW = SamplerAddressMode::ClampToEdge});
	return true;
}

void Device::end() {
	_started = false;

#if SP_REF_DEBUG
	if (isRetainTrackerEnabled()) {
		log::debug("Gl-Device", "Backtrace for ", (void *)this);
		foreachBacktrace([] (uint64_t id, Time time, const std::vector<std::string> &vec) {
			StringStream stream;
			stream << "[" << id << ":" << time.toHttp<Interface>() << "]:\n";
			for (auto &it : vec) {
				stream << "\t" << it << "\n";
			}
			log::debug("Gl-Device-Backtrace", stream.str());
		});
	}
#endif
}

Rc<Shader> Device::getProgram(StringView name) {
	std::unique_lock<Mutex> lock(_shaderMutex);
	auto it = _shaders.find(name);
	if (it != _shaders.end()) {
		return it->second;
	}
	return nullptr;
}

Rc<Shader> Device::addProgram(Rc<Shader> program) {
	std::unique_lock<Mutex> lock(_shaderMutex);
	auto it = _shaders.find(program->getName());
	if (it == _shaders.end()) {
		_shaders.emplace(program->getName().str<Interface>(), program);
		return program;
	} else {
		return it->second;
	}
}

Rc<Framebuffer> Device::makeFramebuffer(const QueuePassData *, SpanView<Rc<ImageView>>) {
	return nullptr;
}

auto Device::makeImage(const ImageInfoData &) -> Rc<ImageStorage> {
	return nullptr;
}

Rc<Semaphore> Device::makeSemaphore() {
	return nullptr;
}

Rc<ImageView> Device::makeImageView(const Rc<ImageObject> &, const ImageViewInfo &) {
	return nullptr;
}

void Device::addObject(Object *obj) {
	std::unique_lock<Mutex> lock(_objectMutex);
	_objects.emplace(obj);
}

void Device::removeObject(Object *obj) {
	std::unique_lock<Mutex> lock(_objectMutex);
	_objects.erase(obj);
}

void Device::onLoopStarted(Loop &loop) {

}

void Device::onLoopEnded(Loop &) {

}

bool Device::supportsUpdateAfterBind(DescriptorType) const {
	return false;
}

void Device::clearShaders() {
	_shaders.clear();
}

void Device::invalidateObjects() {
	Vector<ObjectData> data;

	std::unique_lock<Mutex> lock(_objectMutex);
	for (auto &it : _objects) {
		if (auto img = dynamic_cast<ImageObject *>(it)) {
			log::warn("Gl-Device", "Image ", (void *)it, " \"", img->getName(), "\" ((", typeid(*it).name(),
					") [rc:", it->getReferenceCount(), "] was not destroyed before device destruction");
		} else if (auto pass = dynamic_cast<RenderPass *>(it)) {
			log::warn("Gl-Device", "RenderPass ", (void *)it, " \"", pass->getName(), "\" (", typeid(*it).name(),
					") [rc:", it->getReferenceCount(), "] was not destroyed before device destruction");
		} else if (auto obj = dynamic_cast<BufferObject *>(it)) {
			log::warn("Gl-Device", "Buffer ", (void *)it, " \"", obj->getName(), "\" ((", typeid(*it).name(),
					") [rc:", it->getReferenceCount(), "] was not destroyed before device destruction");
		} else {
			auto name = it->getName();
			if (!name.empty()) {
				log::warn("Gl-Device", "Object ", (void *)it, " \"", name, "\" ((", typeid(*it).name(),
						") [rc:", it->getReferenceCount(), "] was not destroyed before device destruction");
			} else {
				log::warn("Gl-Device", "Object ", (void *)it, " (", typeid(*it).name(),
						") [rc:", it->getReferenceCount(), "] was not destroyed before device destruction");
			}
		}
#if SP_REF_DEBUG
		log::warn("Gl-Device", "Backtrace for ", (void *)it);
		it->foreachBacktrace([] (uint64_t id, Time time, const std::vector<std::string> &vec) {
			StringStream stream;
			stream << "[" << id << ":" << time.toHttp<Interface>() << "]:\n";
			for (auto &it : vec) {
				stream << "\t" << it << "\n";
			}
			log::warn("Gl-Device-Backtrace", stream.str());
		});
#endif

		auto d = (ObjectData *)&it->getObjectData();

		data.emplace_back(*d);

		d->callback = nullptr;
	}
	_objects.clear();
	lock.unlock();

	for (auto &_object : data) {
		if (_object.callback) {
			_object.callback(_object.device, _object.type, _object.handle, _object.ptr);
		}
	}

	data.clear();
}

}
