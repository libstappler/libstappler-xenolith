/**
 Copyright (c) 2023-2024 Stappler LLC <admin@stappler.dev>

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

#include "XLPlatformLinuxDbus.h"
#include "SPPlatformUnistd.h"
#include "SPThread.h"
#include "SPDso.h"

#include <sys/eventfd.h>
#include <sys/epoll.h>
#include <sys/timerfd.h>

#define NM_DBUS_INTERFACE_NAME "org.freedesktop.NetworkManager"
#define NM_DBUS_SIGNAL_STATE_CHANGED "StateChanged"

typedef enum
{
  DBUS_BUS_SESSION,    /**< The login session bus */
  DBUS_BUS_SYSTEM,     /**< The systemwide bus */
  DBUS_BUS_STARTER     /**< The bus that started us, if any */
} DBusBusType;

typedef enum
{
  DBUS_HANDLER_RESULT_HANDLED,
  DBUS_HANDLER_RESULT_NOT_YET_HANDLED,
  DBUS_HANDLER_RESULT_NEED_MEMORY
} DBusHandlerResult;

typedef enum
{
  DBUS_WATCH_READABLE = 1 << 0,
  DBUS_WATCH_WRITABLE = 1 << 1,
  DBUS_WATCH_ERROR    = 1 << 2,
  DBUS_WATCH_HANGUP   = 1 << 3
  /* Internal to libdbus, there is also _DBUS_WATCH_NVAL in dbus-watch.h */
} DBusWatchFlags;

typedef enum
{
  DBUS_DISPATCH_DATA_REMAINS,  /**< There is more data to potentially convert to messages. */
  DBUS_DISPATCH_COMPLETE,      /**< All currently available data has been processed. */
  DBUS_DISPATCH_NEED_MEMORY    /**< More memory is needed to continue. */
} DBusDispatchStatus;

struct DBusError
{
  const char *name;    /**< public error name field */
  const char *message; /**< public error message field */

  unsigned int dummy1 : 1; /**< placeholder */
  unsigned int dummy2 : 1; /**< placeholder */
  unsigned int dummy3 : 1; /**< placeholder */
  unsigned int dummy4 : 1; /**< placeholder */
  unsigned int dummy5 : 1; /**< placeholder */

  void *padding1; /**< placeholder */
};

typedef struct DBusMessageIter DBusMessageIter;

struct DBusMessageIter
{
  void *dummy1;         /**< Don't use this */
  void *dummy2;         /**< Don't use this */
  uint32_t dummy3; /**< Don't use this */
  int dummy4;           /**< Don't use this */
  int dummy5;           /**< Don't use this */
  int dummy6;           /**< Don't use this */
  int dummy7;           /**< Don't use this */
  int dummy8;           /**< Don't use this */
  int dummy9;           /**< Don't use this */
  int dummy10;          /**< Don't use this */
  int dummy11;          /**< Don't use this */
  int pad1;             /**< Don't use this */
  void *pad2;           /**< Don't use this */
  void *pad3;           /**< Don't use this */
};

typedef uint32_t  dbus_bool_t;
typedef struct DBusMessage DBusMessage;
typedef struct DBusConnection DBusConnection;
typedef struct DBusPendingCall DBusPendingCall;
typedef struct DBusWatch DBusWatch;
typedef struct DBusTimeout DBusTimeout;

typedef dbus_bool_t (*DBusAddWatchFunction)(DBusWatch *watch, void *data);
typedef void (*DBusWatchToggledFunction)(DBusWatch *watch, void *data);
typedef void (*DBusRemoveWatchFunction)(DBusWatch *watch, void *data);
typedef dbus_bool_t (*DBusAddTimeoutFunction)(DBusTimeout *timeout, void *data);
typedef void (*DBusTimeoutToggledFunction)(DBusTimeout *timeout, void *data);
typedef void (*DBusRemoveTimeoutFunction)(DBusTimeout *timeout, void *data);
typedef void (*DBusDispatchStatusFunction)(DBusConnection *connection, DBusDispatchStatus new_status, void *data);
typedef void (*DBusWakeupMainFunction)(void *data);
typedef dbus_bool_t (*DBusAllowUnixUserFunction)(DBusConnection *connection, unsigned long uid, void *data);
typedef dbus_bool_t (*DBusAllowWindowsUserFunction)(DBusConnection *connection, const char *user_sid, void *data);
typedef void (*DBusPendingCallNotifyFunction) (DBusPendingCall *pending, void *user_data);
typedef void (*DBusFreeFunction) (void *memory);
typedef DBusHandlerResult (*DBusHandleMessageFunction) (DBusConnection *connection, DBusMessage *message, void *user_data);

enum DBusTypeWrapper {
	DBUS_TYPE_INVALID     = int('\0'),
	DBUS_TYPE_BYTE        = int( 'y'),
	DBUS_TYPE_BOOLEAN     = int( 'b'),
	DBUS_TYPE_INT16       = int( 'n'),
	DBUS_TYPE_UINT16      = int( 'q'),
	DBUS_TYPE_INT32       = int( 'i'),
	DBUS_TYPE_UINT32      = int( 'u'),
	DBUS_TYPE_INT64       = int( 'x'),
	DBUS_TYPE_UINT64      = int( 't'),
	DBUS_TYPE_DOUBLE      = int( 'd'),
	DBUS_TYPE_STRING      = int( 's'),
	DBUS_TYPE_OBJECT_PATH = int( 'o'),
	DBUS_TYPE_SIGNATURE   = int( 'g'),
	DBUS_TYPE_UNIX_FD     = int( 'h'),

	/* Compound types */
	DBUS_TYPE_ARRAY       = int( 'a'),
	DBUS_TYPE_VARIANT     = int( 'v'),
	DBUS_TYPE_STRUCT      = int( 'r'),
	DBUS_TYPE_DICT_ENTRY  = int( 'e'),
};

enum DBusMessageTypeWrapper {
	DBUS_MESSAGE_TYPE_INVALID		= int(0),
	DBUS_MESSAGE_TYPE_METHOD_CALL	= int(1),
	DBUS_MESSAGE_TYPE_METHOD_RETURN	= int(2),
	DBUS_MESSAGE_TYPE_ERROR			= int(3),
	DBUS_MESSAGE_TYPE_SIGNAL		= int(4),
};

enum NMDeviceType {
	NM_DEVICE_TYPE_UNKNOWN = 0, // unknown device
	NM_DEVICE_TYPE_GENERIC = 14, // generic support for unrecognized device types
	NM_DEVICE_TYPE_ETHERNET = 1, // a wired ethernet device
	NM_DEVICE_TYPE_WIFI = 2, // an 802.11 WiFi device
	NM_DEVICE_TYPE_UNUSED1 = 3, // not used
	NM_DEVICE_TYPE_UNUSED2 = 4, // not used
	NM_DEVICE_TYPE_BT = 5, // a Bluetooth device supporting PAN or DUN access protocols
	NM_DEVICE_TYPE_OLPC_MESH = 6, // an OLPC XO mesh networking device
	NM_DEVICE_TYPE_WIMAX = 7, // an 802.16e Mobile WiMAX broadband device
	NM_DEVICE_TYPE_MODEM = 8, // a modem supporting analog telephone, CDMA/EVDO, GSM/UMTS, or LTE network access protocols
	NM_DEVICE_TYPE_INFINIBAND = 9, // an IP-over-InfiniBand device
	NM_DEVICE_TYPE_BOND = 10, // a bond master interface
	NM_DEVICE_TYPE_VLAN = 11, // an 802.1Q VLAN interface
	NM_DEVICE_TYPE_ADSL = 12, // ADSL modem
	NM_DEVICE_TYPE_BRIDGE = 13, // a bridge master interface
	NM_DEVICE_TYPE_TEAM = 15, // a team master interface
	NM_DEVICE_TYPE_TUN = 16, // a TUN or TAP interface
	NM_DEVICE_TYPE_IP_TUNNEL = 17, // a IP tunnel interface
	NM_DEVICE_TYPE_MACVLAN = 18, // a MACVLAN interface
	NM_DEVICE_TYPE_VXLAN = 19, // a VXLAN interface
	NM_DEVICE_TYPE_VETH = 20, // a VETH interface
};

static constexpr auto DBUS_TIMEOUT_USE_DEFAULT = int(-1);

