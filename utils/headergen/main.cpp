/**
 Copyright (c) 2023-2025 Stappler LLC <admin@stappler.dev>
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

#include "SPCommon.h"
#include "SPMemory.h"
#include "SPFilesystem.h"
#include "SPBitmap.h"
#include "SPVectorImage.h"
#include "SPData.h"
#include "RegistryData.h"

static constexpr auto HELP_STRING(
R"HelpString(headergen <options> registry|icons|material
Options:
    -v (--verbose)
    -h (--help))HelpString");

namespace STAPPLER_VERSIONIZED stappler::headergen {

using namespace mem_std;

static int parseOptionSwitch(Value &ret, char c, const char *str) {
	if (c == 'h') {
		ret.setBool(true, "help");
	} else if (c == 'v') {
		ret.setBool(true, "verbose");
	}
	return 1;
}

static int parseOptionString(Value &ret, const StringView &str, int argc, const char * argv[]) {
	if (str == "help") {
		ret.setBool(true, "help");
	} else if (str == "verbose") {
		ret.setBool(true, "verbose");
	}
	return 1;
}

struct IconData {
	String name;
	String title;
	Bytes data;
	size_t nbytes;
	size_t ncompressed;
};

static IconData &exportIcon(Map<String, IconData> &icons, StringView name, vg::VectorImage &image) {
	auto paths = image.getPaths();
	if (paths.size() > 1) {
		for (auto &it : paths) {
			if (it.second->getStyle() == vg::DrawFlags::None) {
				image.removePath(it.second);
			}
		}
	}
	paths.clear();

	paths = image.getPaths();

	auto it = paths.begin();
	auto path = it->second->getPath();

	++ it;

	while (it != paths.end()) {
		path->addPath(*it->second->getPath());
		++ it;
	}

	auto data = path->encode();

	auto nameStr = name.str<Interface>();
	auto titleStr = name.str<Interface>();
	titleStr[0] = ::toupper(titleStr[0]);

	Bytes compressed;
	//auto compressed = data::compress<Interface>(data.data(), data.size(), data::EncodeFormat::Compression::LZ4HCCompression, true);

	if (!compressed.empty()) {
		auto iit = icons.emplace(nameStr, IconData{nameStr, titleStr, compressed, data.size(), compressed.size()}).first;
		return iit->second;
	} else {
		auto iit = icons.emplace(nameStr, IconData{nameStr, titleStr, data, data.size(), compressed.size()}).first;
		return iit->second;
	}
}

auto LICENSE_STRING =
R"Text(/**
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

// Generated with headergen
)Text";

static void makeMaterialIconSource(FileInfo path, const Map<String, IconData> &icons) {
	StringStream sourceFile;

	sourceFile << LICENSE_STRING <<
R"Text(
///@ SP_EXCLUDE

#include "XLCommon.h"
#include "XLIcons.h"

#include "XLIconImage.cc"

// clang-format off

namespace STAPPLER_VERSIONIZED stappler::xenolith {

)Text";

	for (auto &it : icons) {
		auto text = base16::encode<Interface>(CoderSource(it.second.data));
		auto d = text.c_str();
		bool first = false;
		sourceFile << "static const uint8_t s_icon_" << it.first << "[] = { ";
		for (size_t i = 0; i < text.size() / 2; ++ i) {
			if (!first) {
				first = true;
			} else {
				sourceFile << ",";
			}
			sourceFile << "0x" << d[i * 2] << d[i * 2 + 1];
		}
		sourceFile << "};\n";
	}

	sourceFile <<
R"Text(
StringView getIconName(IconName name) {
	switch (name) {
	case IconName::None: return "Nnne"; break;
	case IconName::Empty: return "Empty"; break;
	case IconName::Stappler_CursorIcon: return "Stappler_CursorIcon"; break;
	case IconName::Stappler_SelectioinStartIcon: return "Stappler_SelectioinStartIcon"; break;
	case IconName::Stappler_SelectioinEndIcon: return "Stappler_SelectioinEndIcon"; break;
	case IconName::Dynamic_Loader: return "Dynamic_Loader"; break;
	case IconName::Dynamic_Nav: return "Dynamic_Nav"; break;
	case IconName::Dynamic_DownloadProgress: return "Dynamic_DownloadProgress"; break;
)Text";

	for (auto &it : icons) {
		sourceFile << "\tcase IconName::" << it.second.title << ": return \"" << it.second.title << "\"; break;\n";
	}

	sourceFile <<
R"Text(	default: break;
	}
	return StringView();
}

bool getIconData(IconName name, const Callback<void(BytesView)> &cb) {
	switch (name) {
	case IconName::None: break;
	case IconName::Empty: break;
	case IconName::Stappler_CursorIcon: break;
	case IconName::Stappler_SelectioinStartIcon: break;
	case IconName::Stappler_SelectioinEndIcon: break;
	case IconName::Dynamic_Loader: break;
	case IconName::Dynamic_Nav: break;
	case IconName::Dynamic_DownloadProgress: break;
)Text";

	for (auto &it : icons) {
		sourceFile << "\tcase IconName::" << it.second.title << ":";
		sourceFile << "cb(BytesView(s_icon_" << it.first << ", " << it.second.nbytes << "));";
		sourceFile << " return true; break;\n";
	}

	sourceFile <<
R"Text(	default: break;
	}
	return false;
}

}
)Text";

	filesystem::write(path, sourceFile.str());
}

static void makeMaterialIconHeader(FileInfo path, const Map<String, IconData> &icons) {
	StringStream headerFile;

	headerFile << LICENSE_STRING <<
R"Text(
#ifndef XENOLITH_RESOURCES_ICONS_XLICONS_H_
#define XENOLITH_RESOURCES_ICONS_XLICONS_H_

#include "XLCommon.h"
#include "SPVectorImage.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith {

enum class IconName : uint16_t {
	None = 0,
	Empty,

	Stappler_CursorIcon,
	Stappler_SelectioinStartIcon,
	Stappler_SelectioinEndIcon,

	Dynamic_Loader,
	Dynamic_Nav,
	Dynamic_DownloadProgress,

)Text";
	for (auto &it : icons) {
		headerFile << "\t" << it.second.title << ",\n";
	}

	headerFile <<
R"Text(	Max
};

SP_PUBLIC StringView getIconName(IconName);
SP_PUBLIC bool getIconData(IconName, const Callback<void(BytesView)> &);

SP_PUBLIC void drawIcon(vg::VectorImage &, IconName, float progress);

}

#endif /* XENOLITH_RESOURCES_ICONS_XLICONS_H_ */
)Text";

	filesystem::write(path, headerFile.str());
}

