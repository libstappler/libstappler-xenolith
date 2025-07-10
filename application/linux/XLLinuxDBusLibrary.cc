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

#include "XLLinuxDBusLibrary.h"

namespace STAPPLER_VERSIONIZED stappler::xenolith::platform {

namespace dbus {

BusFilter::~BusFilter() {
	if (added) {
		if (connection->lib->dbus_error_is_set(&error)) {
			connection->lib->dbus_error_free(&error);
		}
		connection->lib->dbus_bus_remove_match(connection->connection, filter.data(), &error);
		if (connection->lib->dbus_error_is_set(&error)) {
			log::error("DBus", "Fail to remove filter: ", error.name, ": ", error.message);
		}
	}
	if (connection->lib->dbus_error_is_set(&error)) {
		connection->lib->dbus_error_free(&error);
	}
	connection = nullptr;
}

BusFilter::BusFilter(NotNull<Connection> c, StringView filter)
: connection(c), filter(filter.str<Interface>()) {
	connection->lib->dbus_error_init(&error);
	connection->lib->dbus_bus_add_match(connection->connection, filter.data(), &error);
	if (connection->lib->dbus_error_is_set(&error)) {
		log::error("DBus", "Fail to add filter: ", error.name, ": ", error.message);
		connection->lib->dbus_bus_remove_match(connection->connection, filter.data(), nullptr);
	} else {
		added = true;
	}
}

Connection::~Connection() {
	close();
	if (lib->dbus_error_is_set(&error)) {
		lib->dbus_error_free(&error);
	}
}

Connection::Connection(Library *lib, EventCallback &&cb, DBusBusType type)
: lib(lib), callback(sp::move(cb)), type(type) {
	lib->dbus_error_init(&error);
	connection = lib->dbus_bus_get_private(type, &error);
	if (lib->dbus_error_is_set(&error)) {
		log::error("DBus", "Fail to connect: ", error.name, ": ", error.message);
	}

	if (connection) {
		lib->dbus_connection_set_watch_functions(connection, [](DBusWatch *watch, void *data) {
			auto conn = reinterpret_cast<Connection *>(data);
			return conn->callback(conn, Event{Event::AddWatch, {watch}});
		}, [](DBusWatch *watch, void *data) {
			auto conn = reinterpret_cast<Connection *>(data);
			conn->callback(conn, Event{Event::RemoveWatch, {watch}});
		}, [](DBusWatch *watch, void *data) {
			auto conn = reinterpret_cast<Connection *>(data);
			conn->callback(conn, Event{Event::ToggleWatch, {watch}});
		}, this, nullptr);

		lib->dbus_connection_set_timeout_functions(connection,
				[](DBusTimeout *timeout, void *data) {
			auto conn = reinterpret_cast<Connection *>(data);
			return conn->callback(conn, Event{Event::AddTimeout, {.timeout = timeout}});
		}, [](DBusTimeout *timeout, void *data) {
			auto conn = reinterpret_cast<Connection *>(data);
			conn->callback(conn, Event{Event::RemoveTimeout, {.timeout = timeout}});
		}, [](DBusTimeout *timeout, void *data) {
			auto conn = reinterpret_cast<Connection *>(data);
			conn->callback(conn, Event{Event::ToggleTimeout, {.timeout = timeout}});
		}, this, nullptr);

		lib->dbus_connection_set_wakeup_main_function(connection, [](void *data) {
			auto conn = reinterpret_cast<Connection *>(data);
			conn->callback(conn, Event{Event::Wakeup, {nullptr}});
		}, this, nullptr);

		lib->dbus_connection_set_dispatch_status_function(connection,
				[](DBusConnection *connection, DBusDispatchStatus new_status, void *data) {
			auto conn = reinterpret_cast<Connection *>(data);
			if (new_status == DBUS_DISPATCH_DATA_REMAINS) {
				conn->callback(conn, Event{Event::Dispatch, {nullptr}});
			}
		}, this, nullptr);

		lib->dbus_connection_add_filter(connection,
				[](DBusConnection *, DBusMessage *msg, void *data) -> DBusHandlerResult {
			auto conn = reinterpret_cast<Connection *>(data);
			return DBusHandlerResult(conn->callback(conn, Event{Event::Message, {.message = msg}}));
		}, this, nullptr);
	}
}

static void parseServiceList(Library *lib, Set<String> &services, DBusMessage *reply) {
	Type current_type = Type::Invalid;
	DBusMessageIter iter;

	lib->dbus_message_iter_init(reply, &iter);
	while ((current_type = Type(lib->dbus_message_iter_get_arg_type(&iter))) != Type::Invalid) {
		if (current_type == Type::Array) {
			Type sub_type = Type::Invalid;
			DBusMessageIter sub;
			lib->dbus_message_iter_recurse(&iter, &sub);
			while ((sub_type = Type(lib->dbus_message_iter_get_arg_type(&sub))) != Type::Invalid) {
				if (sub_type == Type::String) {
					char *str = nullptr;
					lib->dbus_message_iter_get_basic(&sub, &str);
					if (str && *str != ':') {
						services.emplace(str);
					}
				}
				lib->dbus_message_iter_next(&sub);
			}
		}
		lib->dbus_message_iter_next(&iter);
	}
}

void Connection::setup() {
	callMethod("org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus", "ListNames",
			nullptr, [](NotNull<Connection> c, DBusMessage *reply) {
		parseServiceList(c->lib, c->services, reply);
		c->connected = true;
		c->callback(c, Event{Event::Connected});
	}, this);
}

DBusPendingCall *Connection::callMethod(StringView bus, StringView path, StringView iface,
		StringView method, const Callback<void(DBusMessage *)> &argsCallback,
		Function<void(NotNull<Connection>, DBusMessage *)> &&resultCallback, Ref *ref) {

	struct MessageData {
		Rc<Library> interface;
		Rc<Connection> connection;
		Function<void(NotNull<Connection>, DBusMessage *)> callback;
		Rc<Ref> ref;

		static void parseReply(DBusPendingCall *pending, void *user_data) {
			auto data = reinterpret_cast<MessageData *>(user_data);

			if (data->interface->dbus_pending_call_get_completed(pending)) {
				auto reply = data->interface->dbus_pending_call_steal_reply(pending);
				if (reply) {
					if (data->callback) {
						data->callback(data->connection, reply);
					}
					data->interface->dbus_message_unref(reply);
				}
			}

			data->interface->dbus_pending_call_unref(pending);
		}

		static void freeMessage(void *user_data) {
			auto data = reinterpret_cast<MessageData *>(user_data);
			delete data;
		}
	};

	DBusPendingCall *ret = nullptr;

	auto message =
			lib->dbus_message_new_method_call(bus.data(), path.data(), iface.data(), method.data());

	if (argsCallback) {
		argsCallback(message);
	}

	auto success = lib->dbus_connection_send_with_reply(connection, message, &ret,
			DBUS_TIMEOUT_USE_DEFAULT);

	lib->dbus_message_unref(message);

	if (success && ret) {
		auto data = new MessageData;
		data->interface = lib;
		data->connection = this;
		data->callback = sp::move(resultCallback);
		data->ref = ref;

		lib->dbus_pending_call_set_notify(ret, MessageData::parseReply, data,
				MessageData::freeMessage);
		flush();
	}
	return ret;
}

bool Connection::handle(event::Handle *handle, const Event &ev, event::PollFlags flags) {
	switch (ev.type) {
	case Event::TriggerWatch: return lib->dbus_watch_handle(ev.watch, getWatchFlags(flags)); break;
	case Event::TriggerTimeout: return lib->dbus_timeout_handle(ev.timeout); break;
	default: break;
	}
	return false;
}

void Connection::flush() { lib->dbus_connection_flush(connection); }

DBusDispatchStatus Connection::dispatch() { return lib->dbus_connection_dispatch(connection); }

void Connection::dispatchAll() {
	while (lib->dbus_connection_dispatch(connection) == DBUS_DISPATCH_DATA_REMAINS) {
		// empty
	}
}

void Connection::close() {
	if (lib && connection) {
		lib->dbus_connection_close(connection);
		lib->dbus_connection_unref(connection);
		connection = nullptr;
	}
}

bool Library::init() {
	_handle = Dso("libdbus-1.so");
	if (!_handle) {
		log::error("DBusLibrary", "Fail to open libdbus-1.so");
		return false;
	}

	if (open(_handle)) {
		return true;
	} else {
		_handle = Dso();
	}
	return false;
}

bool Library::open(Dso &handle) {
	XL_LOAD_PROTO(handle, dbus_error_init)
	XL_LOAD_PROTO(handle, dbus_error_free)
	XL_LOAD_PROTO(handle, dbus_message_new_method_call)
	XL_LOAD_PROTO(handle, dbus_message_append_args)
	XL_LOAD_PROTO(handle, dbus_message_is_signal)
	XL_LOAD_PROTO(handle, dbus_message_is_error)
	XL_LOAD_PROTO(handle, dbus_message_unref)
	XL_LOAD_PROTO(handle, dbus_message_iter_init)
	XL_LOAD_PROTO(handle, dbus_message_iter_recurse)
	XL_LOAD_PROTO(handle, dbus_message_iter_next)
	XL_LOAD_PROTO(handle, dbus_message_iter_has_next)
	XL_LOAD_PROTO(handle, dbus_message_iter_get_arg_type)
	XL_LOAD_PROTO(handle, dbus_message_iter_get_element_type)
	XL_LOAD_PROTO(handle, dbus_message_iter_get_element_count)
	XL_LOAD_PROTO(handle, dbus_message_iter_get_fixed_array)
	XL_LOAD_PROTO(handle, dbus_message_iter_get_basic)
	XL_LOAD_PROTO(handle, dbus_message_iter_get_signature)
	XL_LOAD_PROTO(handle, dbus_message_get_type)
	XL_LOAD_PROTO(handle, dbus_message_get_path)
	XL_LOAD_PROTO(handle, dbus_message_get_interface)
	XL_LOAD_PROTO(handle, dbus_message_get_member)
	XL_LOAD_PROTO(handle, dbus_message_get_error_name)
	XL_LOAD_PROTO(handle, dbus_message_get_destination)
	XL_LOAD_PROTO(handle, dbus_message_get_sender)
	XL_LOAD_PROTO(handle, dbus_message_get_signature)
	XL_LOAD_PROTO(handle, dbus_connection_send_with_reply_and_block)
	XL_LOAD_PROTO(handle, dbus_connection_send_with_reply)
	XL_LOAD_PROTO(handle, dbus_connection_set_watch_functions)
	XL_LOAD_PROTO(handle, dbus_connection_set_timeout_functions)
	XL_LOAD_PROTO(handle, dbus_connection_set_wakeup_main_function)
	XL_LOAD_PROTO(handle, dbus_connection_set_dispatch_status_function)
	XL_LOAD_PROTO(handle, dbus_connection_add_filter)
	XL_LOAD_PROTO(handle, dbus_connection_close)
	XL_LOAD_PROTO(handle, dbus_connection_unref)
	XL_LOAD_PROTO(handle, dbus_connection_flush)
	XL_LOAD_PROTO(handle, dbus_connection_dispatch)
	XL_LOAD_PROTO(handle, dbus_error_is_set)
	XL_LOAD_PROTO(handle, dbus_bus_get)
	XL_LOAD_PROTO(handle, dbus_bus_get_private)
	XL_LOAD_PROTO(handle, dbus_bus_add_match)
	XL_LOAD_PROTO(handle, dbus_bus_remove_match)
	XL_LOAD_PROTO(handle, dbus_pending_call_ref)
	XL_LOAD_PROTO(handle, dbus_pending_call_unref)
	XL_LOAD_PROTO(handle, dbus_pending_call_set_notify)
	XL_LOAD_PROTO(handle, dbus_pending_call_get_completed)
	XL_LOAD_PROTO(handle, dbus_pending_call_steal_reply)
	XL_LOAD_PROTO(handle, dbus_pending_call_block)
	XL_LOAD_PROTO(handle, dbus_watch_get_unix_fd)
	XL_LOAD_PROTO(handle, dbus_watch_get_flags)
	XL_LOAD_PROTO(handle, dbus_watch_get_data)
	XL_LOAD_PROTO(handle, dbus_watch_set_data)
	XL_LOAD_PROTO(handle, dbus_watch_handle)
	XL_LOAD_PROTO(handle, dbus_watch_get_enabled)
	XL_LOAD_PROTO(handle, dbus_timeout_get_interval)
	XL_LOAD_PROTO(handle, dbus_timeout_get_data)
	XL_LOAD_PROTO(handle, dbus_timeout_set_data)
	XL_LOAD_PROTO(handle, dbus_timeout_handle)
	XL_LOAD_PROTO(handle, dbus_timeout_get_enabled)

	if (!validateFunctionList(&_dbus_first_fn, &_dbus_last_fn)) {
		log::error("XcbLibrary", "Fail to load libxcb");
		return false;
	}

	return true;
}

void Library::close() { _handle.close(); }

unsigned int getWatchFlags(event::PollFlags events) {
	short flags = 0;
	if (hasFlag(events, event::PollFlags::In)) {
		flags |= DBUS_WATCH_READABLE;
	}
	if (hasFlag(events, event::PollFlags::Out)) {
		flags |= DBUS_WATCH_WRITABLE;
	}
	if (hasFlag(events, event::PollFlags::HungUp)) {
		flags |= DBUS_WATCH_HANGUP;
	}
	if (hasFlag(events, event::PollFlags::Err)) {
		flags |= DBUS_WATCH_ERROR;
	}
	return flags;
}

event::PollFlags getPollFlags(unsigned int flags) {
	event::PollFlags ret = event::PollFlags::None;
	if (flags & DBUS_WATCH_READABLE) {
		ret |= event::PollFlags::In;
	}
	if (flags & DBUS_WATCH_WRITABLE) {
		ret |= event::PollFlags::Out;
	}
	if (flags & DBUS_WATCH_HANGUP) {
		ret |= event::PollFlags::HungUp;
	}
	if (flags & DBUS_WATCH_ERROR) {
		ret |= event::PollFlags::Err;
	}
	return ret;
}


struct MessageDescriptionParser {
	bool onBasicValue(const BasicValue &val);
	bool onArrayBegin(Type type);
	bool onArrayEnd();
	bool onStructBegin(StringView sig);
	bool onStructEnd();
	bool onVariantBegin(StringView sig);
	bool onVariantEnd();
	bool onDictEntryBegin();
	bool onDictEntryEnd();