#if XL_LINK
extern "C" {
void dbus_error_init (DBusError *error);
void dbus_error_free (DBusError *error);
DBusMessage* dbus_message_new_method_call(const char *bus_name, const char *path, const char *iface, const char *method);
dbus_bool_t dbus_message_append_args(DBusMessage *message, int first_arg_type, ...);
dbus_bool_t dbus_message_is_signal(DBusMessage *message, const char *iface, const char *signal_name);
dbus_bool_t dbus_message_is_error(DBusMessage *message, const char *error_name);
void dbus_message_unref(DBusMessage *message);
dbus_bool_t dbus_message_iter_init(DBusMessage *message, DBusMessageIter *iter);
void dbus_message_iter_recurse(DBusMessageIter *iter, DBusMessageIter *sub);
void dbus_message_iter_next(DBusMessageIter *iter);
int dbus_message_iter_get_arg_type(DBusMessageIter *iter);
void dbus_message_iter_get_basic(DBusMessageIter *iter, void *value);
int dbus_message_get_type(DBusMessage *message);
const char* dbus_message_get_path(DBusMessage *message);
const char* dbus_message_get_interface(DBusMessage *message);
const char* dbus_message_get_member(DBusMessage *message);
const char* dbus_message_get_error_name(DBusMessage *message);
const char* dbus_message_get_destination(DBusMessage *message);
const char* dbus_message_get_sender(DBusMessage *message);
const char* dbus_message_get_signature(DBusMessage *message);
DBusMessage* dbus_connection_send_with_reply_and_block(DBusConnection *connection,
		DBusMessage *message, int timeout_milliseconds, DBusError *error);
dbus_bool_t dbus_connection_send_with_reply(DBusConnection *connection,
		DBusMessage *message, DBusPendingCall **pending_return, int timeout_milliseconds);
dbus_bool_t dbus_connection_set_watch_functions(DBusConnection *connection, DBusAddWatchFunction add_function,
		DBusRemoveWatchFunction remove_function, DBusWatchToggledFunction toggled_function,
		void *data, DBusFreeFunction free_data_function);
dbus_bool_t dbus_connection_set_timeout_functions(DBusConnection *connection, DBusAddTimeoutFunction add_function,
		DBusRemoveTimeoutFunction remove_function, DBusTimeoutToggledFunction toggled_function,
		void *data, DBusFreeFunction free_data_function);
void dbus_connection_set_wakeup_main_function(DBusConnection *connection, DBusWakeupMainFunction wakeup_main_function,
		void *data, DBusFreeFunction free_data_function);
void dbus_connection_set_dispatch_status_function(DBusConnection *connection, DBusDispatchStatusFunction function,
		void *data, DBusFreeFunction free_data_function);
dbus_bool_t dbus_connection_add_filter(DBusConnection *connection, DBusHandleMessageFunction function,
		void *user_data, DBusFreeFunction free_data_function);
void dbus_connection_close(DBusConnection *connection);
void dbus_connection_unref(DBusConnection *connection);
void dbus_connection_flush(DBusConnection *connection);
DBusDispatchStatus dbus_connection_dispatch(DBusConnection *connection);
dbus_bool_t dbus_error_is_set(const DBusError *error);
DBusConnection* dbus_bus_get(DBusBusType type, DBusError *error);
DBusConnection* dbus_bus_get_private(DBusBusType type, DBusError *error);
void dbus_bus_add_match(DBusConnection *connection, const char *rule, DBusError *error);
DBusPendingCall* dbus_pending_call_ref(DBusPendingCall *pending);
void dbus_pending_call_unref(DBusPendingCall *pending);
dbus_bool_t dbus_pending_call_set_notify(DBusPendingCall *pending, DBusPendingCallNotifyFunction function,
		void *user_data, DBusFreeFunction free_user_data);
dbus_bool_t dbus_pending_call_get_completed(DBusPendingCall *pending);
DBusMessage* dbus_pending_call_steal_reply(DBusPendingCall *pending);
void dbus_pending_call_block(DBusPendingCall *pending);
int dbus_watch_get_unix_fd(DBusWatch *watch);
unsigned int dbus_watch_get_flags(DBusWatch *watch);
void* dbus_watch_get_data(DBusWatch *watch);
void dbus_watch_set_data(DBusWatch *watch, void *data, DBusFreeFunction free_data_function);
dbus_bool_t dbus_watch_handle(DBusWatch *watch, unsigned int flags);
dbus_bool_t dbus_watch_get_enabled(DBusWatch *watch);
int dbus_timeout_get_interval(DBusTimeout *timeout);
void* dbus_timeout_get_data(DBusTimeout *timeout);
void dbus_timeout_set_data(DBusTimeout *timeout, void *data, DBusFreeFunction free_data_function);
dbus_bool_t dbus_timeout_handle(DBusTimeout *timeout);
dbus_bool_t dbus_timeout_get_enabled(DBusTimeout *timeout);
}
#endif


namespace STAPPLER_VERSIONIZED stappler::xenolith::platform {

struct DBusInterface : public thread::ThreadInterface<Interface> {
	struct Error {
		DBusError error;
		DBusInterface *iface;

		Error(DBusInterface *);
		~Error();

		bool isSet() const;
		void reset();
	};

	struct Connection {
		DBusInterface *iface;
		DBusConnection *connection;

		Connection();
		Connection(DBusInterface *, DBusBusType type);
		~Connection();

		void setup();

		dbus_bool_t addWatch(DBusWatch *);
		void watchToggled(DBusWatch *);
		void removeWatch(DBusWatch *);

		dbus_bool_t addTimeout(DBusTimeout *);
		void timeoutToggled(DBusTimeout *);
		void removeTimeout(DBusTimeout *);

		void wakeup();
		void schedule_dispatch();

		DBusHandlerResult handleMessage(DBusMessage *);

		explicit operator bool() const { return connection != nullptr; }

		void flush();
		DBusDispatchStatus dispatch();
		void dispatchAll();

		void close();

		Connection(Connection &&) = delete;
		Connection &operator=(Connection &&) = delete;
		Connection(const Connection &) = delete;
		Connection &operator=(const Connection &) = delete;
	};

	enum class EventType {
		Watch,
		Timeout,
	};

	struct EventStruct {
		EventType type;
		int fd;
	    struct epoll_event event;
		Connection *connection;
		bool enabled = false;
		union {
			DBusWatch *watch;
			DBusTimeout *timeout;
		};

		static void free(void *);

		void handle(uint32_t ev);
	};

	DBusInterface();
	virtual ~DBusInterface();

	DBusMessage *getSettingSync(Connection &c, const char *key, const char *value, Error &err);
	bool parseType(DBusMessage *const reply, const int type, void *value);

	explicit operator bool () const { return handle ? true : false; }

	bool openHandle(Dso &d);

	bool startThread();

	virtual void threadInit();
	virtual void threadDispose();
	virtual bool worker();

	void wakeup();
	void addEvent(Function<void()> &&);
	void addEvent(EventStruct *);
	void updateEvent(EventStruct *);
	void removeEvent(EventStruct *);

	void handleEvents();
	DBusHandlerResult handleMessage(Connection *, DBusMessage *);
	DBusHandlerResult handleNetworkStateChanged(DBusMessage *);

	DBusPendingCall *readInterfaceTheme(Function<void(InterfaceThemeInfo &&)> &&cb);
	DBusPendingCall *updateNetworkState(Connection &c, Function<void(NetworkState &&)> &&cb);

	DBusPendingCall *loadServiceNames(Connection &c, Set<String> &, Function<void()> &&);

	DBusPendingCall *callMethod(Connection &c, StringView bus, StringView path, StringView iface, StringView method,
			const Callback<void(DBusMessage *)> &, Function<void(DBusMessage *)> &&);

	void parseServiceList(Set<String> &, DBusMessage *);
	NetworkState parseNetworkState(DBusMessage *);
	InterfaceThemeInfo parseInterfaceThemeSettings(DBusMessage *);

	bool parseProperty(DBusMessageIter *entry, void *value, DBusTypeWrapper = DBUS_TYPE_INVALID);

	void setNetworkState(NetworkState &&);

	void addNetworkConnectionCallback(void *key, Function<void(const NetworkState &)> &&);
	void removeNetworkConnectionCallback(void *key);

	InterfaceThemeInfo getCurrentTheme() {
		std::unique_lock lock(interfaceMutex);
		if (interfaceLoaded) {
			return interfaceTheme;
		}
		interfaceCondvar.wait(lock);
		return interfaceTheme;
	}