static int exportMaterialIcons(const FileInfo path) {
	size_t i = 0;
	Map<String, IconData> icons;

	filesystem::ftw(path, [&] (const FileInfo &subpath, FileType type) {
		if (type == FileType::File) {
			auto name = filepath::name(filepath::root(subpath));
			//std::cout << name << "\n";
			if (name == "materialicons" || name == "materialiconsoutlined") {
				if (filepath::fullExtension(subpath) == "svg" && filepath::name(subpath) == "24px") {
					StringStream iconName;
					bool empty = true;
					auto p = filepath::root(subpath.path);
					if (p.starts_with(path.path)) {
						p += path.path.size();
						if (p.is('/')) {
							++ p;
						}
					}
					filepath::split(p, [&] (StringView substr) {
						if (substr == "materialicons") {
							iconName << "_solid";
						} else if (substr == "materialiconsoutlined") {
							iconName << "_outline";
						} else {
							if (!empty) {
								iconName << "_";
							} else {
								empty = false;
							}
							iconName << substr;
						}
					});

					vg::VectorImage image;
					if (image.init(FileInfo(subpath))) {
						auto &ic = exportIcon(icons, iconName.str(), image);

						std::cout << "[" << i << "] " << ic.title << " - " << subpath << " " << ic.nbytes << " - " << ic.ncompressed << "\n";
						++ i;
					} else {
						std::cout << "Fail to open: " << subpath << "\n";
					}
				}
			} else if (name != "materialiconssharp" && name != "materialiconsround" && name != "materialiconstwotone") {
				std::cout << name << " " << subpath << "\n";
			}
		}
		return true;
	});

	size_t full = 0;
	size_t compressed = 0;

	for (auto &it : icons) {
		full += it.second.nbytes;
		if (it.second.ncompressed) {
			compressed += it.second.ncompressed;
		} else {
			compressed += it.second.nbytes;
		}
	}

	std::cout << full << " " << compressed << "\n";

	auto headerPath = FileInfo("gen/XLIcons.h");
	auto sourcePath = FileInfo("gen/XLIcons.cpp");
	filesystem::mkdir(filepath::root(headerPath));
	filesystem::remove(headerPath);
	filesystem::remove(sourcePath);

	makeMaterialIconHeader(headerPath, icons);
	makeMaterialIconSource(sourcePath, icons);

	return 0;
}