	const CallbackStream *out = nullptr;
	uint32_t indentLevel = 0;
};

void describe(Library *lib, NotNull<DBusMessage> message, const CallbackStream &out) {
	out << "Header:\n";

	auto iface = lib->dbus_message_get_interface(message);
	if (iface) {
		out << "\tInterface: " << iface << "\n";
	}
	auto path = lib->dbus_message_get_path(message);
	if (path) {
		out << "\tPath: " << path << "\n";
	}
	auto member = lib->dbus_message_get_member(message);
	if (member) {
		out << "\tMember: " << member << "\n";
	}
	auto dest = lib->dbus_message_get_destination(message);
	if (dest) {
		out << "\tDestination: " << dest << "\n";
	}
	auto sender = lib->dbus_message_get_sender(message);
	if (sender) {
		out << "\tSender: " << sender << "\n";
	}

	out << "Data:\n";
	MessageDescriptionParser parser;
	parser.indentLevel = 1;
	parser.out = &out;

	lib->parseMessage(message, parser);
}

void describe(Library *lib, NotNull<DBusMessageIter> message, const CallbackStream &out) {
	MessageDescriptionParser parser;
	parser.out = &out;

	lib->parseMessage(message, parser);
}

SP_PUBLIC bool isFixedType(Type t) {
	switch (t) {
	case Type::Byte:
	case Type::Boolean:
	case Type::Int16:
	case Type::Uint16:
	case Type::Int32:
	case Type::Uint32:
	case Type::Int64:
	case Type::Uint64:
	case Type::Double:
	case Type::Fd: return true; break;
	default: break;
	}
	return false;
}

SP_PUBLIC bool isBasicType(Type t) {
	switch (t) {
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
	case Type::Fd: return true; break;
	default: break;
	}
	return false;
}

SP_PUBLIC bool isContainerType(Type t) {
	switch (t) {
	case Type::Array:
	case Type::Variant:
	case Type::Struct:
	case Type::DictEntry: return true; break;
	default: break;
	}
	return false;
}

bool MessageDescriptionParser::onBasicValue(const BasicValue &val) {
	for (size_t i = 0; i < indentLevel; ++i) { (*out) << "\t"; }
	(*out) << val.type << "(";
	switch (val.type) {
	case Type::Byte: (*out) << int(val.value.byt); break;
	case Type::Boolean: (*out) << (val.value.bool_val ? "true" : "false"); break;
	case Type::Int16: (*out) << val.value.i16; break;
	case Type::Uint16: (*out) << val.value.u16; break;
	case Type::Int32: (*out) << val.value.i32; break;
	case Type::Uint32: (*out) << val.value.u32; break;
	case Type::Int64: (*out) << val.value.i64; break;
	case Type::Uint64: (*out) << val.value.u64; break;
	case Type::Double: (*out) << val.value.dbl; break;
	case Type::String: (*out) << val.value.str; break;
	case Type::Path: (*out) << val.value.str; break;
	case Type::Signature: (*out) << val.value.str; break;
	case Type::Fd: (*out) << int(val.value.fd); break;
	default: break;
	}
	(*out) << ")\n";
	return true;
}
bool MessageDescriptionParser::onArrayBegin(Type type) {
	for (size_t i = 0; i < indentLevel; ++i) { (*out) << "\t"; }
	(*out) << "Array(" << type << ")\n";
	++indentLevel;
	return true;
}
bool MessageDescriptionParser::onArrayEnd() {
	--indentLevel;
	return true;
}
bool MessageDescriptionParser::onStructBegin(StringView sig) {
	for (size_t i = 0; i < indentLevel; ++i) { (*out) << "\t"; }
	(*out) << "Struct(" << sig << ")\n";
	++indentLevel;
	return true;
}
bool MessageDescriptionParser::onStructEnd() {
	--indentLevel;
	return true;
}
bool MessageDescriptionParser::onVariantBegin(StringView sig) {
	for (size_t i = 0; i < indentLevel; ++i) { (*out) << "\t"; }
	(*out) << "Variant(" << sig << ")\n";
	++indentLevel;
	return true;
}
bool MessageDescriptionParser::onVariantEnd() {
	--indentLevel;
	return true;
}
bool MessageDescriptionParser::onDictEntryBegin() {
	for (size_t i = 0; i < indentLevel; ++i) { (*out) << "\t"; }
	(*out) << "DictEntry\n";
	++indentLevel;
	return true;
}
bool MessageDescriptionParser::onDictEntryEnd() {
	--indentLevel;
	return true;
}

struct MessagePropertyParser {
	static bool parse(Library *lib, NotNull<DBusMessageIter> entry, BasicValue &target) {
		MessagePropertyParser parser;
		parser.lib = lib;
		parser.target = &target;
		return lib->parseMessage(entry, parser) && parser.found;
	}