	void (*dbus_error_init) (DBusError *error) = nullptr;
	void (*dbus_error_free) (DBusError *error) = nullptr;
	DBusMessage* (*dbus_message_new_method_call) (const char *bus_name, const char *path, const char *iface, const char *method) = nullptr;
	dbus_bool_t (*dbus_message_append_args) (DBusMessage *message, int first_arg_type, ...) = nullptr;
	dbus_bool_t (*dbus_message_is_signal) (DBusMessage *message, const char *iface, const char *signal_name) = nullptr;
	dbus_bool_t (*dbus_message_is_error) (DBusMessage *message, const char *error_name) = nullptr;	void (*dbus_message_unref) (DBusMessage *message) = nullptr;
	dbus_bool_t (*dbus_message_iter_init) (DBusMessage *message, DBusMessageIter *iter) = nullptr;
	void (*dbus_message_iter_recurse) (DBusMessageIter *iter, DBusMessageIter *sub) = nullptr;
	void (*dbus_message_iter_next) (DBusMessageIter *iter) = nullptr;
	int (*dbus_message_iter_get_arg_type) (DBusMessageIter *iter) = nullptr;
	void (*dbus_message_iter_get_basic) (DBusMessageIter *iter, void *value) = nullptr;
	int (*dbus_message_get_type)(DBusMessage *message) = nullptr;
	const char* (*dbus_message_get_path) (DBusMessage *message) = nullptr;
	const char* (*dbus_message_get_interface) (DBusMessage *message) = nullptr;
	const char* (*dbus_message_get_member) (DBusMessage *message) = nullptr;
	const char* (*dbus_message_get_error_name) (DBusMessage *message) = nullptr;
	const char* (*dbus_message_get_destination) (DBusMessage *message) = nullptr;
	const char* (*dbus_message_get_sender) (DBusMessage *message) = nullptr;
	const char* (*dbus_message_get_signature) (DBusMessage *message) = nullptr;
	DBusMessage* (*dbus_connection_send_with_reply_and_block) (DBusConnection *connection,
			DBusMessage *message, int timeout_milliseconds, DBusError *error) = nullptr;
	dbus_bool_t (*dbus_connection_send_with_reply) (DBusConnection *connection,
			DBusMessage *message, DBusPendingCall **pending_return, int timeout_milliseconds);
	dbus_bool_t (*dbus_connection_set_watch_functions) (DBusConnection *connection, DBusAddWatchFunction add_function,
			DBusRemoveWatchFunction remove_function, DBusWatchToggledFunction toggled_function,
			void *data, DBusFreeFunction free_data_function) = nullptr;
	dbus_bool_t (*dbus_connection_set_timeout_functions) (DBusConnection *connection, DBusAddTimeoutFunction add_function,
			DBusRemoveTimeoutFunction remove_function, DBusTimeoutToggledFunction toggled_function,
			void *data, DBusFreeFunction free_data_function) = nullptr;
	void (*dbus_connection_set_wakeup_main_function) (DBusConnection *connection, DBusWakeupMainFunction wakeup_main_function,
			void *data, DBusFreeFunction free_data_function) = nullptr;
	void (*dbus_connection_set_dispatch_status_function) (DBusConnection *connection, DBusDispatchStatusFunction function,
			void *data, DBusFreeFunction free_data_function) = nullptr;
	dbus_bool_t (*dbus_connection_add_filter) (DBusConnection *connection, DBusHandleMessageFunction function,
			void *user_data, DBusFreeFunction free_data_function) = nullptr;
	void (*dbus_connection_close) (DBusConnection *connection) = nullptr;
	void (*dbus_connection_unref) (DBusConnection *connection) = nullptr;
	void (*dbus_connection_flush)(DBusConnection *connection) = nullptr;
	DBusDispatchStatus (*dbus_connection_dispatch)(DBusConnection *connection) = nullptr;
	dbus_bool_t (*dbus_error_is_set) (const DBusError *error) = nullptr;
	DBusConnection* (*dbus_bus_get) (DBusBusType type, DBusError *error) = nullptr;
	DBusConnection* (*dbus_bus_get_private) (DBusBusType type, DBusError *error) = nullptr;
	void (*dbus_bus_add_match) (DBusConnection *connection, const char *rule, DBusError *error) = nullptr;
	DBusPendingCall* (*dbus_pending_call_ref) (DBusPendingCall *pending) = nullptr;
	void (*dbus_pending_call_unref) (DBusPendingCall *pending) = nullptr;
	dbus_bool_t (*dbus_pending_call_set_notify) (DBusPendingCall *pending, DBusPendingCallNotifyFunction function,
			void *user_data, DBusFreeFunction free_user_data) = nullptr;
	dbus_bool_t (*dbus_pending_call_get_completed) (DBusPendingCall *pending) = nullptr;
	DBusMessage* (*dbus_pending_call_steal_reply) (DBusPendingCall *pending) = nullptr;
	void (*dbus_pending_call_block) (DBusPendingCall *pending) = nullptr;
	int (*dbus_watch_get_unix_fd) (DBusWatch *watch) = nullptr;
	unsigned int (*dbus_watch_get_flags) (DBusWatch *watch) = nullptr;
	void* (*dbus_watch_get_data) (DBusWatch *watch) = nullptr;
	void (*dbus_watch_set_data) (DBusWatch *watch, void *data, DBusFreeFunction free_data_function) = nullptr;
	dbus_bool_t (*dbus_watch_handle) (DBusWatch *watch, unsigned int flags) = nullptr;
	dbus_bool_t (*dbus_watch_get_enabled) (DBusWatch *watch) = nullptr;
	int (*dbus_timeout_get_interval) (DBusTimeout *timeout) = nullptr;
	void* (*dbus_timeout_get_data) (DBusTimeout *timeout) = nullptr;
	void (*dbus_timeout_set_data) (DBusTimeout *timeout, void *data, DBusFreeFunction free_data_function) = nullptr;
	dbus_bool_t (*dbus_timeout_handle) (DBusTimeout *timeout) = nullptr;
	dbus_bool_t (*dbus_timeout_get_enabled) (DBusTimeout *timeout) = nullptr;

	Dso handle;
	Connection *sessionConnection = nullptr;
	Connection *systemConnection = nullptr;

	bool dbusThreadStarted = false;
	std::thread dbusThread;
	std::atomic_flag shouldExit;

	std::mutex interfaceMutex;
	std::condition_variable interfaceCondvar;
	bool interfaceLoaded = false;
	InterfaceThemeInfo interfaceTheme;

	NetworkState networkState;
	Set<String> sessionServices;
	Set<String> systemServices;

	bool hasDesktopPortal = false;
	bool hasNetworkManager = false;

	int epollFd = -1;
	int eventFd = -1;
    struct epoll_event epollEventFd;

	std::mutex eventMutex;
    Vector<Function<void()>> events;

    struct StateCallback {
    	Function<void(const NetworkState &)> callback;
    	Rc<Ref> ref;
    };

