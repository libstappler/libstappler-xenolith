/**
Copyright (c) 2021 Roman Katuntsev <sbkarr@stappler.org>
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

#ifndef XENOLITH_BACKEND_VK_XLVKRENDERPASSIMPL_H_
#define XENOLITH_BACKEND_VK_XLVKRENDERPASSIMPL_H_

#include "XLVk.h"
#include "XLCoreQueuePass.h"
#include "XLCoreObject.h"

namespace stappler::xenolith::vk {

class Device;
class RenderPass;

struct DescriptorData {
	core::ObjectHandle object;
	Rc<Ref> data;
};

struct DescriptorBinding {
	VkDescriptorType type;
	Vector<DescriptorData> data;

	~DescriptorBinding();

	DescriptorBinding(VkDescriptorType, uint32_t count);

	Rc<Ref> write(uint32_t, DescriptorBufferInfo &&);
	Rc<Ref> write(uint32_t, DescriptorImageInfo &&);
	Rc<Ref> write(uint32_t, DescriptorBufferViewInfo &&);
};

struct DescriptorSet : public Ref {
	VkDescriptorSet set;
	Vector<DescriptorBinding> bindings;
};

class Framebuffer : public core::Framebuffer {
public:
	virtual ~Framebuffer() { }

	bool init(Device &dev, RenderPass *renderPass, SpanView<Rc<core::ImageView>> imageViews, Extent2 extent);

	VkFramebuffer getFramebuffer() const { return _framebuffer; }

protected:
	using core::Framebuffer::init;

	VkFramebuffer _framebuffer = VK_NULL_HANDLE;
};

class RenderPass : public core::RenderPass {
public:
	struct LayoutData {
		VkPipelineLayout layout = VK_NULL_HANDLE;
		VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
		Vector<VkDescriptorSetLayout> layouts;
		Vector<Rc<DescriptorSet>> sets;
	};

	struct Data {
		VkRenderPass renderPass = VK_NULL_HANDLE;
		VkRenderPass renderPassAlternative = VK_NULL_HANDLE;
		Vector<LayoutData> layouts;

		bool cleanup(Device &dev);
	};

	virtual ~RenderPass() { }

	virtual bool init(Device &dev, QueuePassData &);

	VkRenderPass getRenderPass(bool alt = false) const;
	VkPipelineLayout getPipelineLayout(uint32_t idx) const { return _data->layouts[idx].layout; }
	const Vector<Rc<DescriptorSet>> &getDescriptorSets(uint32_t idx) const { return _data->layouts[idx].sets; }
	const Vector<VkClearValue> &getClearValues() const { return _clearValues; }

	//VkDescriptorSet getDescriptorSet(uint32_t) const;

	// if async is true - update descriptors with updateAfterBind flag
	// 			   false - without updateAfterBindFlag
	virtual bool writeDescriptors(const core::QueuePassHandle &, uint32_t layoutIndex, bool async) const;

	virtual void perform(const core::QueuePassHandle &, CommandBuffer &buf, const Callback<void()> &);

protected:
	using core::RenderPass::init;

	bool initGraphicsPass(Device &dev, QueuePassData &);
	bool initComputePass(Device &dev, QueuePassData &);
	bool initTransferPass(Device &dev, QueuePassData &);
	bool initGenericPass(Device &dev, QueuePassData &);

	bool initDescriptors(Device &dev, const QueuePassData &, Data &);
	bool initDescriptors(Device &dev, const core::PipelineLayoutData &, Data &, LayoutData &);

	Vector<VkAttachmentDescription> _attachmentDescriptions;
	Vector<VkAttachmentDescription> _attachmentDescriptionsAlternative;
	Vector<VkAttachmentReference> _attachmentReferences;
	Vector<uint32_t> _preservedAttachments;
	Vector<VkSubpassDependency> _subpassDependencies;
	Vector<VkSubpassDescription> _subpasses;
	Set<const core::AttachmentPassData *> _variableAttachments;
	Data *_data = nullptr;

	Vector<VkClearValue> _clearValues;
};

}

#endif /* XENOLITH_BACKEND_VK_XLVKRENDERPASSIMPL_H_ */
