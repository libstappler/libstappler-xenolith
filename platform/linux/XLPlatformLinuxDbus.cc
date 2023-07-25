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

#include "XLPlatformLinuxDbus.h"

#include <dlfcn.h>

namespace stappler::xenolith::platform {

DBusLibrary::DBusLibrary() {
	auto d = dlopen("libdbus-1.so", RTLD_LAZY);
	if (d) {
		if (open(d)) {
			handle = d;
		} else {
			dlclose(d);
		}
	}
}

DBusLibrary::~DBusLibrary() {
	if (handle) {
		dlclose(handle);
		handle = nullptr;
	}
}

bool DBusLibrary::open(void *d) {
	this->dbus_error_init =
			reinterpret_cast<decltype(this->dbus_error_init)>(dlsym(d, "dbus_error_init"));
	this->dbus_message_new_method_call =
			reinterpret_cast<decltype(this->dbus_message_new_method_call)>(dlsym(d, "dbus_message_new_method_call"));
	this->dbus_message_append_args =
			reinterpret_cast<decltype(this->dbus_message_append_args)>(dlsym(d, "dbus_message_append_args"));
	this->dbus_connection_send_with_reply_and_block =
			reinterpret_cast<decltype(this->dbus_connection_send_with_reply_and_block)>(dlsym(d, "dbus_connection_send_with_reply_and_block"));
	this->dbus_message_unref =
			reinterpret_cast<decltype(this->dbus_message_unref)>(dlsym(d, "dbus_message_unref"));
	this->dbus_error_is_set =
			reinterpret_cast<decltype(this->dbus_error_is_set)>(dlsym(d, "dbus_error_is_set"));
	this->dbus_message_iter_init =
			reinterpret_cast<decltype(this->dbus_message_iter_init)>(dlsym(d, "dbus_message_iter_init"));
	this->dbus_message_iter_recurse =
			reinterpret_cast<decltype(this->dbus_message_iter_recurse)>(dlsym(d, "dbus_message_iter_recurse"));
	this->dbus_message_iter_get_arg_type =
			reinterpret_cast<decltype(this->dbus_message_iter_get_arg_type)>(dlsym(d, "dbus_message_iter_get_arg_type"));
	this->dbus_message_iter_get_basic =
			reinterpret_cast<decltype(this->dbus_message_iter_get_basic)>(dlsym(d, "dbus_message_iter_get_basic"));
	this->dbus_bus_get =
			reinterpret_cast<decltype(this->dbus_bus_get)>(dlsym(d, "dbus_bus_get"));

	return this->dbus_error_init
			&& this->dbus_message_new_method_call
			&& this->dbus_message_append_args
			&& this->dbus_connection_send_with_reply_and_block
			&& this->dbus_message_unref
			&& this->dbus_error_is_set
			&& this->dbus_message_iter_init
			&& this->dbus_message_iter_recurse
			&& this->dbus_message_iter_get_arg_type
			&& this->dbus_message_iter_get_basic
			&& this->dbus_bus_get;
}

DBusMessage *DBusLibrary::getSettingSync(DBusConnection *const connection, const char *key, const char *value) {
	DBusError error;
	dbus_bool_t success;
	DBusMessage *message;
	DBusMessage *reply;

	this->dbus_error_init(&error);

	message = this->dbus_message_new_method_call("org.freedesktop.portal.Desktop", "/org/freedesktop/portal/desktop", "org.freedesktop.portal.Settings", "Read");

	success = this->dbus_message_append_args(message, DBUS_TYPE_STRING, &key, DBUS_TYPE_STRING, &value, DBUS_TYPE_INVALID);

	if (!success)
		return NULL;

	reply = this->dbus_connection_send_with_reply_and_block(connection, message, DBUS_TIMEOUT_USE_DEFAULT, &error);

	this->dbus_message_unref(message);

	if (this->dbus_error_is_set(&error))
		return NULL;

	return reply;
}

bool DBusLibrary::parseType(DBusMessage *const reply, const int type, void *value) {
	DBusMessageIter iter[3];

	this->dbus_message_iter_init(reply, &iter[0]);
	if (this->dbus_message_iter_get_arg_type(&iter[0]) != DBUS_TYPE_VARIANT)
		return false;

	this->dbus_message_iter_recurse(&iter[0], &iter[1]);
	if (this->dbus_message_iter_get_arg_type(&iter[1]) != DBUS_TYPE_VARIANT)
		return false;

	this->dbus_message_iter_recurse(&iter[1], &iter[2]);
	if (this->dbus_message_iter_get_arg_type(&iter[2]) != type)
		return false;

	this->dbus_message_iter_get_basic(&iter[2], value);

	return true;
}

}