    Map<void *, StateCallback> networkCallbacks;
};

static Rc<DBusInterface> s_connection = Rc<DBusInterface>::alloc();

static uint32_t DBusInterface_watchFlagsToEpoll(unsigned int flags) {
	uint32_t events = 0;
	if (flags & DBUS_WATCH_READABLE)
		events |= EPOLLIN;
	if (flags & DBUS_WATCH_WRITABLE)
		events |= EPOLLOUT;
	return events;
}

static unsigned int DBusInterface_epollToWatchFlags(uint32_t events) {
	short flags = 0;
	if (events & EPOLLIN)
		flags |= DBUS_WATCH_READABLE;
	if (events & EPOLLOUT)
		flags |= DBUS_WATCH_WRITABLE;
	if (events & EPOLLHUP)
		flags |= DBUS_WATCH_HANGUP;
	if (events & EPOLLERR)
		flags |= DBUS_WATCH_ERROR;
	return flags;
}

DBusInterface::Error::Error(DBusInterface *i) : iface(i) {
	iface->dbus_error_init(&error);
}

DBusInterface::Error::~Error() {
	if (iface->dbus_error_is_set(&error)) {
		iface->dbus_error_free(&error);
	}
}

bool DBusInterface::Error::isSet() const {
	return iface->dbus_error_is_set(&error);
}

void DBusInterface::Error::reset() {
	if (iface->dbus_error_is_set(&error)) {
		iface->dbus_error_free(&error);
	}
}

DBusInterface::Connection::Connection() : iface(nullptr), connection(nullptr) { }

DBusInterface::Connection::Connection(DBusInterface *i, DBusBusType type)
: iface(i) {
	Error error(iface);
	connection = iface->dbus_bus_get_private(type, &error.error);
}

DBusInterface::Connection::~Connection() {
	close();
}

static dbus_bool_t DBusInterface_Connection_addWatch(DBusWatch *watch, void *data) {
	auto conn = reinterpret_cast<DBusInterface::Connection *>(data);
	return conn->addWatch(watch);
}

static void DBusInterface_Connection_watchToggled(DBusWatch *watch, void *data) {
	auto conn = reinterpret_cast<DBusInterface::Connection *>(data);
	conn->watchToggled(watch);
}

static void DBusInterface_Connection_removeWatch(DBusWatch *watch, void *data) {
	auto conn = reinterpret_cast<DBusInterface::Connection *>(data);
	conn->removeWatch(watch);
}

static dbus_bool_t DBusInterface_Connection_addTimeout(DBusTimeout *timeout, void *data) {
	auto conn = reinterpret_cast<DBusInterface::Connection *>(data);
	return conn->addTimeout(timeout);
}

static void DBusInterface_Connection_timeoutToggled(DBusTimeout *timeout, void *data) {
	auto conn = reinterpret_cast<DBusInterface::Connection *>(data);
	conn->timeoutToggled(timeout);
}

static void DBusInterface_Connection_removeTimeout(DBusTimeout *timeout, void *data) {
	auto conn = reinterpret_cast<DBusInterface::Connection *>(data);
	conn->removeTimeout(timeout);
}

static void DBusInterface_Connection_wakeupMain(void *data) {
	auto conn = reinterpret_cast<DBusInterface::Connection *>(data);
	conn->wakeup();
}

static void DBusInterface_Connection_dispatchStatus(DBusConnection *connection, DBusDispatchStatus new_status, void *data) {
	if (new_status == DBUS_DISPATCH_DATA_REMAINS) {
		auto conn = reinterpret_cast<DBusInterface::Connection *>(data);
		conn->schedule_dispatch();
	}
}

static DBusHandlerResult DBusInterface_Connection_handleMessage(DBusConnection *connection, DBusMessage *message, void *data) {
	auto conn = reinterpret_cast<DBusInterface::Connection *>(data);
	return conn->handleMessage(message);
}

void DBusInterface::Connection::setup() {
	if (connection) {
		iface->dbus_connection_set_watch_functions(connection, DBusInterface_Connection_addWatch,
				DBusInterface_Connection_watchToggled, DBusInterface_Connection_removeWatch, this, nullptr);
		iface->dbus_connection_set_timeout_functions(connection, DBusInterface_Connection_addTimeout,
				DBusInterface_Connection_timeoutToggled, DBusInterface_Connection_removeTimeout, this, nullptr);
		iface->dbus_connection_set_wakeup_main_function(connection, DBusInterface_Connection_wakeupMain, this, nullptr);
		iface->dbus_connection_set_dispatch_status_function(connection, DBusInterface_Connection_dispatchStatus, this, nullptr);
		iface->dbus_connection_add_filter(connection, DBusInterface_Connection_handleMessage, this, nullptr);
		schedule_dispatch();
	}
}

dbus_bool_t DBusInterface::Connection::addWatch(DBusWatch *watch) {
	auto ev = new EventStruct;
	ev->type = DBusInterface::EventType::Watch;
	ev->fd = iface->dbus_watch_get_unix_fd(watch);
	ev->event.data.ptr = ev;
	ev->event.events = DBusInterface_watchFlagsToEpoll(iface->dbus_watch_get_flags(watch));
	ev->connection = this;
	ev->enabled = false;
	ev->watch = watch;
	iface->dbus_watch_set_data(watch, ev, EventStruct::free);

	if (iface->dbus_watch_get_enabled(watch)) {
		ev->enabled = true;
		iface->addEvent(ev);
	}
	return 1;
}

void DBusInterface::Connection::watchToggled(DBusWatch *watch) {
	auto ev = reinterpret_cast<EventStruct *>(iface->dbus_watch_get_data(watch));
	if (iface->dbus_watch_get_enabled(watch)) {
		if (!ev->enabled) {
			ev->enabled = true;
			iface->addEvent(ev);
		}
	} else {
		if (ev->enabled) {
			iface->removeEvent(ev);
			ev->enabled = false;
		}
	}
}

void DBusInterface::Connection::removeWatch(DBusWatch *watch) {
	auto ev = reinterpret_cast<EventStruct *>(iface->dbus_watch_get_data(watch));
	if (ev->enabled) {
		iface->removeEvent(ev);
		ev->enabled = false;
	}
	iface->dbus_watch_set_data(watch, nullptr, nullptr);
}

dbus_bool_t DBusInterface::Connection::addTimeout(DBusTimeout *timeout) {
	auto ev = new EventStruct;
	ev->type = DBusInterface::EventType::Timeout;

	ev->fd = ::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);

	auto milliseconds = iface->dbus_timeout_get_interval(timeout);

	struct itimerspec initTimer;
	initTimer.it_interval.tv_nsec = 0;
	initTimer.it_interval.tv_sec = 0;
	initTimer.it_value.tv_nsec = (milliseconds % 1000) * 1000 * 1000;
	initTimer.it_value.tv_sec = milliseconds / 1000;

	::timerfd_settime(ev->fd, 0, &initTimer, nullptr);

	ev->event.data.ptr = ev;
	ev->event.events = EPOLLIN;
	ev->connection = this;
	ev->enabled = false;
	ev->timeout = timeout;
	iface->dbus_timeout_set_data(timeout, ev, EventStruct::free);

	if (iface->dbus_timeout_get_enabled(timeout)) {
		ev->enabled = true;
		iface->addEvent(ev);
	}

	return 1;
}

void DBusInterface::Connection::timeoutToggled(DBusTimeout *timeout) {
	auto ev = reinterpret_cast<EventStruct *>(iface->dbus_timeout_get_data(timeout));
	if (iface->dbus_timeout_get_enabled(timeout)) {
		if (!ev->enabled) {
			ev->enabled = true;

			auto milliseconds = iface->dbus_timeout_get_interval(timeout);

			struct itimerspec initTimer;
			initTimer.it_interval.tv_nsec = 0;
			initTimer.it_interval.tv_sec = 0;
			initTimer.it_value.tv_nsec = (milliseconds % 1000) * 1000 * 1000;
			initTimer.it_value.tv_sec = milliseconds / 1000;

			::timerfd_settime(ev->fd, 0, &initTimer, nullptr);

			iface->addEvent(ev);
		}
	} else {
		if (ev->enabled) {
			iface->removeEvent(ev);
			ev->enabled = false;
		}
	}
}

void DBusInterface::Connection::removeTimeout(DBusTimeout *timeout) {
	auto ev = reinterpret_cast<EventStruct *>(iface->dbus_timeout_get_data(timeout));
	if (ev->enabled) {
		iface->removeEvent(ev);
		ev->enabled = false;
	}
	iface->dbus_timeout_set_data(timeout, nullptr, nullptr);
}

void DBusInterface::Connection::wakeup() {
	iface->wakeup();
}

void DBusInterface::Connection::schedule_dispatch() {
	iface->addEvent([this] {
		dispatchAll();
	});
}

DBusHandlerResult DBusInterface::Connection::handleMessage(DBusMessage *message) {
	return iface->handleMessage(this, message);
}

void DBusInterface::Connection::flush() {
	iface->dbus_connection_flush(connection);
}

DBusDispatchStatus DBusInterface::Connection::dispatch() {
	return iface->dbus_connection_dispatch(connection);
}

void DBusInterface::Connection::dispatchAll() {
	while (iface->dbus_connection_dispatch(connection) == DBUS_DISPATCH_DATA_REMAINS) {
		// empty
	}
}

void DBusInterface::Connection::close() {
	if (iface && connection) {
		iface->dbus_connection_close(connection);
		iface->dbus_connection_unref(connection);
		connection = nullptr;
	}
}

void DBusInterface::EventStruct::free(void *obj) {
	EventStruct *ev = reinterpret_cast<EventStruct *>(obj);
	switch (ev->type) {
	case EventType::Watch:
		break;
	case EventType::Timeout:
		::close(ev->fd);
		break;
	}
	delete ev;
}

void DBusInterface::EventStruct::handle(uint32_t ev) {
	switch (type) {
	case EventType::Watch:
		connection->iface->dbus_watch_handle(watch, DBusInterface_epollToWatchFlags(ev));
		break;
	case EventType::Timeout: {
		auto milliseconds = connection->iface->dbus_timeout_get_interval(timeout);
		struct itimerspec initTimer;
		initTimer.it_interval.tv_nsec = 0;
		initTimer.it_interval.tv_sec = 0;
		initTimer.it_value.tv_nsec = (milliseconds % 1000) * 1000 * 1000;
		initTimer.it_value.tv_sec = milliseconds / 1000;

		::timerfd_settime(fd, 0, &initTimer, nullptr);

		connection->iface->dbus_timeout_handle(timeout);
		break;
	}
	}
}

DBusInterface::DBusInterface() {
	epollEventFd.data.fd = -1;

#ifndef XL_LINK
	handle = Dso("libdbus-1.so");
	if (!handle) {
		return;
	}
#endif

	if (openHandle(handle)) {
		sessionConnection = new Connection(this, DBUS_BUS_SESSION);
		if (sessionConnection->connection == nullptr) {
			delete sessionConnection;
			sessionConnection = nullptr;
		}

		systemConnection = new Connection(this, DBUS_BUS_SYSTEM);
		if (systemConnection->connection == nullptr) {
			delete systemConnection;
			systemConnection = nullptr;
		}
	} else {
		handle = Dso();
	}

	if (sessionConnection && systemConnection) {
		startThread();
	} else if (handle) {
		delete sessionConnection;
		delete systemConnection;
	}
}