	static bool parse(Library *lib, NotNull<DBusMessageIter> entry, Vector<uint32_t> &target) {
		MessagePropertyParser parser;
		parser.lib = lib;
		parser.u32ArrayTarget = &target;
		return lib->parseMessage(entry, parser) && parser.found;
	}

	static bool parse(Library *lib, NotNull<DBusMessageIter> entry, uint32_t &val) {
		BasicValue ret;
		if (parse(lib, entry, ret)) {
			switch (ret.type) {
			case Type::Byte: val = ret.value.byt; break;
			case Type::Boolean: val = ret.value.bool_val; break;
			case Type::Uint16: val = ret.value.u16; break;
			case Type::Uint32: val = ret.value.u32; break;
			default:
				log::error("DBus", "Fail to read uint32_t property: invalid type");
				return false;
				break;
			}
			return true;
		}
		return false;
	}

	static bool parse(Library *lib, NotNull<DBusMessageIter> entry, const char *&val) {
		BasicValue ret;
		if (parse(lib, entry, ret)) {
			switch (ret.type) {
			case Type::String: val = ret.value.str; break;
			case Type::Path: val = ret.value.str; break;
			case Type::Signature: val = ret.value.str; break;
			default:
				log::error("DBus", "Fail to read string property: invalid type");
				return false;
				break;
			}
			return true;
		}
		return false;
	}

