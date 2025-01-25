## Операционные системы
# Лабораторная работа №2

### Setup
```
meson setup build
```

### Compile
```
meson compile -C build
```

Реализация `page cache` с политикой `MRU` (Most recent used). Проверка библиотеки осущетвляется с помощью нагрузчика `ema-replace-int` - замена всех вхождений целого числа в файле
