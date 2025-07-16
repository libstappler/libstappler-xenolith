/**
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

#ifndef XENOLITH_APPLICATION_LINUX_XLLINUXDBUS_H_
#define XENOLITH_APPLICATION_LINUX_XLLINUXDBUS_H_

#include "XLCommon.h"

#if LINUX

#include "XLLinux.h"

#include <dbus/dbus.h>

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform {

namespace dbus {

class Library;
struct Connection;

struct Event {
	enum Type {
		None,
		AddWatch,
		ToggleWatch,
		RemoveWatch,
		TriggerWatch, // emitted by polling
		AddTimeout,
		ToggleTimeout,
		RemoveTimeout,
		TriggerTimeout, // emitted by polling
		Dispatch,
		Wakeup,

		Connected,
		Message,
	};

	Type type = None;
	union {
		DBusWatch *watch;
		DBusTimeout *timeout;
		DBusMessage *message;
	};
};

enum class Type : int {
	Invalid = int('\0'),
	Byte = int('y'),
	Boolean = int('b'),
	Int16 = int('n'),
	Uint16 = int('q'),
	Int32 = int('i'),
	Uint32 = int('u'),
	Int64 = int('x'),
	Uint64 = int('t'),
	Double = int('d'),
	String = int('s'),
	Path = int('o'),
	Signature = int('g'),
	Fd = int('h'),

	/* Compound types */
	Array = int('a'),
	Variant = int('v'),
	Struct = int('r'),
	DictEntry = int('e'),
};

enum class MessageType {
	Invalid = int(0),
	MethodCall = int(1),
	MethodReturn = int(2),
	Error = int(3),
	Signal = int(4),
};

struct BasicValue {
	Type type;
	DBusBasicValue value;
};

struct SP_PUBLIC Error {
	Library *iface = nullptr;

	Error(Library *);
	~Error();

	bool isSet() const;
	void reset();
};

struct SP_PUBLIC BusFilter : public Ref {
	DBusError error;
	Rc<Connection> connection;
	String filter;
	bool added = false;

	virtual ~BusFilter();
	BusFilter(NotNull<Connection>, StringView filter);
};

struct SP_PUBLIC Connection : public Ref {
	using EventCallback = Function<uint32_t(Connection *, const Event &)>;

	Rc<Library> lib;
	EventCallback callback;
	DBusConnection *connection = nullptr;
	DBusBusType type;
	DBusError error;

	bool connected = false;
	Set<String> services;

	virtual ~Connection();

	Connection(Library *, EventCallback &&, DBusBusType);

	void setup();

	explicit operator bool() const { return connection != nullptr; }

	DBusPendingCall *callMethod(StringView bus, StringView path, StringView iface,
			StringView method, const Callback<void(DBusMessage *)> &,
			Function<void(NotNull<Connection>, DBusMessage *)> &&, Ref * = nullptr);

	bool handle(event::Handle *, const Event &, event::PollFlags);

	void flush();
	DBusDispatchStatus dispatch();
	void dispatchAll();

	void close();
};

class SP_PUBLIC Library : public Ref {
public:
	virtual ~Library() = default;

	Library() { }

	bool init();

	bool open(Dso &handle);
	void close();

	template <typename Parser>
	bool parseMessage(NotNull<DBusMessage>, Parser &);

	template <typename Parser>
	bool parseMessage(NotNull<DBusMessageIter>, Parser &);