DBusInterface::~DBusInterface() {
	shouldExit.clear();
	wakeup();
	if (dbusThreadStarted) {
		dbusThread.join();
	}
	if (sessionConnection) {
		delete sessionConnection;
		sessionConnection = nullptr;
	}
	if (systemConnection) {
		delete systemConnection;
		systemConnection = nullptr;
	}
}

bool DBusInterface::openHandle(Dso &d) {
#if XL_LINK
	this->dbus_error_init = &::dbus_error_init;
	this->dbus_error_free = &::dbus_error_free;
	this->dbus_message_new_method_call = &::dbus_message_new_method_call;
	this->dbus_message_append_args = &::dbus_message_append_args;
	this->dbus_message_is_signal = &::dbus_message_is_signal;
	this->dbus_message_is_error = &::dbus_message_is_error;
	this->dbus_message_unref = &::dbus_message_unref;
	this->dbus_message_iter_init = &::dbus_message_iter_init;
	this->dbus_message_iter_next = &::dbus_message_iter_next;
	this->dbus_message_iter_recurse = &::dbus_message_iter_recurse;
	this->dbus_message_iter_get_arg_type = &::dbus_message_iter_get_arg_type;
	this->dbus_message_iter_get_basic = &::dbus_message_iter_get_basic;
	this->dbus_message_get_type = &::dbus_message_get_type;
	this->dbus_message_get_path = &::dbus_message_get_path;
	this->dbus_message_get_interface = &::dbus_message_get_interface;
	this->dbus_message_get_member = &::dbus_message_get_member;
	this->dbus_message_get_error_name = &::dbus_message_get_error_name;
	this->dbus_message_get_destination = &::dbus_message_get_destination;
	this->dbus_message_get_sender = &::dbus_message_get_sender;
	this->dbus_message_get_signature = &::dbus_message_get_signature;
	this->dbus_connection_send_with_reply_and_block = &::dbus_connection_send_with_reply_and_block;
	this->dbus_connection_send_with_reply = &::dbus_connection_send_with_reply;
	this->dbus_connection_set_watch_functions = &::dbus_connection_set_watch_functions;
	this->dbus_connection_set_timeout_functions = &::dbus_connection_set_timeout_functions;
	this->dbus_connection_set_wakeup_main_function = &::dbus_connection_set_wakeup_main_function;
	this->dbus_connection_set_dispatch_status_function = &::dbus_connection_set_dispatch_status_function;
	this->dbus_connection_add_filter = &::dbus_connection_add_filter;
	this->dbus_connection_close = &::dbus_connection_close;
	this->dbus_connection_unref = &::dbus_connection_unref;
	this->dbus_connection_flush = &::dbus_connection_flush;
	this->dbus_connection_dispatch = &::dbus_connection_dispatch;
	this->dbus_error_is_set = &::dbus_error_is_set;
	this->dbus_bus_get = &::dbus_bus_get;
	this->dbus_bus_get_private = &::dbus_bus_get_private;
	this->dbus_bus_add_match = &::dbus_bus_add_match;
	this->dbus_pending_call_ref = &::dbus_pending_call_ref;
	this->dbus_pending_call_unref = &::dbus_pending_call_unref;
	this->dbus_pending_call_set_notify = &::dbus_pending_call_set_notify;
	this->dbus_pending_call_get_completed = &::dbus_pending_call_get_completed;
	this->dbus_pending_call_steal_reply = &::dbus_pending_call_steal_reply;
	this->dbus_pending_call_block = &::dbus_pending_call_block;
	this->dbus_watch_get_unix_fd = &::dbus_watch_get_unix_fd;
	this->dbus_watch_get_flags = &::dbus_watch_get_flags;
	this->dbus_watch_get_data = &::dbus_watch_get_data;
	this->dbus_watch_set_data = &::dbus_watch_set_data;
	this->dbus_watch_handle = &::dbus_watch_handle;
	this->dbus_watch_get_enabled = &::dbus_watch_get_enabled;
	this->dbus_timeout_get_interval = &::dbus_timeout_get_interval;
	this->dbus_timeout_get_data = &::dbus_timeout_get_data;
	this->dbus_timeout_set_data = &::dbus_timeout_set_data;
	this->dbus_timeout_handle = &::dbus_timeout_handle;
	this->dbus_timeout_get_enabled = &::dbus_timeout_get_enabled;
#else
	this->dbus_error_init =
			d.sym<decltype(this->dbus_error_init)>("dbus_error_init");
	this->dbus_error_free =
			d.sym<decltype(this->dbus_error_free)>("dbus_error_free");
	this->dbus_message_new_method_call =
			d.sym<decltype(this->dbus_message_new_method_call)>("dbus_message_new_method_call");
	this->dbus_message_append_args =
			d.sym<decltype(this->dbus_message_append_args)>("dbus_message_append_args");
	this->dbus_message_is_signal =
			d.sym<decltype(this->dbus_message_is_signal)>("dbus_message_is_signal");
	this->dbus_message_is_error =
			d.sym<decltype(this->dbus_message_is_error)>("dbus_message_is_error");
	this->dbus_message_unref =
			d.sym<decltype(this->dbus_message_unref)>("dbus_message_unref");
	this->dbus_message_iter_init =
			d.sym<decltype(this->dbus_message_iter_init)>("dbus_message_iter_init");
	this->dbus_message_iter_next =
			d.sym<decltype(this->dbus_message_iter_next)>("dbus_message_iter_next");
	this->dbus_message_iter_recurse =
			d.sym<decltype(this->dbus_message_iter_recurse)>("dbus_message_iter_recurse");
	this->dbus_message_iter_get_arg_type =
			d.sym<decltype(this->dbus_message_iter_get_arg_type)>("dbus_message_iter_get_arg_type");
	this->dbus_message_iter_get_basic =
			d.sym<decltype(this->dbus_message_iter_get_basic)>("dbus_message_iter_get_basic");
	this->dbus_message_get_type =
			d.sym<decltype(this->dbus_message_get_type)>("dbus_message_get_type");
	this->dbus_message_get_path =
			d.sym<decltype(this->dbus_message_get_path)>("dbus_message_get_path");
	this->dbus_message_get_interface =
			d.sym<decltype(this->dbus_message_get_interface)>("dbus_message_get_interface");
	this->dbus_message_get_member =
			d.sym<decltype(this->dbus_message_get_member)>("dbus_message_get_member");
	this->dbus_message_get_error_name =
			d.sym<decltype(this->dbus_message_get_error_name)>("dbus_message_get_error_name");
	this->dbus_message_get_destination =
			d.sym<decltype(this->dbus_message_get_destination)>("dbus_message_get_destination");
	this->dbus_message_get_sender =
			d.sym<decltype(this->dbus_message_get_sender)>("dbus_message_get_sender");
	this->dbus_message_get_signature =
			d.sym<decltype(this->dbus_message_get_signature)>("dbus_message_get_signature");
	this->dbus_connection_send_with_reply_and_block =
			d.sym<decltype(this->dbus_connection_send_with_reply_and_block)>("dbus_connection_send_with_reply_and_block");
	this->dbus_connection_send_with_reply =
			d.sym<decltype(this->dbus_connection_send_with_reply)>("dbus_connection_send_with_reply");
	this->dbus_connection_set_watch_functions =
			d.sym<decltype(this->dbus_connection_set_watch_functions)>("dbus_connection_set_watch_functions");
	this->dbus_connection_set_timeout_functions =
			d.sym<decltype(this->dbus_connection_set_timeout_functions)>("dbus_connection_set_timeout_functions");
	this->dbus_connection_set_wakeup_main_function =
			d.sym<decltype(this->dbus_connection_set_wakeup_main_function)>("dbus_connection_set_wakeup_main_function");
	this->dbus_connection_set_dispatch_status_function =
			d.sym<decltype(this->dbus_connection_set_dispatch_status_function)>("dbus_connection_set_dispatch_status_function");
	this->dbus_connection_add_filter =
			d.sym<decltype(this->dbus_connection_add_filter)>("dbus_connection_add_filter");
	this->dbus_connection_close =
			d.sym<decltype(this->dbus_connection_close)>("dbus_connection_close");
	this->dbus_connection_unref =
			d.sym<decltype(this->dbus_connection_unref)>("dbus_connection_unref");
	this->dbus_connection_flush =
			d.sym<decltype(this->dbus_connection_flush)>("dbus_connection_flush");
	this->dbus_connection_dispatch =
			d.sym<decltype(this->dbus_connection_dispatch)>("dbus_connection_dispatch");
	this->dbus_error_is_set =
			d.sym<decltype(this->dbus_error_is_set)>("dbus_error_is_set");
	this->dbus_bus_get =
			d.sym<decltype(this->dbus_bus_get)>("dbus_bus_get");
	this->dbus_bus_get_private =
			d.sym<decltype(this->dbus_bus_get_private)>("dbus_bus_get_private");
	this->dbus_bus_add_match =
			d.sym<decltype(this->dbus_bus_add_match)>("dbus_bus_add_match");
	this->dbus_pending_call_ref =
			d.sym<decltype(this->dbus_pending_call_ref)>("dbus_pending_call_ref");
	this->dbus_pending_call_unref =
			d.sym<decltype(this->dbus_pending_call_unref)>("dbus_pending_call_unref");
	this->dbus_pending_call_set_notify =
			d.sym<decltype(this->dbus_pending_call_set_notify)>("dbus_pending_call_set_notify");
	this->dbus_pending_call_get_completed =
			d.sym<decltype(this->dbus_pending_call_get_completed)>("dbus_pending_call_get_completed");
	this->dbus_pending_call_steal_reply =
			d.sym<decltype(this->dbus_pending_call_steal_reply)>("dbus_pending_call_steal_reply");
	this->dbus_pending_call_block =
			d.sym<decltype(this->dbus_pending_call_block)>("dbus_pending_call_block");
	this->dbus_watch_get_unix_fd =
			d.sym<decltype(this->dbus_watch_get_unix_fd)>("dbus_watch_get_unix_fd");
	this->dbus_watch_get_flags =
			d.sym<decltype(this->dbus_watch_get_flags)>("dbus_watch_get_flags");
	this->dbus_watch_get_data =
			d.sym<decltype(this->dbus_watch_get_data)>("dbus_watch_get_data");
	this->dbus_watch_set_data =
			d.sym<decltype(this->dbus_watch_set_data)>("dbus_watch_set_data");
	this->dbus_watch_handle =
			d.sym<decltype(this->dbus_watch_handle)>("dbus_watch_handle");
	this->dbus_watch_get_enabled =
			d.sym<decltype(this->dbus_watch_get_enabled)>("dbus_watch_get_enabled");
	this->dbus_timeout_get_interval =
			d.sym<decltype(this->dbus_timeout_get_interval)>("dbus_timeout_get_interval");
	this->dbus_timeout_get_data =
			d.sym<decltype(this->dbus_timeout_get_data)>("dbus_timeout_get_data");
	this->dbus_timeout_set_data =
			d.sym<decltype(this->dbus_timeout_set_data)>("dbus_timeout_set_data");
	this->dbus_timeout_handle =
			d.sym<decltype(this->dbus_timeout_handle)>("dbus_timeout_handle");
	this->dbus_timeout_get_enabled =
			d.sym<decltype(this->dbus_timeout_get_enabled)>("dbus_timeout_get_enabled");
#endif

	return this->dbus_error_init
		&& this->dbus_error_free
		&& this->dbus_message_new_method_call
		&& this->dbus_message_append_args
		&& this->dbus_message_unref
		&& this->dbus_message_iter_init
		&& this->dbus_message_iter_next
		&& this->dbus_message_iter_recurse
		&& this->dbus_message_iter_get_arg_type
		&& this->dbus_message_iter_get_basic
		&& this->dbus_message_get_type
		&& this->dbus_message_get_path
		&& this->dbus_message_get_interface
		&& this->dbus_message_get_member
		&& this->dbus_message_get_error_name
		&& this->dbus_message_get_destination
		&& this->dbus_message_get_sender
		&& this->dbus_message_get_signature
		&& this->dbus_connection_send_with_reply_and_block
		&& this->dbus_connection_send_with_reply
		&& this->dbus_connection_set_watch_functions
		&& this->dbus_connection_set_timeout_functions
		&& this->dbus_connection_set_wakeup_main_function
		&& this->dbus_connection_set_dispatch_status_function
		&& this->dbus_connection_add_filter
		&& this->dbus_connection_close
		&& this->dbus_connection_unref
		&& this->dbus_connection_flush
		&& this->dbus_connection_dispatch
		&& this->dbus_error_is_set
		&& this->dbus_bus_get
		&& this->dbus_bus_get_private
		&& this->dbus_bus_add_match
		&& this->dbus_pending_call_ref
		&& this->dbus_pending_call_unref
		&& this->dbus_pending_call_set_notify
		&& this->dbus_pending_call_get_completed
		&& this->dbus_pending_call_steal_reply
		&& this->dbus_watch_get_unix_fd
		&& this->dbus_watch_get_flags
		&& this->dbus_watch_get_data
		&& this->dbus_watch_set_data
		&& this->dbus_watch_handle
		&& this->dbus_watch_get_enabled
		&& this->dbus_timeout_get_interval
		&& this->dbus_timeout_get_data
		&& this->dbus_timeout_set_data
		&& this->dbus_timeout_handle
		&& this->dbus_timeout_get_enabled;
}

