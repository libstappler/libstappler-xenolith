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

#ifndef XENOLITH_PLATFORM_LINUX_XLPLATFORMLINUXDBUS_H_
#define XENOLITH_PLATFORM_LINUX_XLPLATFORMLINUXDBUS_H_

#include "XLCommon.h"

#if LINUX

typedef enum
{
  DBUS_BUS_SESSION,    /**< The login session bus */
  DBUS_BUS_SYSTEM,     /**< The systemwide bus */
  DBUS_BUS_STARTER     /**< The bus that started us, if any */
} DBusBusType;

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

#define DBUS_TYPE_VARIANT       ((int) 'v')
#define DBUS_TYPE_STRING        ((int) 's')
#define DBUS_TYPE_INVALID       ((int) '\0')
#define DBUS_TYPE_INT32         ((int) 'i')
#define DBUS_TIMEOUT_USE_DEFAULT (-1)

namespace stappler::xenolith::platform {

class DBusLibrary {
public:
	DBusLibrary();
	~DBusLibrary();

	DBusMessage *getSettingSync(DBusConnection *const connection, const char *key, const char *value);
	bool parseType(DBusMessage *const reply, const int type, void *value);

	operator bool () const { return handle != nullptr; }

	void (*dbus_error_init) (DBusError *error) = nullptr;
	DBusMessage* (*dbus_message_new_method_call) (const char *bus_name, const char *path, const char *iface, const char *method) = nullptr;
	dbus_bool_t (*dbus_message_append_args) (DBusMessage *message, int first_arg_type, ...) = nullptr;
	DBusMessage* (*dbus_connection_send_with_reply_and_block) (DBusConnection *connection,
			DBusMessage *message, int timeout_milliseconds, DBusError *error) = nullptr;
	void (*dbus_message_unref) (DBusMessage *message) = nullptr;
	dbus_bool_t (*dbus_error_is_set) (const DBusError *error) = nullptr;
	dbus_bool_t (*dbus_message_iter_init) (DBusMessage *message, DBusMessageIter *iter) = nullptr;
	void (*dbus_message_iter_recurse) (DBusMessageIter *iter, DBusMessageIter *sub) = nullptr;
	int (*dbus_message_iter_get_arg_type) (DBusMessageIter *iter) = nullptr;
	void (*dbus_message_iter_get_basic) (DBusMessageIter *iter, void *value) = nullptr;
	DBusConnection* (*dbus_bus_get) (DBusBusType type, DBusError *error) = nullptr;

protected:
	bool open(void *d);

	void *handle = nullptr;

};

}

#endif

#endif /* XENOLITH_PLATFORM_LINUX_XLPLATFORMLINUXDBUS_H_ */
