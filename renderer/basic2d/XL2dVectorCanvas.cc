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

#include "XL2dVectorCanvas.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::basic2d {

struct VectorCanvasPathOutput {
	Color4F color;
	VertexData *vertexes = nullptr;
	uint32_t material = 0;
	uint32_t objects = 0;
};

struct VectorCanvasPathDrawer {
	const VectorPath *path = nullptr;

	bool verbose = false;
	float quality = 0.5f; // approximation level (more is better)
	Color4F originalColor;
	geom::Tesselator::RelocateRule relocateRule = geom::Tesselator::RelocateRule::Auto;

	uint32_t draw(memory::pool_t *pool, const VectorPath &p, const Mat4 &transform, VertexData *, bool cache);
};

struct VectorCanvasCacheData {
	Rc<VertexData> data;
	String name;
	float quality = 1.0f;
	float scale = 1.0f;
	vg::DrawStyle style = vg::DrawStyle::Fill;

	bool operator< (const VectorCanvasCacheData &other) const {
		if (style != other.style) {
			return toInt(style) < toInt(other.style);
		} else if (name != other.name) {
			return name < other.name;
		} else if (quality != other.quality) {
			return quality < other.quality;
		} else {
			return scale < other.scale;
		}
	}
};

struct VectorCanvasCache {
	static Mutex s_cacheMutex;
	static VectorCanvasCache *s_instance;

	static void retain();
	static void release();

	static const VectorCanvasCacheData *getCacheData(const VectorCanvasCacheData &);
	static const VectorCanvasCacheData *setCacheData(VectorCanvasCacheData &&);

	VectorCanvasCache();
	~VectorCanvasCache();

	uint32_t refCount = 0;
	Set<VectorCanvasCacheData> cacheData;
};

VectorCanvasCache *VectorCanvasCache::s_instance = nullptr;
Mutex VectorCanvasCache::s_cacheMutex;

struct VectorCanvas::Data : memory::AllocPool {
	memory::pool_t *pool = nullptr;
	memory::pool_t *transactionPool = nullptr;
	bool isOwned = true;
	bool deferred = false;

	VectorCanvasPathDrawer pathDrawer;

	Mat4 transform = Mat4::IDENTITY;
	Vector<Mat4> states;

	TimeInterval subAccum;

	Rc<VectorImageData> image;
	Size2 targetSize;

	Vector<TransformVertexData> *out = nullptr;

	Data(memory::pool_t *p, bool deferred);
	~Data();

	void save();
	void restore();

	void applyTransform(const Mat4 &t);

	void draw(const VectorPath &, StringView cache);
	void draw(const VectorPath &, StringView cache, const Mat4 &);

	void doDraw(const VectorPath &, StringView cache);

	void writeCacheData(const VectorPath &p, VertexData *out, const VertexData &source);
};

static void VectorCanvasPathDrawer_pushVertex(void *ptr, uint32_t idx, const Vec2 &pt, float vertexValue) {
	auto out = reinterpret_cast<VectorCanvasPathOutput *>(ptr);
	if (size_t(idx) >= out->vertexes->data.size()) {
		out->vertexes->data.resize(idx + 1);
	}

	out->vertexes->data[idx] = Vertex{
		Vec4(pt, 0.0f, 1.0f),
		Vec4(out->color.r, out->color.g, out->color.b, out->color.a * vertexValue),
		Vec2(0.0f, 0.0f), out->material, 0
	};
}

static void VectorCanvasPathDrawer_pushTriangle(void *ptr, uint32_t pt[3]) {
	auto out = reinterpret_cast<VectorCanvasPathOutput *>(ptr);
	out->vertexes->indexes.emplace_back(pt[0]);
	out->vertexes->indexes.emplace_back(pt[1]);
	out->vertexes->indexes.emplace_back(pt[2]);
	++ out->objects;
}