bool DBusInterface::startThread() {
	dbusThreadStarted = true;
	shouldExit.test_and_set();
	dbusThread = std::thread(DBusInterface::workerThread, this, nullptr);
	return true;
}

void DBusInterface::threadInit() {
	thread::ThreadInfo::setThreadInfo("DBusThread");

	eventFd = ::eventfd(0, EFD_NONBLOCK);
	epollEventFd.events = EPOLLIN | EPOLLET | EPOLLEXCLUSIVE;
	epollEventFd.data.ptr = this;

	epollFd = ::epoll_create1(0);

	int err = ::epoll_ctl(epollFd, EPOLL_CTL_ADD, eventFd, &epollEventFd);
	if (err == -1) {
		char buf[256] = { 0 };
		log::error("DBusInterface", "Fail to add eventFd with EPOLL_CTL_ADD: ", strerror_r(errno, buf, 255));
	}

	sessionConnection->setup();
	systemConnection->setup();

	addEvent([this] {
		loadServiceNames(*sessionConnection, sessionServices, [this] {
			log::verbose("DBusInterface", "Session bus loaded");
			if (sessionServices.find("org.freedesktop.portal.Desktop") != sessionServices.end()) {
				hasDesktopPortal = true;
				readInterfaceTheme([this] (InterfaceThemeInfo &&theme) {
					std::unique_lock lock(interfaceMutex);
					interfaceTheme = move(theme);
					interfaceLoaded = true;
					interfaceCondvar.notify_all();
				});
			} else {
				std::unique_lock lock(interfaceMutex);
				interfaceLoaded = true;
				interfaceCondvar.notify_all();
			}
		});
	});

	addEvent([this] {
		loadServiceNames(*systemConnection, systemServices, [this] {
			log::verbose("DBusInterface", "System bus loaded");
			if (systemServices.find("org.freedesktop.NetworkManager") != systemServices.end()) {
				Error error(this);
				hasNetworkManager = true;
				dbus_bus_add_match(systemConnection->connection, "type='signal',interface='"
						NM_DBUS_INTERFACE_NAME "'", &error.error);
				// see signals from the given interface
				systemConnection->flush();
				if (error.isSet()) {
					log::error("DBusConnection", "fail to add signal match: ", error.error.message);
				}

				updateNetworkState(*systemConnection, [this] (NetworkState &&state) {
					setNetworkState(move(state));
				});
			}
		});
	});
}

void DBusInterface::threadDispose() {
	if (eventFd >= 0) {
		::close(eventFd);
		eventFd = -1;
	}
	if (epollFd >= 0) {
		::close(epollFd);
		epollFd = -1;
	}
}

bool DBusInterface::worker() {
	std::array<struct epoll_event, 16> _events;

	while (shouldExit.test_and_set()) {
		int nevents = epoll_wait(epollFd, _events.data(), 16, 100);
		if (nevents == -1 && errno != EINTR) {
			char buf[256] = { 0 };
			log::error("DBusConnection", "epoll_wait() failed with errno ", errno, " (", strerror_r(errno, buf, 255), ")");
			break;
		} else if (errno == EINTR) {
			continue;
		}

		for (int i = 0; i < nevents; i++) {
			if (_events[i].data.ptr == this && (_events[i].events & EPOLLIN)) {
				// wakeup trigger
				handleEvents();
			} else {
				EventStruct *event = reinterpret_cast<EventStruct *>(_events[i].data.ptr);
				event->handle(_events[i].events);
			}
		}
	}
	return false;
}

void DBusInterface::wakeup() {
	addEvent([] () {}); // add empty function for event
}

void DBusInterface::addEvent(Function<void()> &&ev) {
	std::unique_lock lock(eventMutex);
	events.emplace_back(move(ev));
	lock.unlock();

	uint64_t value = 1;
	if (::write(eventFd, &value, sizeof(uint64_t)) != sizeof(uint64_t)) {
		char buf[256] = { 0 };
		log::error("DBusInterface", "Fail to send dbus event with write: ", strerror_r(errno, buf, 255));
	}
}

