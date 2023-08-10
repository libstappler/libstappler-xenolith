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

#ifndef XENOLITH_CORE_XLCOREFRAMEREQUEST_H_
#define XENOLITH_CORE_XLCOREFRAMEREQUEST_H_

#include "XLCoreFrameEmitter.h"

namespace stappler::xenolith::core {

struct FrameOutputBinding : public Ref {
	using CompleteCallback = Function<bool(FrameAttachmentData &data, bool success, Ref *)>;

	const AttachmentData *attachment = nullptr;
	CompleteCallback callback;
	Rc<Ref> handle;

	FrameOutputBinding(const AttachmentData *, CompleteCallback &&, Rc<Ref> && = nullptr);

	virtual ~FrameOutputBinding();

	bool handleReady(FrameAttachmentData &data, bool success);
};

class FrameRequest final : public Ref {
public:
	using CompleteCallback = FrameOutputBinding::CompleteCallback;

	virtual ~FrameRequest();

	bool init(const Rc<FrameEmitter> &, const FrameContraints &);
	bool init(const Rc<Queue> &q);
	bool init(const Rc<Queue> &q, const FrameContraints &);
	bool init(const Rc<Queue> &q, const Rc<FrameEmitter> &, const FrameContraints &);

	void addSignalDependency(Rc<DependencyEvent> &&);
	void addSignalDependencies(Vector<Rc<DependencyEvent>> &&);

	void addImageSpecialization(const ImageAttachment *, ImageInfoData &&);
	const ImageInfoData *getImageSpecialization(const ImageAttachment *image) const;

	bool addInput(const Attachment *a, Rc<AttachmentInputData> &&);
	bool addInput(const AttachmentData *, Rc<AttachmentInputData> &&);

	void setQueue(const Rc<Queue> &q);

	void setOutput(Rc<FrameOutputBinding> &&);
	void setOutput(const AttachmentData *, CompleteCallback &&, Rc<Ref> && = nullptr);
	void setOutput(const Attachment *a, CompleteCallback &&cb, Rc<Ref> && = nullptr);

	void setRenderTarget(const AttachmentData *, Rc<ImageStorage> &&);

	bool onOutputReady(Loop &, FrameAttachmentData &);
	void onOutputInvalidated(Loop &, FrameAttachmentData &);

	void finalize(Loop &, HashMap<const AttachmentData *, FrameAttachmentData *> &attachments, bool success);
	void signalDependencies(Loop &, Queue *, bool success);

	Rc<AttachmentInputData> getInputData(const AttachmentData *attachment);

	const Rc<PoolRef> &getPool() const { return _pool; }
	Rc<ImageStorage> getRenderTarget(const AttachmentData *);

	const Rc<FrameEmitter> &getEmitter() const { return _emitter; }
	const Rc<Queue> &getQueue() const { return _queue; }

	Set<Rc<Queue>> getQueueList() const;

	const FrameContraints & getFrameConstraints() const { return _constraints; }

	void setReadyForSubmit(bool value) { _readyForSubmit = value; }
	bool isReadyForSubmit() const { return _readyForSubmit; }

	bool isPersistentMapping() const { return _persistentMappings; }

	void setSceneId(uint64_t val) { _sceneId = val; }
	uint64_t getSceneId() const { return _sceneId; }

	const Vector<Rc<DependencyEvent>> &getSignalDependencies() const { return _signalDependencies; }

	FrameRequest() = default;

	void waitForInput(FrameQueue &, const Rc<AttachmentHandle> &a, Function<void(bool)> &&cb);

	const FrameOutputBinding *getOutputBinding(const AttachmentData *) const;

protected:
	FrameRequest(const FrameRequest &) = delete;
	FrameRequest &operator=(const FrameRequest &) = delete;

	Rc<PoolRef> _pool;
	Rc<FrameEmitter> _emitter;
	Rc<Queue> _queue;
	FrameContraints _constraints;
	Map<const AttachmentData *, Rc<AttachmentInputData>> _input;
	bool _readyForSubmit = true; // if true, do not wait synchronization with other active frames in emitter
	bool _persistentMappings = true; // true; // true; // try to map per-frame GPU memory persistently
	uint64_t _sceneId = 0;

	Map<const ImageAttachment *, ImageInfoData> _imageSpecialization;
	Map<const AttachmentData *, Rc<FrameOutputBinding>> _output;
	Map<const AttachmentData *, Rc<ImageStorage>> _renderTargets;

	Vector<Rc<DependencyEvent>> _signalDependencies;

	struct WaitInputData {
		Rc<FrameQueue> queue;
		AttachmentHandle *handle;
		Function<void(bool)> callback;
	};

	Map<const AttachmentData *, WaitInputData> _waitForInputs;
};

}

#endif /* XENOLITH_CORE_XLCOREFRAMEREQUEST_H_ */