Rc<VectorCanvas> VectorCanvas::getInstance(bool deferred) {
	static thread_local Rc<VectorCanvas> tl_instance = nullptr;
	if (!tl_instance) {
		tl_instance = Rc<VectorCanvas>::create(deferred);
	}
	return tl_instance;
}

VectorCanvas::~VectorCanvas() {
	if (_data && _data->isOwned) {
		auto p = _data->pool;
		_data->~Data();
		_data = nullptr;
		memory::pool::destroy(p);
	}
}

bool VectorCanvas::init(bool deferred, float quality, Color4F color) {
	auto p = memory::pool::createTagged("xenolith::VectorCanvas");
	memory::pool::context ctx(p);
	_data = new (p) Data(p, deferred);
	if (_data) {
		_data->pathDrawer.quality = quality;
		_data->pathDrawer.originalColor = color;
		return true;
	}
	return false;
}

void VectorCanvas::setColor(Color4F color) {
	_data->pathDrawer.originalColor = color;
}

Color4F VectorCanvas::getColor() const {
	return _data->pathDrawer.originalColor;
}

void VectorCanvas::setQuality(float value) {
	_data->pathDrawer.quality = value;
}

float VectorCanvas::getQuality() const {
	return _data->pathDrawer.quality;
}

void VectorCanvas::setRelocateRule(geom::Tesselator::RelocateRule rule) {
	_data->pathDrawer.relocateRule = rule;
}

geom::Tesselator::RelocateRule VectorCanvas::getRelocateRule() const {
	return _data->pathDrawer.relocateRule;
}

void VectorCanvas::setVerbose(bool val) {
	_data->pathDrawer.verbose = val;
}

Rc<VectorCanvasResult> VectorCanvas::draw(Rc<VectorImageData> &&image, Size2 targetSize) {
	auto ret = Rc<VectorCanvasResult>::alloc();
	_data->out = &ret->data;
	_data->image = move(image);
	ret->targetSize = _data->targetSize = targetSize;
	ret->targetColor = getColor();

	auto imageSize = _data->image->getImageSize();

	Mat4 t = Mat4::IDENTITY;
	t.scale(targetSize.width / imageSize.width, targetSize.height / imageSize.height, 1.0f);

	ret->targetTransform = t;

	auto &m = _data->image->getViewBoxTransform();
	if (!m.isIdentity()) {
		t *= m;
	}

	bool isIdentity = t.isIdentity();

	if (!isIdentity) {
		_data->save();
		_data->applyTransform(t);
	}

	_data->image->draw([&, this] (const VectorPath &path, StringView cacheId, const Mat4 &pos) {
		if (pos.isIdentity()) {
			_data->draw(path, cacheId);
		} else {
			_data->draw(path, cacheId, pos);
		}
	});

	if (!isIdentity) {
		_data->restore();
	}

	if (!_data->out->empty() && _data->out->back().data->data.empty()) {
		_data->out->pop_back();
	}

	_data->out = nullptr;
	_data->image = nullptr;
	ret->updateColor(ret->targetColor);
	return ret;
}

VectorCanvas::Data::Data(memory::pool_t *p, bool deferred) : pool(p), deferred(deferred) {
	transactionPool = memory::pool::create(pool);

	VectorCanvasCache::retain();
}

VectorCanvas::Data::~Data() {
	VectorCanvasCache::release();

	memory::pool::destroy(transactionPool);
}

void VectorCanvas::Data::save() {
	states.push_back(transform);
}

void VectorCanvas::Data::restore() {
	if (!states.empty()) {
		transform = states.back();
		states.pop_back();
	}
}

void VectorCanvas::Data::applyTransform(const Mat4 &t) {
	transform *= t;
}

void VectorCanvas::Data::draw(const VectorPath &path, StringView cache) {
	bool hasTransform = !path.getTransform().isIdentity();
	if (hasTransform) {
		save();
		applyTransform(path.getTransform());
	}

	doDraw(path, cache);

	if (hasTransform) {
		restore();
	}
}