SP_EXTERN_C int main(int argc, const char **argv) {
	Value opts;
	Vector<String> args;
	data::parseCommandLineOptions<Interface, Value>(opts, argc, argv,
		[&] (Value &, StringView arg) {
			args.emplace_back(arg.str<Interface>());
		},
			&parseOptionSwitch, &parseOptionString);
	if (opts.getBool("help")) {
		std::cout << HELP_STRING << "\n";
		return 0;
	}

	if (opts.getBool("verbose")) {
		std::cout << " Current work dir: " << filesystem::currentDir<Interface>() << "\n";
		std::cout << " Options: " << data::EncodeFormat::Pretty << opts << "\n";
	}

	return perform_main([&] {
		if (args.size() <= 1 || args.at(1) == "registry") {
			RegistryData registryData;
			if (registryData.load()) {
				registryData.write();
			}
		} else if (args.at(1) == "icons") {
			auto exportIcon = [] (StringView path) {
				auto name = filepath::name(path);
				auto target = filepath::merge<Interface>(filepath::root(path), toString(name, ".lzimg"));
				auto targetH = filepath::merge<Interface>(filepath::root(path), toString(name, ".h"));
				auto b = filesystem::readIntoMemory<Interface>(path);
	
				Bitmap bmp(b);
				bmp.convert(bitmap::PixelFormat::RGBA8888);
	
				std::cout << "Image: " << filepath::name(path) << ": " << bmp.width() << " x " << bmp.height() << "\n";
	
				Value val{
					pair("width", Value(bmp.width())),
					pair("height", Value(bmp.height())),
					pair("data", Value(bmp.data()))
				};
	
				data::save(val, FileInfo(target), data::EncodeFormat::CborCompressed);
				auto bytes = data::write(val, data::EncodeFormat::CborCompressed);
	
				size_t counter = 16;
	
				StringStream stream;
				stream << "icon = {" << std::hex;\
				for (auto &it : bytes) {
					++ counter;
					if (counter >= 16) {
						stream << "\n\t";
						counter = 0;
					} else {
						stream << " ";
					}
					stream << "0x" << uint32_t(it) << ",";
				}
				stream << "\n}\n";
	
				filesystem::write(FileInfo(targetH), stream.str());
			};
	
			exportIcon(filepath::reconstructPath<Interface>(
					filesystem::currentDir<Interface>("../../resources/images/window-close-symbolic.png")));
			exportIcon(filepath::reconstructPath<Interface>(
					filesystem::currentDir<Interface>("../../resources/images/window-maximize-symbolic.png")));
			exportIcon(filepath::reconstructPath<Interface>(
					filesystem::currentDir<Interface>("../../resources/images/window-minimize-symbolic.png")));
			exportIcon(filepath::reconstructPath<Interface>(
					filesystem::currentDir<Interface>("../../resources/images/window-restore-symbolic.png")));
			exportIcon(filepath::reconstructPath<Interface>(
					filesystem::currentDir<Interface>("../../resources/images/window-close-symbolic-active.png")));
			exportIcon(filepath::reconstructPath<Interface>(
					filesystem::currentDir<Interface>("../../resources/images/window-maximize-symbolic-active.png")));
			exportIcon(filepath::reconstructPath<Interface>(
					filesystem::currentDir<Interface>("../../resources/images/window-minimize-symbolic-active.png")));
			exportIcon(filepath::reconstructPath<Interface>(
					filesystem::currentDir<Interface>("../../resources/images/window-restore-symbolic-active.png")));
		} else if (args.at(1) == "material" && args.size() > 2) {
			auto path = args.at(2);
			return exportMaterialIcons(FileInfo(path));
		}
	
		return 0;
	});
}

}