	decltype(&_xl_null_fn) _dbus_first_fn = &_xl_null_fn;
	XL_DEFINE_PROTO(dbus_error_init)
	XL_DEFINE_PROTO(dbus_error_free)
	XL_DEFINE_PROTO(dbus_message_new_method_call)
	XL_DEFINE_PROTO(dbus_message_append_args)
	XL_DEFINE_PROTO(dbus_message_is_signal)
	XL_DEFINE_PROTO(dbus_message_is_error)
	XL_DEFINE_PROTO(dbus_message_unref)
	XL_DEFINE_PROTO(dbus_message_iter_init)
	XL_DEFINE_PROTO(dbus_message_iter_recurse)
	XL_DEFINE_PROTO(dbus_message_iter_next)
	XL_DEFINE_PROTO(dbus_message_iter_has_next)
	XL_DEFINE_PROTO(dbus_message_iter_get_arg_type)
	XL_DEFINE_PROTO(dbus_message_iter_get_element_type)
	XL_DEFINE_PROTO(dbus_message_iter_get_element_count)
	XL_DEFINE_PROTO(dbus_message_iter_get_fixed_array)
	XL_DEFINE_PROTO(dbus_message_iter_get_basic)
	XL_DEFINE_PROTO(dbus_message_iter_get_signature)
	XL_DEFINE_PROTO(dbus_message_get_type)
	XL_DEFINE_PROTO(dbus_message_get_path)
	XL_DEFINE_PROTO(dbus_message_get_interface)
	XL_DEFINE_PROTO(dbus_message_get_member)
	XL_DEFINE_PROTO(dbus_message_get_error_name)
	XL_DEFINE_PROTO(dbus_message_get_destination)
	XL_DEFINE_PROTO(dbus_message_get_sender)
	XL_DEFINE_PROTO(dbus_message_get_signature)
	XL_DEFINE_PROTO(dbus_connection_set_exit_on_disconnect)
	XL_DEFINE_PROTO(dbus_connection_send_with_reply_and_block)
	XL_DEFINE_PROTO(dbus_connection_send_with_reply)
	XL_DEFINE_PROTO(dbus_connection_set_watch_functions)
	XL_DEFINE_PROTO(dbus_connection_set_timeout_functions)
	XL_DEFINE_PROTO(dbus_connection_set_wakeup_main_function)
	XL_DEFINE_PROTO(dbus_connection_set_dispatch_status_function)
	XL_DEFINE_PROTO(dbus_connection_add_filter)
	XL_DEFINE_PROTO(dbus_connection_close)
	XL_DEFINE_PROTO(dbus_connection_unref)
	XL_DEFINE_PROTO(dbus_connection_flush)
	XL_DEFINE_PROTO(dbus_connection_dispatch)
	XL_DEFINE_PROTO(dbus_error_is_set)
	XL_DEFINE_PROTO(dbus_bus_get)
	XL_DEFINE_PROTO(dbus_bus_get_private)
	XL_DEFINE_PROTO(dbus_bus_add_match)
	XL_DEFINE_PROTO(dbus_bus_remove_match)
	XL_DEFINE_PROTO(dbus_pending_call_ref)
	XL_DEFINE_PROTO(dbus_pending_call_unref)
	XL_DEFINE_PROTO(dbus_pending_call_set_notify)
	XL_DEFINE_PROTO(dbus_pending_call_get_completed)
	XL_DEFINE_PROTO(dbus_pending_call_steal_reply)
	XL_DEFINE_PROTO(dbus_pending_call_block)
	XL_DEFINE_PROTO(dbus_watch_get_unix_fd)
	XL_DEFINE_PROTO(dbus_watch_get_flags)
	XL_DEFINE_PROTO(dbus_watch_get_data)
	XL_DEFINE_PROTO(dbus_watch_set_data)
	XL_DEFINE_PROTO(dbus_watch_handle)
	XL_DEFINE_PROTO(dbus_watch_get_enabled)
	XL_DEFINE_PROTO(dbus_timeout_get_interval)
	XL_DEFINE_PROTO(dbus_timeout_get_data)
	XL_DEFINE_PROTO(dbus_timeout_set_data)
	XL_DEFINE_PROTO(dbus_timeout_handle)
	XL_DEFINE_PROTO(dbus_timeout_get_enabled)
	decltype(&_xl_null_fn) _dbus_last_fn = &_xl_null_fn;

protected:
	Dso _handle;
};

SP_PUBLIC unsigned int getWatchFlags(event::PollFlags events);
SP_PUBLIC event::PollFlags getPollFlags(unsigned int);
SP_PUBLIC void describe(Library *lib, NotNull<DBusMessage> message, const CallbackStream &);
SP_PUBLIC void describe(Library *lib, NotNull<DBusMessageIter> message, const CallbackStream &);

SP_PUBLIC bool isFixedType(Type);
SP_PUBLIC bool isBasicType(Type);
SP_PUBLIC bool isContainerType(Type);

} // namespace dbus

static constexpr auto NM_SERVICE_NAME = "org.freedesktop.NetworkManager";
static constexpr auto NM_SERVICE_CONNECTION_NAME =
		"org.freedesktop.NetworkManager.Connection.Active";
