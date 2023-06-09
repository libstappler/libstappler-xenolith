/**
 Copyright (c) 2021-2022 Roman Katuntsev <sbkarr@stappler.org>
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

#ifndef XENOLITH_BACKEND_VK_XLVKQUEUEPASS_H_
#define XENOLITH_BACKEND_VK_XLVKQUEUEPASS_H_

#include "XLVkSync.h"
#include "XLVkObject.h"
#include "XLCoreQueuePass.h"

namespace stappler::xenolith::vk {

class CommandBuffer;
class Device;
class Loop;
class Fence;
class CommandPool;

class QueuePass : public core::QueuePass {
public:
	virtual ~QueuePass();

	virtual bool init(QueuePassBuilder &passBuilder) override;
	virtual void invalidate() override;

	QueueOperations getQueueOps() const { return _queueOps; }

	virtual Rc<QueuePassHandle> makeFrameHandle(const FrameQueue &) override;

protected:
	QueueOperations _queueOps = QueueOperations::Graphics;
};

class QueuePassHandle : public core::QueuePassHandle {
public:
	static VkRect2D rotateScissor(const core::FrameContraints &constraints, const URect &scissor);

	virtual ~QueuePassHandle();
	virtual void invalidate();

	virtual bool prepare(FrameQueue &, Function<void(bool)> &&) override;
	virtual void submit(FrameQueue &, Rc<FrameSync> &&, Function<void(bool)> &&onSubmited, Function<void(bool)> &&onComplete) override;
	virtual void finalize(FrameQueue &, bool) override;

	virtual QueueOperations getQueueOps() const;

protected:
	virtual Vector<const CommandBuffer *> doPrepareCommands(FrameHandle &);
	virtual bool doSubmit(FrameHandle &frame, Function<void(bool)> &&onSubmited);

	virtual void doSubmitted(FrameHandle &, Function<void(bool)> &&, bool, Rc<Fence> &&);

	// called before OnComplete event sended to FrameHandle (so, before any finalization)
	virtual void doComplete(FrameQueue &, Function<void(bool)> &&, bool);

	Function<void(bool)> _onPrepared;
	bool _valid = true;
	bool _commandsReady = false;
	bool _descriptorsReady = false;

	Device *_device = nullptr;
	Loop *_loop = nullptr;
	Rc<Fence> _fence;
	Rc<CommandPool> _pool;
	Rc<DeviceQueue> _queue;
	Vector<const CommandBuffer *> _buffers;
	Rc<FrameSync> _sync;
	core::FrameContraints _constraints;
};

}

#endif /* XENOLITH_BACKEND_VK_XLVKQUEUEPASS_H_ */
