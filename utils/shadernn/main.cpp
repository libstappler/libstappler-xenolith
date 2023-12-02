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

#include "XLCommon.h"
#include "XLCore.h"
#include "SPData.h"
#include "XLApplication.h"
#include "XLViewCommandLine.h"
#include "XLVkPlatform.h"
#include "XLVkLoop.h"

#include "XLSnnGenTest.h"
#include "XLSnnInputTest.h"
#include "XLSnnModelTest.h"
#include "XLSnnModelProcessor.h"

namespace stappler::xenolith::shadernn {

void runApplication(Function<void(const Application &)> &&cb) {
	Application::CommonInfo commonInfo({
		.bundleName = String("org.stappler.xenolith.cli"),
		.applicationName = String("xenolith-cli"),
		.applicationVersion = String("0.1.0")
	});

	auto instance = vk::platform::createInstance([&] (vk::platform::VulkanInstanceData &data, const vk::platform::VulkanInstanceInfo &info) {
		data.applicationName = commonInfo.applicationName;
		data.applicationVersion = commonInfo.applicationVersion;
		return true;
	});

	// create main looper
	auto app = Rc<Application>::create(move(commonInfo), move(instance));

	// define device selector/initializer
	auto data = Rc<vk::LoopData>::alloc();
	data->deviceSupportCallback = [] (const vk::DeviceInfo &dev) {
		return dev.requiredExtensionsExists && dev.requiredFeaturesExists;
	};

	Application::CallbackInfo callbackInfo({
		.updateCallback = [&] (const Application &app, const UpdateTime &time) {
			if (time.app == 0) {
				cb(app);
			}
		}
	});

	core::LoopInfo info;
	info.platformData = data;

	// run main loop with 2 additional threads and 0.5sec update interval
	app->run(callbackInfo, move(info), 2, TimeInterval::microseconds(500000));
}

static constexpr auto HELP_STRING(
R"HelpString(testapp <options>
Options are one of:
	-v (--verbose)
	-h (--help))HelpString");

SP_EXTERN_C int _spMain(argc, argv) {
	ViewCommandLineData data;
	Vector<String> args;

	data::parseCommandLineOptions<Interface, ViewCommandLineData>(data, argc, argv,
		[&] (ViewCommandLineData &, StringView str) {
			args.emplace_back(str.str<Interface>());
		},
		&parseViewCommandLineSwitch, &parseViewCommandLineString);

	if (data.help) {
		std::cout << HELP_STRING << "\n";
		return 0;
	}

	if (data.verbose) {
		std::cout << " Current work dir: " << stappler::filesystem::currentDir<Interface>() << "\n";
		std::cout << " Documents dir: " << stappler::filesystem::documentsPathReadOnly<Interface>() << "\n";
		std::cout << " Cache dir: " << stappler::filesystem::cachesPathReadOnly<Interface>() << "\n";
		std::cout << " Writable dir: " << stappler::filesystem::writablePathReadOnly<Interface>() << "\n";
		std::cout << " Options: " << stappler::data::EncodeFormat::Pretty << data.encode() << "\n";
		std::cout << " Arguments: \n";
		for (auto &it : args) {
			std::cout << "\t" << it << "\n";
		}
	}

	if (args.size() >= 2) {
		auto arg = args.at(1);

		runApplication([&] (const Application &app) {
			if (arg == "model" && args.size() == 4) {
				auto modelPath = args.at(2);
				auto inputPath = args.at(3);

				auto queue = Rc<ModelQueue>::create(modelPath, ModelFlags::None, inputPath);
				if (queue) {
					app.getGlLoop()->compileQueue(queue, [app = &app, queue] (bool success) {
						Application::getInstance()->performOnMainThread([app, queue] {
							queue->run(const_cast<Application *>(app));
						}, nullptr);
					});
				}
				return 0;
			} else if (arg == "gen") {
				auto queue = Rc<vk::shadernn::GenQueue>::create();

				// then compile it on graphics device
				app.getGlLoop()->compileQueue(queue, [app = &app, queue] (bool success) {
					Application::getInstance()->performOnMainThread([app, queue] {
						queue->run(const_cast<Application *>(app), Extent3(16, 16, 16));
					}, nullptr);
				});
			} else if (arg == "input" && args.size() == 3) {
				auto image = args.at(2);
				auto queue = Rc<vk::shadernn::InputQueue>::create();

				// then compile it on graphics device
				app.getGlLoop()->compileQueue(queue, [app = &app, queue, image] (bool success) {
					Application::getInstance()->performOnMainThread([app, queue, image] {
						queue->run(const_cast<Application *>(app), image);
					}, nullptr);
				});
			}
			return 0;
		});
	}

	return 0;
}

}
