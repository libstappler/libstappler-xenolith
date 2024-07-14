# Пример реализации математического ядра для решения вычислительных задач

Реализует пример решения задач статистического анализа, инференса и обучения нейросетей.

## Требования

Полная установка Vulkan

## Структура

Makefile - Файл описания проекта
main.cpp - Точка входа
src - Основная директория кода
src/backend -Реализация математического ядра для Vulkan
src/layer - Абстрактные вычислительные слои математического ядра
src/processor - Инструмент для запуска
shaders - Шейдеры математического ядра
models - подготовленные модели для тестирования
tests -  Код для тестирования

## Сборка

```
make
make install
```


## Работа приложения

Запуск приложения:

```
$ stappler-build/host/debug/gcc/testapp --help
testapp <options> model <path-to-model-json> <path-to-input> - run model
testapp <options> gen - test random generation layer
testapp <options> input <path-to-input> - test input filter+normalizer
Options are one of:
	-v (--verbose)
	-h (--help)

```