	bool onArray(size_t size, Type type, NotNull<DBusMessageIter> entry) {
		if (!found && u32ArrayTarget && type == Type::Uint32) {
			if (size > 0 && size != maxOf<size_t>()) {
				uint32_t *ptr = nullptr;
				int size = 0;
				lib->dbus_message_iter_get_fixed_array(entry, &ptr, &size);

				u32ArrayTarget->resize(size);
				memcpy(u32ArrayTarget->data(), ptr, sizeof(uint32_t) * size);
				found = true;
				return true;
			}
		}
		return false;
	}

	bool onBasicValue(const BasicValue &val) {
		if (target) {
			if (!found) {
				*target = val;
				found = true;
				return true;
			}
		}
		return false;
	}

	Library *lib = nullptr;
	bool found = false;
	BasicValue *target = nullptr;
	Vector<uint32_t> *u32ArrayTarget = nullptr;
};

struct MessageNetworkStateParser {
	bool onArrayBegin(Type type) { return true; }
	bool onArrayEnd() { return true; }

	bool onDictEntry(const BasicValue &val, NotNull<DBusMessageIter> entry) {
		if (val.type == Type::String) {
			StringView prop(val.value.str);
			if (prop == "NetworkingEnabled") {
				uint32_t value = 0;
				if (MessagePropertyParser::parse(lib, entry, value)) {
					target->networkingEnabled = value ? true : false;
				}
			} else if (prop == "WirelessEnabled") {
				uint32_t value = 0;
				if (MessagePropertyParser::parse(lib, entry, value)) {
					target->wirelessEnabled = value ? true : false;
				}
			} else if (prop == "WwanEnabled") {
				uint32_t value = 0;
				if (MessagePropertyParser::parse(lib, entry, value)) {
					target->wwanEnabled = value ? true : false;
				}
			} else if (prop == "WimaxEnabled") {
				uint32_t value = 0;
				if (MessagePropertyParser::parse(lib, entry, value)) {
					target->wimaxEnabled = value ? true : false;
				}
			} else if (prop == "PrimaryConnectionType") {
				const char *value = nullptr;
				if (MessagePropertyParser::parse(lib, entry, value)) {
					target->primaryConnectionType = String(value);
				}
			} else if (prop == "Metered") {
				uint32_t value = 0;
				if (MessagePropertyParser::parse(lib, entry, value)) {
					target->metered = NMMetered(value);
				}
			} else if (prop == "State") {
				uint32_t value = 0;
				if (MessagePropertyParser::parse(lib, entry, value)) {
					target->state = NMState(value);
				}
			} else if (prop == "Connectivity") {
				uint32_t value = 0;
				if (MessagePropertyParser::parse(lib, entry, value)) {
					target->connectivity = NMConnectivityState(value);
				}
			} else if (prop == "Capabilities") {
				Vector<uint32_t> value;
				if (MessagePropertyParser::parse(lib, entry, value)) {
					target->capabilities = sp::move(value);
				}
			}
		}
		return true;
	}