static constexpr auto NM_SERVICE_VPN_NAME = "org.freedesktop.NetworkManager.VPN.Plugin";
static constexpr auto NM_SERVICE_FILTER =
		"type='signal',interface='org.freedesktop.NetworkManager'";
static constexpr auto NM_SERVICE_CONNECTION_FILTER =
		"type='signal',interface='org.freedesktop.NetworkManager.Connection.Active'";
static constexpr auto NM_SERVICE_VPN_FILTER =
		"type='signal',interface='org.freedesktop.NetworkManager.VPN.Plugin'";
static constexpr auto NM_SERVICE_PATH = "/org/freedesktop/NetworkManager";
static constexpr auto NM_SIGNAL_STATE_CHANGED = "StateChanged";
static constexpr auto NM_SIGNAL_PROPERTIES_CHANGED = "PropertiesChanged";

enum NMState {
	NM_STATE_UNKNOWN = 0, // networking state is unknown
	NM_STATE_ASLEEP = 10, // networking is not enabled
	NM_STATE_DISCONNECTED = 20, // there is no active network connection
	NM_STATE_DISCONNECTING = 30, // network connections are being cleaned up
	NM_STATE_CONNECTING = 40, // a network connection is being started
	NM_STATE_CONNECTED_LOCAL = 50, // there is only local IPv4 and/or IPv6 connectivity
	NM_STATE_CONNECTED_SITE = 60, // there is only site-wide IPv4 and/or IPv6 connectivity
	NM_STATE_CONNECTED_GLOBAL = 70, // there is global IPv4 and/or IPv6 Internet connectivity
};

enum NMConnectivityState {
	NM_CONNECTIVITY_UNKNOWN = 1, // Network connectivity is unknown.
	NM_CONNECTIVITY_NONE = 2, // The host is not connected to any network.
	NM_CONNECTIVITY_PORTAL =
			3, // The host is behind a captive portal and cannot reach the full Internet.
	NM_CONNECTIVITY_LIMITED =
			4, // The host is connected to a network, but does not appear to be able to reach the full Internet.
	NM_CONNECTIVITY_FULL =
			5, // The host is connected to a network, and appears to be able to reach the full Internet.
};

enum NMMetered {
	NM_METERED_UNKNOWN = 0, // The metered status is unknown
	NM_METERED_YES = 1, // Metered, the value was statically set
	NM_METERED_NO = 2, // Not metered, the value was statically set
	NM_METERED_GUESS_YES = 3, // Metered, the value was guessed
	NM_METERED_GUESS_NO = 4, // Not metered, the value was guessed
};

struct SP_PUBLIC NetworkState {
	bool networkingEnabled = false;
	bool wirelessEnabled = false;
	bool wwanEnabled = false;
	bool wimaxEnabled = false;
	NMMetered metered = NMMetered::NM_METERED_UNKNOWN;
	NMState state = NMState::NM_STATE_UNKNOWN;
	NMConnectivityState connectivity = NMConnectivityState::NM_CONNECTIVITY_UNKNOWN;
	String primaryConnectionType;
	Vector<uint32_t> capabilities;

	NetworkState() = default;
	NetworkState(NotNull<dbus::Library>, NotNull<DBusMessage>);

	void description(const CallbackStream &out) const;