void VectorCanvas::Data::draw(const VectorPath &path, StringView cache, const Mat4 &mat) {
	auto matTransform = path.getTransform() * mat;
	bool hasTransform = !matTransform.isIdentity();

	if (hasTransform) {
		save();
		applyTransform(matTransform);
	}

	doDraw(path, cache);

	if (hasTransform) {
		restore();
	}
}

void VectorCanvas::Data::doDraw(const VectorPath &path, StringView cache) {
	VertexData *outData = nullptr;
	if (out->empty() || !out->back().data->data.empty()) {
		out->emplace_back(TransformVertexData{transform, Rc<VertexData>::alloc()});
	}

	outData = out->back().data.get();
	memory::pool::push(transactionPool);

	do {
		if (!deferred && !cache.empty()) {
			auto style = path.getStyle();
			float quality = pathDrawer.quality;

			Vec3 scaleVec; transform.getScale(&scaleVec);
			float scale = std::max(scaleVec.x, scaleVec.y);

			VectorCanvasCacheData data{nullptr, cache.str<Interface>(), quality, scale, style};

			if (auto it = VectorCanvasCache::getCacheData(data)) {
				if (!it->data->indexes.empty()) {
					writeCacheData(path, outData, *it->data);
				}
				break;
			}

			data.data = Rc<VertexData>::alloc();

			auto ret = pathDrawer.draw(transactionPool, path, transform, data.data, true);
			if (ret != 0) {
				if (auto it = VectorCanvasCache::setCacheData(move(data))) {
					writeCacheData(path, outData, *it->data);
				}
			} else {
				outData->data.clear();
				outData->indexes.clear();
				out->back().transform = transform;
			}
		} else {
			auto ret = pathDrawer.draw(transactionPool, path, transform, outData, false);
			if (ret == 0) {
				outData->data.clear();
				outData->indexes.clear();
				out->back().transform = transform;
			}
		}
	} while (0);

	memory::pool::pop();
	memory::pool::clear(transactionPool);
}

void VectorCanvas::Data::writeCacheData(const VectorPath &p, VertexData *out, const VertexData &source) {
	auto fillColor = Color4F(p.getFillColor());
	auto strokeColor = Color4F(p.getStrokeColor());

	Vec4 fillVec = fillColor;
	Vec4 strokeVec = strokeColor;

	out->indexes = source.indexes;
	out->data = source.data;
	for (auto &it : out->data) {
		if (it.material == 0) {
			it.color = it.color * fillVec;
		} else if (it.material == 1) {
			it.color = it.color * strokeVec;
		}
	}
}