void DBusInterface::addEvent(EventStruct *ev) {
	int err = ::epoll_ctl(epollFd, EPOLL_CTL_ADD, ev->fd, &ev->event);
	if (err == -1) {
		char buf[256] = { 0 };
		log::error("DBusInterface", "Fail to add event with EPOLL_CTL_ADD: ", strerror_r(errno, buf, 255));
	}
}

void DBusInterface::updateEvent(EventStruct *ev) {
	int err = ::epoll_ctl(epollFd, EPOLL_CTL_MOD, ev->fd, &ev->event);
	if (err == -1) {
		char buf[256] = { 0 };
		log::error("DBusInterface", "Fail to add event with EPOLL_CTL_ADD: ", strerror_r(errno, buf, 255));
	}
}

void DBusInterface::removeEvent(EventStruct *ev) {
	int err = ::epoll_ctl(epollFd, EPOLL_CTL_DEL, ev->fd, &ev->event);
	if (err == -1) {
		char buf[256] = { 0 };
		log::error("DBusInterface", "Fail to add event with EPOLL_CTL_ADD: ", strerror_r(errno, buf, 255));
	}
}

void DBusInterface::handleEvents() {
	uint64_t value = 0;
	auto sz = ::read(eventFd, &value, sizeof(uint64_t));
	if (sz == 8 && value) {
		std::unique_lock lock(eventMutex);
		auto data = move(events);
		events.clear();
		lock.unlock();

		for (auto &it : data) {
			it();
		}
	}
}

