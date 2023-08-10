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

#include "AppScene.h"

#include "XLVkAttachment.h"
#include "XLDirector.h"
#include "XL2dSprite.h"
#include "XLApplication.h"

#include "XLVkAttachment.h"
#include "XLFontLibrary.h"
#include "XL2dSceneContent.h"

#include "AppRootLayout.h"
#include "AppDelegate.h"
#include "backend/vk/XL2dVkShadowPass.h"

namespace stappler::xenolith::app {

bool AppScene::init(Application *app, const core::FrameContraints &constraints) {
	// build presentation RenderQueue
	core::Queue::Builder builder("Loader");

	basic2d::vk::ShadowPass::RenderQueueInfo info{
		app, Extent2(constraints.extent.width, constraints.extent.height), basic2d::vk::ShadowPass::Flags::None,
		[&] (core::Resource::Builder &resourceBuilder) {
			resourceBuilder.addImage("xenolith-1-480.png",
					core::ImageInfo(core::ImageFormat::R8G8B8A8_UNORM, core::ImageUsage::Sampled, core::ImageHints::Opaque),
					FilePath("resources/xenolith-1-480.png"));
			resourceBuilder.addImage("xenolith-2-480.png",
					core::ImageInfo(core::ImageFormat::R8G8B8A8_UNORM, core::ImageUsage::Sampled, core::ImageHints::Opaque),
					FilePath("resources/xenolith-2-480.png"));
		}
	};

	basic2d::vk::ShadowPass::makeDefaultRenderQueue(builder, info);

	if (!Scene2d::init(move(builder), constraints)) {
		return false;
	}

	auto content = Rc<SceneContent2d>::create();

	setContent(content);

	filesystem::mkdir(filesystem::cachesPath<Interface>());

	Rc<SceneLayout2d> l;
	auto dataPath = filesystem::cachesPath<Interface>("org.stappler.xenolith.test.AppScene.cbor");
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

void AppScene::onPresented(Director *dir) {
	Scene2d::onPresented(dir);
}

void AppScene::onFinished(Director *dir) {
	Scene2d::onFinished(dir);
}

void AppScene::update(const UpdateTime &time) {
	Scene2d::update(time);
}

void AppScene::onEnter(Scene *scene) {
	Scene2d::onEnter(scene);
	std::cout << "AppScene::onEnter\n";
}

void AppScene::onExit() {
	std::cout << "AppScene::onExit\n";
	Scene2d::onExit();
}

void AppScene::render(FrameInfo &info) {
	Scene2d::render(info);
}

void AppScene::runLayout(LayoutName l, Rc<SceneLayout2d> &&node) {
	static_cast<SceneContent2d *>(_content)->replaceLayout(node);
	_contentSizeDirty = true;
}

void AppScene::setActiveLayoutId(StringView name, Value &&data) {
	Value sceneData({
		pair("id", Value(name)),
		pair("data", Value(move(data)))
	});

	auto path = filesystem::cachesPath<Interface>("org.stappler.xenolith.test.AppScene.cbor");
	data::save(sceneData, path, data::EncodeFormat::CborCompressed);
}

}
