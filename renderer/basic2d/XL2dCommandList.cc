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

#include "XL2dCommandList.h"

namespace stappler::xenolith::basic2d {

Command *Command::create(memory::pool_t *p, CommandType t, CommandFlags f) {
	auto commandSize = sizeof(Command);

	auto bytes = memory::pool::palloc(p, commandSize);
	auto c = new (bytes) Command;
	c->next = nullptr;
	c->type = t;
	c->flags = f;
	switch (t) {
	case CommandType::CommandGroup:
		c->data = nullptr;
		break;
	case CommandType::VertexArray:
		c->data = new ( memory::pool::palloc(p, sizeof(CmdVertexArray)) ) CmdVertexArray;
		break;
	case CommandType::Deferred:
		c->data = new ( memory::pool::palloc(p, sizeof(CmdDeferred)) ) CmdDeferred;
		break;
	case CommandType::ShadowArray:
		c->data = new ( memory::pool::palloc(p, sizeof(CmdShadowArray)) ) CmdShadowArray;
		break;
	case CommandType::ShadowDeferred:
		c->data = new ( memory::pool::palloc(p, sizeof(CmdShadowDeferred)) ) CmdShadowDeferred;
		break;
	case CommandType::SdfGroup2D:
		c->data = new ( memory::pool::palloc(p, sizeof(CmdSdfGroup2D)) ) CmdSdfGroup2D;
		break;
	}
	return c;
}

void Command::release() {
	switch (type) {
	case CommandType::CommandGroup:
		break;
	case CommandType::VertexArray:
		if (CmdVertexArray *d = reinterpret_cast<CmdVertexArray *>(data)) {
			for (auto &it : d->vertexes) {
				const_cast<TransformVertexData &>(it).data = nullptr;
			}
		}
		break;
	case CommandType::Deferred:
		if (CmdDeferred *d = reinterpret_cast<CmdDeferred *>(data)) {
			d->deferred = nullptr;
		}
		break;
	case CommandType::ShadowArray:
		if (CmdShadowArray *d = reinterpret_cast<CmdShadowArray *>(data)) {
			for (auto &it : d->vertexes) {
				const_cast<TransformVertexData &>(it).data = nullptr;
			}
		}
		break;
	case CommandType::ShadowDeferred:
		if (CmdShadowDeferred *d = reinterpret_cast<CmdShadowDeferred *>(data)) {
			d->deferred = nullptr;
		}
		break;
	case CommandType::SdfGroup2D:
		break;
	}
}

CommandList::~CommandList() {
	if (!_first) {
		return;
	}

	memory::pool::push(_pool->getPool());

	auto cmd = _first;
	do {
		cmd->release();
		cmd = cmd->next;
	} while (cmd);

	memory::pool::pop();
}

bool CommandList::init(const Rc<PoolRef> &pool) {
	_pool = pool;
	return true;
}

void CommandList::pushVertexArray(Rc<VertexData> &&vert, const Mat4 &t, SpanView<ZOrder> zPath,
		core::MaterialId material, RenderingLevel level, float depthValue, CommandFlags flags) {
	_pool->perform([&] {
		auto cmd = Command::create(_pool->getPool(), CommandType::VertexArray, flags);
		auto cmdData = reinterpret_cast<CmdVertexArray *>(cmd->data);

		// pool memory is 16-bytes aligned, no problems with Mat4
		auto p = new (memory::pool::palloc(_pool->getPool(), sizeof(TransformVertexData))) TransformVertexData();

		p->transform = t;
		p->data = move(vert);

		cmdData->vertexes = makeSpanView(p, 1);

		while (!zPath.empty() && zPath.back() == ZOrder(0)) {
			zPath.pop_back();
		}

		cmdData->zPath = zPath.pdup(_pool->getPool());
		cmdData->material = material;
		cmdData->state = _currentState;
		cmdData->renderingLevel = level;
		cmdData->depthValue = depthValue;

		addCommand(cmd);
	});
}

void CommandList::pushVertexArray(SpanView<TransformVertexData> data, SpanView<ZOrder> zPath,
		core::MaterialId material, RenderingLevel level, float depthValue, CommandFlags flags) {
	_pool->perform([&] {
		auto cmd = Command::create(_pool->getPool(), CommandType::VertexArray, flags);
		auto cmdData = reinterpret_cast<CmdVertexArray *>(cmd->data);

		cmdData->vertexes = data;

		while (!zPath.empty() && zPath.back() == ZOrder(0)) {
			zPath.pop_back();
		}

		cmdData->zPath = zPath.pdup(_pool->getPool());
		cmdData->material = material;
		cmdData->state = _currentState;
		cmdData->renderingLevel = level;
		cmdData->depthValue = depthValue;

		addCommand(cmd);
	});
}

void CommandList::pushDeferredVertexResult(const Rc<DeferredVertexResult> &res, const Mat4 &viewT, const Mat4 &modelT, bool normalized,
		SpanView<ZOrder> zPath, core::MaterialId material, RenderingLevel level, float depthValue, CommandFlags flags) {
	_pool->perform([&] {
		auto cmd = Command::create(_pool->getPool(), CommandType::Deferred, flags);
		auto cmdData = reinterpret_cast<CmdDeferred *>(cmd->data);

		cmdData->deferred = res;
		cmdData->viewTransform = viewT;
		cmdData->modelTransform = modelT;
		cmdData->normalized = normalized;

		while (!zPath.empty() && zPath.back() == ZOrder(0)) {
			zPath.pop_back();
		}

		cmdData->zPath = zPath.pdup(_pool->getPool());
		cmdData->material = material;
		cmdData->state = _currentState;
		cmdData->renderingLevel = level;
		cmdData->depthValue = depthValue;

		addCommand(cmd);
	});
}

void CommandList::pushShadowArray(Rc<VertexData> &&vert, const Mat4 &t, float value) {
	_pool->perform([&] {
		auto cmd = Command::create(_pool->getPool(), CommandType::ShadowArray, CommandFlags::None);
		auto cmdData = reinterpret_cast<CmdShadowArray *>(cmd->data);

		// pool memory is 16-bytes aligned, no problems with Mat4
		auto p = new (memory::pool::palloc(_pool->getPool(), sizeof(TransformVertexData))) TransformVertexData();

		p->transform = t;
		p->data = move(vert);

		cmdData->value = value;
		cmdData->vertexes = makeSpanView(p, 1);
		cmdData->state = _currentState;

		addCommand(cmd);
	});
}

void CommandList::pushShadowArray(SpanView<TransformVertexData> data, float value) {
	_pool->perform([&] {
		auto cmd = Command::create(_pool->getPool(), CommandType::ShadowArray, CommandFlags::None);
		auto cmdData = reinterpret_cast<CmdShadowArray *>(cmd->data);

		cmdData->vertexes = data;
		cmdData->value = value;
		cmdData->state = _currentState;

		addCommand(cmd);
	});
}

void CommandList::pushDeferredShadow(const Rc<DeferredVertexResult> &res, const Mat4 &viewT, const Mat4 &modelT, bool normalized, float value) {
	_pool->perform([&] {
		auto cmd = Command::create(_pool->getPool(), CommandType::ShadowDeferred, CommandFlags::None);
		auto cmdData = reinterpret_cast<CmdShadowDeferred *>(cmd->data);

		cmdData->deferred = res;
		cmdData->viewTransform = viewT;
		cmdData->modelTransform = modelT;
		cmdData->normalized = normalized;
		cmdData->value = value;
		cmdData->state = _currentState;

		addCommand(cmd);
	});
}

void CommandList::pushSdfGroup(const Mat4 &modelT, float value, const Callback<void(CmdSdfGroup2D &)> &cb) {
	_pool->perform([&] {
		auto cmd = Command::create(_pool->getPool(), CommandType::SdfGroup2D, CommandFlags::None);
		auto cmdData = reinterpret_cast<CmdSdfGroup2D *>(cmd->data);

		cmdData->modelTransform = modelT;
		cmdData->value = value;
		cmdData->state = _currentState;

		cb(*cmdData);

		addCommand(cmd);
	});
}

void CommandList::addCommand(Command *cmd) {
	if (!_last) {
		_first = cmd;
	} else {
		_last->next = cmd;
	}
	_last = cmd;
}

}