	bool operator==(const NetworkState &) const = default;
	bool operator!=(const NetworkState &) const = default;
};

} // namespace stappler::xenolith::platform

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform::dbus {

template <typename Parser>
struct MessageParserData {
	Library *lib = nullptr;
	Parser *parser = nullptr;
	BasicValue value;
};

template <typename T>
struct MessageParserTraits {
	static constexpr auto onBasicValue =
			requires(T *t, const BasicValue &val) { t->onBasicValue(val); };

	static constexpr auto onArrayBegin = requires(T *t, Type type) { t->onArrayBegin(type); };
	static constexpr auto onArrayEnd = requires(T *t) { t->onArrayEnd(); };

	static constexpr auto onArray = requires(T *t, size_t size, Type type,
			NotNull<DBusMessageIter> iter) { t->onArray(size, type, iter); };

	static constexpr auto onStructBegin = requires(T *t, StringView str) { t->onStructBegin(str); };
	static constexpr auto onStructEnd = requires(T *t) { t->onStructEnd(); };

	static constexpr auto onStruct = requires(T *t, StringView str, NotNull<DBusMessageIter> iter) {
		t->onStruct(str, iter);
	};

	static constexpr auto onVariantBegin =
			requires(T *t, StringView str) { t->onVariantBegin(str); };
	static constexpr auto onVariantEnd = requires(T *t) { t->onVariantEnd(); };

	static constexpr auto onVariant = requires(T *t, StringView str,
			NotNull<DBusMessageIter> iter) { t->onVariant(str, iter); };

	static constexpr auto onDictEntryBegin = requires(T *t) { t->onVariantBegin(); };
	static constexpr auto onDictEntryEnd = requires(T *t) { t->onVariantEnd(); };

	static constexpr auto onDictEntry = requires(T *t, const BasicValue &val,
			NotNull<DBusMessageIter> iter) { t->onDictEntry(val, iter); };
};

template <typename Parser>
static inline bool _parseMessage(MessageParserData<Parser> &data, DBusMessageIter *iter,
		Type rootType = Type::Invalid) {
	using Traits = MessageParserTraits<Parser>;

	Type currentType = Type::Invalid;

	while ((currentType = Type(data.lib->dbus_message_iter_get_arg_type(iter))) != Type::Invalid) {
		currentType = Type(data.lib->dbus_message_iter_get_arg_type(iter));
		switch (currentType) {
		case Type::Invalid: break;
		case Type::Byte:
		case Type::Boolean:
		case Type::Int16:
		case Type::Uint16:
		case Type::Int32:
		case Type::Uint32:
		case Type::Int64:
		case Type::Uint64:
		case Type::Double:
		case Type::String:
		case Type::Path:
		case Type::Signature:
		case Type::Fd:
			data.value.type = currentType;
			data.lib->dbus_message_iter_get_basic(iter, &data.value.value);
			if constexpr (Traits::onBasicValue) {
				if (!data.parser->onBasicValue(data.value)) {
					return false;
				}
			} else {
				return false;
			}
			break;

		case Type::Array:
			if constexpr (Traits::onArray) {
				DBusMessageIter sub;
				data.lib->dbus_message_iter_recurse(iter, &sub);
				auto elemType = Type(data.lib->dbus_message_iter_get_element_type(iter));
				size_t size = maxOf<size_t>();
				if (isFixedType(elemType)) {
					size = data.lib->dbus_message_iter_get_element_count(iter);
				}
				if (!data.parser->onArray(size, elemType, &sub)) {
					return false;
				}
			} else if constexpr (Traits::onArrayBegin && Traits::onArrayEnd) {
				if (!data.parser->onArrayBegin(
							Type(data.lib->dbus_message_iter_get_element_type(iter)))) {
					return false;
				}

				DBusMessageIter sub;
				data.lib->dbus_message_iter_recurse(iter, &sub);
				auto ret = _parseMessage(data, &sub, currentType);

				if (!data.parser->onArrayEnd() || !ret) {
					return false;
				}
			} else {
				return false;
			}
			break;
		case Type::Struct:
			if constexpr (Traits::onStruct) {
				DBusMessageIter sub;
				data.lib->dbus_message_iter_recurse(iter, &sub);

				if (!data.parser->onStruct(
							StringView(data.lib->dbus_message_iter_get_signature(&sub)), &sub)) {
					return false;
				}
			} else if constexpr (Traits::onStructBegin && Traits::onStructEnd) {
				DBusMessageIter sub;
				data.lib->dbus_message_iter_recurse(iter, &sub);

				if (!data.parser->onStructBegin(
							StringView(data.lib->dbus_message_iter_get_signature(&sub)))) {
					return false;
				}

				auto ret = _parseMessage(data, &sub, currentType);

				if (!data.parser->onStructEnd() || !ret) {
					return false;
				}
			} else {
				return false;
			}
			break;
		case Type::Variant:
			if constexpr (Traits::onVariant) {
				DBusMessageIter sub;
				data.lib->dbus_message_iter_recurse(iter, &sub);

				if (!data.parser->onVariant(
							StringView(data.lib->dbus_message_iter_get_signature(&sub)), &sub)) {
					return false;
				}
			} else if constexpr (Traits::onVariantBegin && Traits::onVariantEnd) {
				DBusMessageIter sub;
				data.lib->dbus_message_iter_recurse(iter, &sub);

				if (!data.parser->onVariantBegin(
							StringView(data.lib->dbus_message_iter_get_signature(&sub)))) {
					return false;
				}

				auto ret = _parseMessage(data, &sub, currentType);

				if (!data.parser->onVariantEnd() || !ret) {
					return false;
				}
			} else {
				DBusMessageIter sub;
				data.lib->dbus_message_iter_recurse(iter, &sub);
				if (!_parseMessage(data, &sub, currentType)) {
					return false;
				}
			}
			break;
		case Type::DictEntry:
			if (rootType == Type::Array) {
				if constexpr (Traits::onDictEntry) {
					DBusMessageIter sub;
					data.lib->dbus_message_iter_recurse(iter, &sub);
					auto type = Type(data.lib->dbus_message_iter_get_arg_type(&sub));
					switch (type) {
					case Type::Byte:
					case Type::Boolean:
					case Type::Int16:
					case Type::Uint16:
					case Type::Int32:
					case Type::Uint32:
					case Type::Int64:
					case Type::Uint64:
					case Type::Double:
					case Type::String:
					case Type::Path:
					case Type::Signature:
					case Type::Fd:
						data.value.type = type;
						data.lib->dbus_message_iter_get_basic(&sub, &data.value.value);
						data.lib->dbus_message_iter_next(&sub);
						if (!data.parser->onDictEntry(data.value, &sub)) {
							return false;
						}
						break;
					default:
						log::error("DBus", "invalid DictEntry key");
						return false;
						break;
					}

				} else if constexpr (Traits::onDictEntryBegin && Traits::onDictEntryEnd) {
					if (!data.parser->onDictEntryBegin()) {
						return false;
					}

					DBusMessageIter sub;
					data.lib->dbus_message_iter_recurse(iter, &sub);
					auto ret = _parseMessage(data, &sub, currentType);

					if (!data.parser->onDictEntryEnd() || !ret) {
						return false;
					}
				} else {
					DBusMessageIter sub;
					data.lib->dbus_message_iter_recurse(iter, &sub);
					if (!_parseMessage(data, &sub, currentType)) {
						return false;
					}
				}
			} else {
				log::error("DBus", "DictEntry should be within Array");
				return false;
			}
			break;
		}
		data.lib->dbus_message_iter_next(iter);
	}
	return true;
}

template <typename Parser>
inline bool Library::parseMessage(NotNull<DBusMessage> msg, Parser &parser) {
	MessageParserData<Parser> data{this, &parser};

	DBusMessageIter iter;
	dbus_message_iter_init(msg, &iter);

	return _parseMessage(data, &iter);
}

template <typename Parser>
inline bool Library::parseMessage(NotNull<DBusMessageIter> iter, Parser &parser) {
	MessageParserData<Parser> data{this, &parser};
	return _parseMessage(data, iter);
}

inline const CallbackStream &operator<<(const CallbackStream &stream, Type t) {
	switch (t) {
	case Type::Invalid: stream << "Invalid"; break;
	case Type::Byte: stream << "Byte"; break;
	case Type::Boolean: stream << "Boolean"; break;
	case Type::Int16: stream << "Int16"; break;
	case Type::Uint16: stream << "Uint16"; break;
	case Type::Int32: stream << "Int32"; break;
	case Type::Uint32: stream << "Uint32"; break;
	case Type::Int64: stream << "Int64"; break;
	case Type::Uint64: stream << "Uint64"; break;
	case Type::Double: stream << "Double"; break;
	case Type::String: stream << "String"; break;
	case Type::Path: stream << "Path"; break;
	case Type::Signature: stream << "Signature"; break;
	case Type::Fd: stream << "Fd"; break;

	/* Compound types */
	case Type::Array: stream << "Array"; break;
	case Type::Variant: stream << "Variant"; break;
	case Type::Struct: stream << "Struct"; break;
	case Type::DictEntry: stream << "DictEntry"; break;
	}
	return stream;
}


} // namespace stappler::xenolith::platform::dbus

#endif

#endif /* XENOLITH_APPLICATION_LINUX_XLLINUXDBUS_H_ */
