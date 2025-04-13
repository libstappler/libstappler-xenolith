/**
 Copyright (c) 2023-2025 Stappler LLC <admin@stappler.dev>

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

#include "AppScene.h"

#include "SPFilepath.h"
#include "XLVkAttachment.h"
#include "XLDirector.h"
#include "XL2dSprite.h"
#include "XLApplication.h"

#include "XLVkAttachment.h"
#include "XL2dSceneContent.h"

#include "AppRootLayout.h"
#include "AppDelegate.h"
#include "backend/vk/XL2dVkShadowPass.h"

#include "MaterialStyleContainer.h"
#include "MaterialSceneContent.h"

namespace stappler::xenolith::app {

bool AppScene::init(Application *app, const core::FrameConstraints &constraints) {
	// build presentation RenderQueue
	core::Queue::Builder builder("Loader");
	builder.addImage("xenolith-1-480.png",
			core::ImageInfo(core::ImageFormat::R8G8B8A8_UNORM, core::ImageUsage::Sampled,
					core::ImageHints::Opaque),
			FileInfo("resources/xenolith-1-480.png"));
	builder.addImage("xenolith-2-480.png",
			core::ImageInfo(core::ImageFormat::R8G8B8A8_UNORM, core::ImageUsage::Sampled,
					core::ImageHints::Opaque),
			FileInfo("resources/xenolith-2-480.png"));

	basic2d::vk::ShadowPass::RenderQueueInfo info{app,
		Extent2(constraints.extent.width, constraints.extent.height),
		basic2d::vk::ShadowPass::Flags::None};

	basic2d::vk::ShadowPass::makeRenderQueue(builder, info);

	if (!Scene2d::init(move(builder), constraints)) {
		return false;
	}

	addComponent(Rc<material2d::StyleContainer>::create());

	auto content = Rc<material2d::SceneContent>::create();

	setContent(content);

	Rc<SceneLayout2d> l;
	auto dataPath = FileInfo("org.stappler.xenolith.test.AppScene.cbor", FileCategory::AppCache);
	if (auto d = data::readFile<Interface>(dataPath)) {
		auto layoutName = getLayoutNameById(d.getString("id"));

		l = makeLayoutNode(layoutName);
		content->pushLayout(l);
		if (!d.getValue("data").empty()) {
			l->setDataValue(move(d.getValue("data")));
		}
	}

	if (!l) {
		content->pushLayout(makeLayoutNode(LayoutName::Root));
	}

	scheduleUpdate();

	return true;
}

void AppScene::onPresented(Director *dir) { Scene2d::onPresented(dir); }

void AppScene::onFinished(Director *dir) { Scene2d::onFinished(dir); }

void AppScene::update(const UpdateTime &time) { Scene2d::update(time); }

void AppScene::handleEnter(Scene *scene) {
	Scene2d::handleEnter(scene);
	std::cout << "AppScene::onEnter\n";
}

void AppScene::handleExit() {
	std::cout << "AppScene::onExit\n";
	Scene2d::handleExit();
}

void AppScene::render(FrameInfo &info) { Scene2d::render(info); }

void AppScene::runLayout(LayoutName l, Rc<SceneLayout2d> &&node) {
	static_cast<SceneContent2d *>(_content)->replaceLayout(node);
	_contentSizeDirty = true;
}

void AppScene::setActiveLayoutId(StringView name, Value &&data) {
	Value sceneData({pair("id", Value(name)), pair("data", Value(move(data)))});

	auto path = FileInfo("org.stappler.xenolith.test.AppScene.cbor", FileCategory::AppCache);
	data::save(sceneData, path, data::EncodeFormat::CborCompressed);
}

} // namespace stappler::xenolith::app