DBusHandlerResult DBusInterface::handleMessage(Connection *, DBusMessage *message) {
	auto type = DBusMessageTypeWrapper(dbus_message_get_type(message));
	if (dbus_message_is_signal(message, NM_DBUS_INTERFACE_NAME, NM_DBUS_SIGNAL_STATE_CHANGED)) {
		return handleNetworkStateChanged(message);
	} else if (type == DBUS_MESSAGE_TYPE_ERROR) {
		log::verbose("DBusInterface", "DBUS_MESSAGE_TYPE_ERROR");
	}
	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

DBusHandlerResult DBusInterface::handleNetworkStateChanged(DBusMessage *message) {
	updateNetworkState(*systemConnection, [this] (NetworkState &&state) {
		setNetworkState(move(state));
	});
	return DBUS_HANDLER_RESULT_HANDLED;
}

DBusPendingCall *DBusInterface::readInterfaceTheme(Function<void(InterfaceThemeInfo &&)> &&cb) {
	return callMethod(*sessionConnection, "org.freedesktop.portal.Desktop",
			"/org/freedesktop/portal/desktop", "org.freedesktop.portal.Settings", "ReadAll",
			[&, this] (DBusMessage *message) {
		const char * array[] = { "org.gnome.desktop.interface" };
		const char **v_ARRAY = array;

		dbus_message_append_args(message,
				DBUS_TYPE_ARRAY, DBUS_TYPE_STRING, &v_ARRAY, 1, DBUS_TYPE_INVALID);
	}, [this, cb = move(cb)] (DBusMessage *reply) {
		cb(parseInterfaceThemeSettings(reply));
	});
}

DBusPendingCall *DBusInterface::updateNetworkState(Connection &c, Function<void(NetworkState &&)> &&cb) {
	return callMethod(c, "org.freedesktop.NetworkManager",
			"/org/freedesktop/NetworkManager", "org.freedesktop.DBus.Properties", "GetAll",
			[this] (DBusMessage *msg) {
		const char *interface_name = "org.freedesktop.NetworkManager";
		dbus_message_append_args(msg, DBUS_TYPE_STRING, &interface_name, DBUS_TYPE_INVALID);
	}, [this, cb = move(cb)] (DBusMessage *reply) {
		cb(parseNetworkState(reply));
	});
}

DBusMessage *DBusInterface::getSettingSync(Connection &c, const char *key, const char *value, Error &err) {
	dbus_bool_t success;
	DBusMessage *message;
	DBusMessage *reply;

	message = this->dbus_message_new_method_call("org.freedesktop.portal.Desktop",
			"/org/freedesktop/portal/desktop", "org.freedesktop.portal.Settings", "Read");

	success = this->dbus_message_append_args(message, DBUS_TYPE_STRING, &key, DBUS_TYPE_STRING, &value, DBUS_TYPE_INVALID);

	if (!success) {
		return NULL;
	}

	reply = this->dbus_connection_send_with_reply_and_block(c.connection, message, DBUS_TIMEOUT_USE_DEFAULT, &err.error);

	this->dbus_message_unref(message);

	if (err.isSet()) {
		return NULL;
	}

	return reply;
}

bool DBusInterface::parseType(DBusMessage *const reply, const int type, void *value) {
	DBusTypeWrapper currentType;
	DBusMessageIter iter[3];

	this->dbus_message_iter_init(reply, &iter[0]);
	currentType = DBusTypeWrapper(this->dbus_message_iter_get_arg_type(&iter[0]));
	if (currentType != DBUS_TYPE_VARIANT)
		return false;

	this->dbus_message_iter_recurse(&iter[0], &iter[1]);
	currentType = DBusTypeWrapper(this->dbus_message_iter_get_arg_type(&iter[1]));
	if (currentType != DBUS_TYPE_VARIANT)
		return false;

	this->dbus_message_iter_recurse(&iter[1], &iter[2]);
	currentType = DBusTypeWrapper(this->dbus_message_iter_get_arg_type(&iter[2]));
	if (currentType != type)
		return false;

	this->dbus_message_iter_get_basic(&iter[2], value);

	return true;
}

struct MessageData {
	DBusInterface *interface;
	DBusInterface::Connection *connection;
	Function<void(DBusMessage *)> callback;

	static void parseReply(DBusPendingCall *pending, void *user_data) {
		auto data = reinterpret_cast<MessageData *>(user_data);

		if (data->interface->dbus_pending_call_get_completed(pending)) {
			auto reply = data->interface->dbus_pending_call_steal_reply(pending);
			if (reply) {

				if (data->callback) {
					data->callback(reply);
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

DBusPendingCall *DBusInterface::loadServiceNames(Connection &c, Set<String> &services, Function<void()> &&cb) {
	return callMethod(c, "org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus", "ListNames",
			nullptr, [this, services = &services, cb = move(cb)] (DBusMessage *reply) {
		parseServiceList(*services, reply);
		cb();
	});
}

DBusPendingCall *DBusInterface::callMethod(Connection &c, StringView bus, StringView path, StringView iface, StringView method,
		const Callback<void(DBusMessage *)> &argsCallback, Function<void(DBusMessage *)> &&resultCallback) {
	DBusPendingCall *ret = nullptr;

	auto message = this->dbus_message_new_method_call(bus.data(),
			path.data(), iface.data(), method.data());

	if (argsCallback) {
		argsCallback(message);
	}

	auto success = this->dbus_connection_send_with_reply(c.connection, message, &ret, DBUS_TIMEOUT_USE_DEFAULT);
	this->dbus_message_unref(message);

	if (success && ret) {
		auto data = new MessageData;
		data->interface = this;
		data->connection = &c;
		data->callback = move(resultCallback);

		this->dbus_pending_call_set_notify(ret, MessageData::parseReply, data, MessageData::freeMessage);
		c.flush();
	}
	return ret;
}

void DBusInterface::parseServiceList(Set<String> &services, DBusMessage *reply) {
	DBusTypeWrapper current_type = DBUS_TYPE_INVALID;
	DBusMessageIter iter;

	dbus_message_iter_init(reply, &iter);
	while ((current_type = DBusTypeWrapper(dbus_message_iter_get_arg_type(&iter))) != DBUS_TYPE_INVALID) {
		if (current_type == DBUS_TYPE_ARRAY) {
			DBusTypeWrapper sub_type = DBUS_TYPE_INVALID;
			DBusMessageIter sub;
			dbus_message_iter_recurse(&iter, &sub);
			while ((sub_type = DBusTypeWrapper(dbus_message_iter_get_arg_type(&sub))) != DBUS_TYPE_INVALID) {
				if (sub_type == DBUS_TYPE_STRING) {
					char *str = nullptr;
					dbus_message_iter_get_basic(&sub, &str);
					if (str && *str != ':') {
						services.emplace(str);
					}
				}
				dbus_message_iter_next(&sub);
			}
		}
		dbus_message_iter_next(&iter);
	}
}

NetworkState DBusInterface::parseNetworkState(DBusMessage *reply) {
	NetworkState ret;

	auto readEntry = [&, this] (DBusMessageIter *entry) {
		DBusTypeWrapper entry_type = DBusTypeWrapper(dbus_message_iter_get_arg_type(entry));

		if (entry_type == DBUS_TYPE_STRING) {
			char *name = nullptr;
			dbus_message_iter_get_basic(entry, &name);
			StringView prop(name);
			if (prop == "NetworkingEnabled") {
				uint32_t value = 0;
				if (parseProperty(entry, &value)) {
					ret.networkingEnabled = value ? true : false;
				}
			} else if (prop == "WirelessEnabled") {
				uint32_t value = 0;
				if (parseProperty(entry, &value)) {
					ret.wirelessEnabled = value ? true : false;
				}
			} else if (prop == "WwanEnabled") {
				uint32_t value = 0;
				if (parseProperty(entry, &value)) {
					ret.wwanEnabled = value ? true : false;
				}
			} else if (prop == "WimaxEnabled") {
				uint32_t value = 0;
				if (parseProperty(entry, &value)) {
					ret.wimaxEnabled = value ? true : false;
				}
			} else if (prop == "PrimaryConnectionType") {
				char *value = nullptr;
				if (parseProperty(entry, &value)) {
					ret.primaryConnectionType = String(value);
				}
			} else if (prop == "Metered") {
				uint32_t value = 0;
				if (parseProperty(entry, &value)) {
					ret.metered = NMMetered(value);
				}
			} else if (prop == "State") {
				uint32_t value = 0;
				if (parseProperty(entry, &value)) {
					ret.state = NMState(value);
				}
			} else if (prop == "Connectivity") {
				uint32_t value = 0;
				if (parseProperty(entry, &value)) {
					ret.connectivity = NMConnectivityState(value);
				}
			} else if (prop == "Capabilities") {
				Vector<uint32_t> value;
				if (parseProperty(entry, &value)) {
					ret.capabilities = move(value);
				}
			}
		}
	};

	DBusTypeWrapper current_type = DBUS_TYPE_INVALID;
	DBusMessageIter iter;

	dbus_message_iter_init(reply, &iter);
	while ((current_type = DBusTypeWrapper(dbus_message_iter_get_arg_type(&iter))) != DBUS_TYPE_INVALID) {
		if (current_type == DBUS_TYPE_ARRAY) {
			DBusTypeWrapper sub_type = DBUS_TYPE_INVALID;
			DBusMessageIter sub;
			dbus_message_iter_recurse(&iter, &sub);
			while ((sub_type = DBusTypeWrapper(dbus_message_iter_get_arg_type(&sub))) != DBUS_TYPE_INVALID) {
				if (sub_type == DBUS_TYPE_DICT_ENTRY) {
					DBusMessageIter entry;
					dbus_message_iter_recurse(&sub, &entry);
					readEntry(&entry);
				}
				dbus_message_iter_next(&sub);
			}
		}
		dbus_message_iter_next(&iter);
	}

	return ret;
}

InterfaceThemeInfo DBusInterface::parseInterfaceThemeSettings(DBusMessage *reply) {
	InterfaceThemeInfo ret;

	auto readEntry = [&, this] (DBusMessageIter *entry) {
		DBusTypeWrapper entry_type = DBusTypeWrapper(dbus_message_iter_get_arg_type(entry));

		if (entry_type == DBUS_TYPE_STRING) {
			char *name = nullptr;
			dbus_message_iter_get_basic(entry, &name);
			StringView prop(name);
			if (prop == "cursor-size") {
				uint32_t value = 0;
				if (parseProperty(entry, &value)) {
					ret.cursorSize = value;
				}
			} else if (prop == "cursor-theme") {
				char *value = nullptr;
				if (parseProperty(entry, &value)) {
					ret.cursorTheme = value;
				}
			}
		}
	};

	auto readNamespaceEntry = [&, this] (DBusMessageIter *entry) {
		DBusTypeWrapper entry_type = DBusTypeWrapper(dbus_message_iter_get_arg_type(entry));

		if (entry_type == DBUS_TYPE_STRING) {
			char *name = nullptr;
			dbus_message_iter_get_basic(entry, &name);
			StringView prop(name);
			if (prop == "org.gnome.desktop.interface") {
				dbus_message_iter_next(entry);
				if (DBusTypeWrapper(dbus_message_iter_get_arg_type(entry)) == DBUS_TYPE_ARRAY) {
					DBusTypeWrapper sub_type = DBUS_TYPE_INVALID;
					DBusMessageIter sub;
					dbus_message_iter_recurse(entry, &sub);
					while ((sub_type = DBusTypeWrapper(dbus_message_iter_get_arg_type(&sub))) != DBUS_TYPE_INVALID) {
						if (sub_type == DBUS_TYPE_DICT_ENTRY) {
							DBusMessageIter dict;
							dbus_message_iter_recurse(&sub, &dict);
							readEntry(&dict);
						}
						dbus_message_iter_next(&sub);
					}
				}
			}
		}
	};

	DBusTypeWrapper current_type = DBUS_TYPE_INVALID;
	DBusMessageIter iter;

	dbus_message_iter_init(reply, &iter);
	while ((current_type = DBusTypeWrapper(dbus_message_iter_get_arg_type(&iter))) != DBUS_TYPE_INVALID) {
		if (current_type == DBUS_TYPE_ARRAY) {
			DBusTypeWrapper sub_type = DBUS_TYPE_INVALID;
			DBusMessageIter sub;
			dbus_message_iter_recurse(&iter, &sub);
			while ((sub_type = DBusTypeWrapper(dbus_message_iter_get_arg_type(&sub))) != DBUS_TYPE_INVALID) {
				if (sub_type == DBUS_TYPE_DICT_ENTRY) {
					DBusMessageIter entry;
					dbus_message_iter_recurse(&sub, &entry);
					readNamespaceEntry(&entry);
				}
				dbus_message_iter_next(&sub);
			}
		}
		dbus_message_iter_next(&iter);
	}

	return ret;
}

bool DBusInterface::parseProperty(DBusMessageIter *entry, void *value, DBusTypeWrapper expectedType) {
	dbus_message_iter_next(entry);
	DBusTypeWrapper type = DBusTypeWrapper(dbus_message_iter_get_arg_type(entry));
	if (type == DBUS_TYPE_VARIANT) {
		DBusMessageIter prop;
		dbus_message_iter_recurse(entry, &prop);
		DBusTypeWrapper propType = DBusTypeWrapper(dbus_message_iter_get_arg_type(&prop));
		if (expectedType == DBUS_TYPE_INVALID || expectedType == propType) {
			switch (propType) {
			case DBUS_TYPE_BOOLEAN:
			case DBUS_TYPE_STRING:
			case DBUS_TYPE_UINT32:
				dbus_message_iter_get_basic(&prop, value);
				return true;
				break;
			case DBUS_TYPE_ARRAY: {
				DBusTypeWrapper sub_type = DBUS_TYPE_INVALID;
				DBusMessageIter sub;
				dbus_message_iter_recurse(&prop, &sub);
				while ((sub_type = DBusTypeWrapper(dbus_message_iter_get_arg_type(&sub))) != DBUS_TYPE_INVALID) {
					auto target = reinterpret_cast<Vector<uint32_t> *>(value);
					switch (sub_type) {
					case DBUS_TYPE_UINT32: {
						uint32_t type = 0;
						dbus_message_iter_get_basic(&sub, &type);
						target->emplace_back(type);
						break;
					}
					default:
						break;
					}
					dbus_message_iter_next(&sub);
				}
				return true;
				break;
			}
			default:
				break;
			}
		}
	}
	return false;
}

void DBusInterface::setNetworkState(NetworkState &&state) {
	if (networkState != state) {
		networkState = move(state);
#if DEBUG
		log::debug("DBusInterface", "Network: ", networkState.description());
#endif
	}
}

void DBusInterface::addNetworkConnectionCallback(void *key, Function<void(const NetworkState &)> &&cb) {
	addEvent([this, cb = move(cb), key] () mutable {
		cb(networkState);
		networkCallbacks.emplace(key, StateCallback{move(cb), this});
	});
}

void DBusInterface::removeNetworkConnectionCallback(void *key) {
	addEvent([this, key] () mutable {
		auto refId = retain();
		networkCallbacks.erase(key);
		release(refId);
	});
}

String NetworkState::description() const {
	StringStream out;
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
		for (auto &it : capabilities) {
			out << it << " ";
		}
		out << ")";
	}
	return out.str();
}

DBusLibrary DBusLibrary::get() {
	return DBusLibrary(s_connection.get());
}

bool DBusLibrary::isAvailable() const {
	return _connection->sessionConnection && _connection->systemConnection;
}

InterfaceThemeInfo DBusLibrary::getCurrentInterfaceTheme() const {
	return _connection->getCurrentTheme();
}

void DBusLibrary::addNetworkConnectionCallback(void *key, Function<void(const NetworkState &)> &&cb) {
	_connection->addNetworkConnectionCallback(key, move(cb));
}

void DBusLibrary::removeNetworkConnectionCallback(void *key) {
	_connection->removeNetworkConnectionCallback(key);
}

DBusLibrary::DBusLibrary(DBusInterface *c) : _connection(c) { }

}