uint32_t VectorCanvasPathDrawer::draw(memory::pool_t *pool, const VectorPath &p, const Mat4 &transform,
		VertexData *out, bool cache) {
	bool success = true;
	path = &p;

	float approxScale = 1.0f;
	auto style = path->getStyle();

	Rc<geom::Tesselator> strokeTess = ((style & geom::DrawStyle::Stroke) != geom::DrawStyle::None) ? Rc<geom::Tesselator>::create(pool) : nullptr;
	Rc<geom::Tesselator> fillTess = ((style & geom::DrawStyle::Fill) != geom::DrawStyle::None) ? Rc<geom::Tesselator>::create(pool) : nullptr;

	Vec3 scale; transform.getScale(&scale);
	approxScale = std::max(scale.x, scale.y);

	geom::LineDrawer line(approxScale * quality, Rc<geom::Tesselator>(fillTess), Rc<geom::Tesselator>(strokeTess),
			path->getStrokeWidth());

	auto d = path->getPoints().data();
	for (auto &it : path->getCommands()) {
		switch (it) {
		case vg::Command::MoveTo: line.drawBegin(d[0].p.x, d[0].p.y); ++ d; break;
		case vg::Command::LineTo: line.drawLine(d[0].p.x, d[0].p.y); ++ d; break;
		case vg::Command::QuadTo: line.drawQuadBezier(d[0].p.x, d[0].p.y, d[1].p.x, d[1].p.y); d += 2; break;
		case vg::Command::CubicTo: line.drawCubicBezier(d[0].p.x, d[0].p.y, d[1].p.x, d[1].p.y, d[2].p.x, d[2].p.y); d += 3; break;
		case vg::Command::ArcTo: line.drawArc(d[0].p.x, d[0].p.y, d[2].f.v, d[2].f.a, d[2].f.b, d[1].p.x, d[1].p.y); d += 3; break;
		case vg::Command::ClosePath: line.drawClose(true); break;
		default: break;
		}
	}

	line.drawClose(false);

	VectorCanvasPathOutput target { Color4F::WHITE, out };
	geom::TessResult result;
	result.target = &target;
	result.pushVertex = VectorCanvasPathDrawer_pushVertex;
	result.pushTriangle = VectorCanvasPathDrawer_pushTriangle;

	if (fillTess) {
		// draw antialias outline only if stroke is transparent enough
		// for cached image, always draw antialias, because user can change color and opacity
		if (path->isAntialiased() && (path->getStyle() == vg::DrawStyle::Fill || path->getStrokeOpacity() < 96 || cache)) {
			fillTess->setAntialiasValue(config::VGAntialiasFactor / approxScale);
			fillTess->setRelocateRule(relocateRule);
		}
		fillTess->setWindingRule(path->getWindingRule());
		if (!fillTess->prepare(result)) {
			success = false;
		}
	}

	if (strokeTess) {
		if (path->isAntialiased()) {
			strokeTess->setAntialiasValue(config::VGAntialiasFactor / approxScale);
		}

		strokeTess->setWindingRule(vg::Winding::NonZero);
		if (!strokeTess->prepare(result)) {
			success = false;
		}
	}

	out->data.resize(result.nvertexes);
	out->indexes.reserve(result.nfaces * 3);

	if (fillTess) {
		target.material = 0;
		if (cache) {
			target.color = Color4F::WHITE;
		} else {
			target.color = Color4F(path->getFillColor());
		}
		fillTess->write(result);
	}

	if (strokeTess) {
		target.material = 1;
		if (cache) {
			target.color = Color4F::WHITE;
		} else {
			target.color = Color4F(path->getStrokeColor());
		}
		strokeTess->write(result);
	}

	if (!success) {
		if (verbose) {
			log::error("VectorCanvasPathDrawer", "Failed path:\n", path->toString(true));
		}
	}

	return target.objects;
}


void VectorCanvasCache::retain() {
	std::unique_lock<Mutex> lock(s_cacheMutex);

	if (!s_instance) {
		s_instance = new VectorCanvasCache();
	}
	++ s_instance->refCount;
}

void VectorCanvasCache::release() {
	std::unique_lock<Mutex> lock(s_cacheMutex);

	if (s_instance) {
		if (s_instance->refCount == 1) {
			delete s_instance;
			s_instance = nullptr;
		} else {
			-- s_instance->refCount;
		}
	}
}

const VectorCanvasCacheData *VectorCanvasCache::getCacheData(const VectorCanvasCacheData &data) {
	std::unique_lock<Mutex> lock(s_cacheMutex);
	if (!s_instance) {
		return nullptr;
	}

	auto it = s_instance->cacheData.find(data);
	if (it != s_instance->cacheData.end()) {
		return &*it;
	}

	return nullptr;
}

const VectorCanvasCacheData *VectorCanvasCache::setCacheData(VectorCanvasCacheData &&data) {
	std::unique_lock<Mutex> lock(s_cacheMutex);
	if (!s_instance) {
		return nullptr;
	}

	auto it = s_instance->cacheData.find(data);
	if (it == s_instance->cacheData.end()) {
		it = s_instance->cacheData.emplace(move(data)).first;
	}
	return &*it;
}