	Library *lib = nullptr;
	NetworkState *target = nullptr;
};


} // namespace dbus

NetworkState::NetworkState(NotNull<dbus::Library> lib, NotNull<DBusMessage> message) {
	dbus::MessageNetworkStateParser parser;
	parser.lib = lib;
	parser.target = this;

	lib->parseMessage(message, parser);
}

void NetworkState::description(const CallbackStream &out) const {
	out << primaryConnectionType << ": ( ";
	if (networkingEnabled) {
		out << "networking ";
	}
	if (wirelessEnabled) {
		out << "wireless ";
	}
	if (wwanEnabled) {
		out << "wwan ";
	}
	if (wimaxEnabled) {
		out << "wimax ";
	}
	out << ")";

	switch (connectivity) {
	case NM_CONNECTIVITY_UNKNOWN: out << " NM_CONNECTIVITY_UNKNOWN"; break;
	case NM_CONNECTIVITY_NONE: out << " NM_CONNECTIVITY_NONE"; break;
	case NM_CONNECTIVITY_PORTAL: out << " NM_CONNECTIVITY_PORTAL"; break;
	case NM_CONNECTIVITY_LIMITED: out << " NM_CONNECTIVITY_LIMITED"; break;
	case NM_CONNECTIVITY_FULL: out << " NM_CONNECTIVITY_FULL"; break;
	}

	switch (state) {
	case NM_STATE_UNKNOWN: out << " NM_STATE_UNKNOWN"; break;
	case NM_STATE_ASLEEP: out << " NM_STATE_ASLEEP"; break;
	case NM_STATE_DISCONNECTED: out << " NM_STATE_DISCONNECTED"; break;
	case NM_STATE_DISCONNECTING: out << " NM_STATE_DISCONNECTING"; break;
	case NM_STATE_CONNECTING: out << " NM_STATE_CONNECTING"; break;
	case NM_STATE_CONNECTED_LOCAL: out << " NM_STATE_CONNECTED_LOCAL"; break;
	case NM_STATE_CONNECTED_SITE: out << " NM_STATE_CONNECTED_SITE"; break;
	case NM_STATE_CONNECTED_GLOBAL: out << " NM_STATE_CONNECTED_GLOBAL"; break;
	}

	switch (metered) {
	case NM_METERED_UNKNOWN: out << " NM_METERED_UNKNOWN"; break;
	case NM_METERED_YES: out << " NM_METERED_YES"; break;
	case NM_METERED_GUESS_YES: out << " NM_METERED_GUESS_YES"; break;
	case NM_METERED_NO: out << " NM_METERED_NO"; break;
	case NM_METERED_GUESS_NO: out << " NM_METERED_GUESS_NO"; break;
	}

	if (!capabilities.empty()) {
		out << " ( ";
		for (auto &it : capabilities) { out << it << " "; }
		out << ")";
	}
}

} // namespace stappler::xenolith::platform
