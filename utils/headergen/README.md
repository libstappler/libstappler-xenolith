# Приложение для обновления заголовоков Vulkan внутри Xenolith

А также других генерируемых ресурсов

Приложение загружает актуальный [регистр функций Vulkan](https://raw.githubusercontent.com/KhronosGroup/Vulkan-Docs/main/xml/vk.xml) и подготавливает файлы для Xenolith

## Сборка

```
make
make install
```

## Работа приложения

Запуск приложения:

```
$ ./headergen
headergen <options> registry|icons|material
Options:
    -v (--verbose)
    -h (--help)
```

* registry - генерирует новые файлы Vulkan
* icons - генерирует иконки для клиентских декораций Wayland
* material - генерирует векторные иконки Material Design