VectorCanvasCache::VectorCanvasCache() {
	auto path = filesystem::writablePath<Interface>("vector_cache.cbor");

	if (filesystem::exists(path)) {
		auto val = data::readFile<Interface>(path);
		for (auto &it : val.asArray()) {
			VectorCanvasCacheData data;
			data.name = it.getString("name");
			data.quality = it.getDouble("quality");
			data.scale = it.getDouble("scale");

			auto &vertexes = it.getBytes("vertexes");
			auto &indexes = it.getBytes("indexes");

			data.data = Rc<VertexData>::alloc();
			data.data->data.assign(reinterpret_cast<Vertex *>(vertexes.data()),
					reinterpret_cast<Vertex *>(vertexes.data() + vertexes.size()));
			data.data->indexes.assign(reinterpret_cast<uint32_t *>(indexes.data()),
					reinterpret_cast<uint32_t *>(indexes.data() + indexes.size()));

			cacheData.emplace(move(data));
		}
	}
}

VectorCanvasCache::~VectorCanvasCache() {
	Value val;
	for (auto &it : cacheData) {
		if (!it.data) {
			continue;
		}

		Value data;
		data.setString(it.name, "name");
		data.setDouble(it.quality, "quality");
		data.setDouble(it.scale, "scale");

		data.setBytes(BytesView(reinterpret_cast<uint8_t *>(it.data->data.data()), it.data->data.size() * sizeof(Vertex)), "vertexes");
		data.setBytes(BytesView(reinterpret_cast<uint8_t *>(it.data->indexes.data()), it.data->indexes.size() * sizeof(uint32_t)), "indexes");

		val.addValue(move(data));
	}

	if (!val.empty()) {
		auto path = filesystem::writablePath<Interface>("vector_cache.cbor");
		filesystem::mkdir(filepath::root(path));

		filesystem::remove(path);
		data::save(val, path, data::EncodeFormat::Cbor);
	}
}

void VectorCanvasResult::updateColor(const Color4F &color) {
	auto copyData = [] (const VertexData *data) {
		auto ret = Rc<VertexData>::alloc();
		ret->data.resize(data->data.size());
		ret->indexes.resize(data->indexes.size());
		memcpy(ret->data.data(), data->data.data(), data->data.size() * sizeof(Vertex));
		memcpy(ret->indexes.data(), data->indexes.data(), data->indexes.size() * sizeof(uint32_t));
		return ret;
	};

	mut.clear();
	mut.reserve(data.size());
	for (auto &it : data) {
		auto &iit = mut.emplace_back(TransformVertexData{it.transform, copyData(it.data)});
		for (auto &vertex : iit.data->data) {
			vertex.color = vertex.color * color;
		}
	}

	targetColor = color;
}

VectorCanvasDeferredResult::~VectorCanvasDeferredResult() {
	if (_future) {
		delete _future;
		_future = nullptr;
	}
}

bool VectorCanvasDeferredResult::init(std::future<Rc<VectorCanvasResult>> &&future, bool waitOnReady) {
	_future = new std::future<Rc<VectorCanvasResult>>(move(future));
	_waitOnReady = waitOnReady;
	return true;
}

SpanView<TransformVertexData> VectorCanvasDeferredResult::getData() {
	std::unique_lock<Mutex> lock(_mutex);
	if (_future) {
		_result = _future->get();
		delete _future;
		_future = nullptr;
		DeferredVertexResult::handleReady();
	}
	return _result->mut;
}

void VectorCanvasDeferredResult::handleReady(Rc<VectorCanvasResult> &&res) {
	std::unique_lock<Mutex> lock(_mutex);
	if (_future) {
		delete _future;
		_future = nullptr;
	}
	_result = move(res);
	DeferredVertexResult::handleReady();
}

Rc<VectorCanvasResult> VectorCanvasDeferredResult::getResult() const {
	std::unique_lock<Mutex> lock(_mutex);
	return _result;
}

void VectorCanvasDeferredResult::updateColor(const Color4F &color) {
	getResult(); // ensure rendering was complete

	std::unique_lock<Mutex> lock(_mutex);
	if (_result) {
		lock.unlock();
		_result->updateColor(color);
	}
}

}
