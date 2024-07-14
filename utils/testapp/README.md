# Приложение графических тестов Xenolith

Создано для демонстрации и отладки графических компонентов

## Требования

Полная установка Vulkan

## Структура

Makefile - Файл описания проекта
main.cpp - Точка входа
src - Основная директория кода
src/widgets - Базовые виджеты для тестов
src/tests - Исходный код тестов
resources - Необходимые ресурсы для тестов

## Сборка

```
make
make install
```

## Работа приложения

Запуск приложения:

```
$ ./testapp --help
testapp <options>
Options are one of:
	--w=<initial screen width in pixels>
	--h=<initial screen height in pixels>
	--d=<pixel density>
	--l=<application locale code>
	--bundle=<application bundle name>
	--renderdoc - try to connect with renderdoc capture layers
	--novalidation - force-disable vulkan validation
	--decor=<left,top,right,bottom> - view decoration padding in pixels
	-v (--verbose)
	-h (--help)
$ ./testapp
```

Используйте графический интерфейс для запуска необходимых тестов

## Android

Проект gradle для Android расположен в директории proj.android. Его можно запустить в Android Studio.

## Добавление теста

Добавить новый тест в src/tests/AppTest.h в `enum class LayoutName`.

Добавить конфигурацию теста в src/tests/AppTests.cpp:

```cpp
	MenuData{LayoutName::<new layout name>, LayoutName::<root menu>, "org.stappler.xenolith.test.<new layout name>",
		"<test description>",
		[] (LayoutName name) { return Rc<NewTestLayoutClassName>::create(); }},
```

Добавить полноэкранную тестовую укладку в виде нового класса:

```
#include "AppLayoutTest.h"

namespace stappler::xenolith::app {

class NewTestLayoutClassName : public LayoutTest {
public:
	virtual ~NewTestLayoutClassName() = default;

	virtual bool init() override;

	virtual void onContentSizeDirty() override;

protected:
	...
};

}
```