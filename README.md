# Tuman

Стерео-эффектор (VST3 / AU / Standalone для macOS) — программный порт 15 алгоритмов
одного аппаратного мульти-эффекта. Алгоритмы восстановлены из прошивки и проверены
посэмплово против эмулятора оригинального кода: при тех же входных данных и положениях
ручек Tuman выдаёт тот же сигнал (кроме мест, перечисленных ниже).

Личный исследовательский проект, не для распространения.

## Установка

1. Открой страницу **Releases** этого репозитория и скачай `Tuman-mac.zip` из последнего релиза.
2. Распакуй и в Терминале выполни:
   ```
   bash ~/Downloads/Tuman-mac/install.sh
   ```
   Скрипт скопирует `Tuman.vst3` и `Tuman.component` в `~/Library/Audio/Plug-Ins`,
   снимет карантин macOS и проверит AU через `auval`.
3. В Ableton Live: Settings → Plug-Ins → Rescan. Tuman появится в Plug-Ins → Timmyjin.

## Управление

| Ручка | Что делает |
|---|---|
| **Mix** | баланс сухого/обработанного (как на устройстве — с небольшим «горбом» громкости в середине) |
| **P1–P4** | четыре параметра алгоритма, подписи меняются вместе с алгоритмом |
| **In / Out** | усиление на входе (вход клиппится на 0 dBFS, как АЦП) и на выходе |
| **Long** | скрытый режим половинной частоты: вдвое длиннее время/буферы, темнее звук (алгоритмы 1–8, кроме Lo-Fi Delay) |
| **Gate** | альтернативное поведение Pump и Wavefolder |

Смена алгоритма проходит как на железе: короткий фейд-аут, очистка памяти, фейд-ин.

| # | Алгоритм | P1 | P2 | P3 | P4 |
|---|---|---|---|---|---|
| 1 | Lush Reverb | Tone | Motion | Size | Decay |
| 2 | Infinity | Low Cut | Damping | Drive | Time |
| 3 | Generative | Low Cut | High Cut | Resonance | Decay |
| 4 | Shimmer | Oct Down | Oct Up | Smear | Feedback |
| 5 | Granular Reverb | Tone | Reverse | Smear | Feedback |
| 6 | Lo-Fi Delay | Tone | Reverse | Time | Feedback |
| 7 | Micro Loops | Loop 1 | Loop 2 | Loop 3 | Overdub |
| 8 | Micro Loops Reverb | Loop 1 | Loop 2 | Loop 3 | Diffuse |
| 9 | Flanger | Env | Release | Time | Stages |
| 10 | Nyquist | Bits | Sweep | Rate | Multiply |
| 11 | Old Tape | Flutter | Wow | Flicker | Tone |
| 12 | Pump | Bass | Punch | Density | Amount |
| 13 | Multiband Drive | Low | Mid | High | Fat |
| 14 | Wavefolder | Tone | Resonance | Shape | Fold |
| 15 | HPF / LPF | HP Freq | HP Res | LP Freq | LP Res |

## Как это устроено

- Движок работает на родной частоте устройства **46 875 Гц**, по одному сэмплу, в float32
  без FMA (`-ffp-contract=off`) — так совпадают все округления. На входе и выходе —
  стерео sinc-ресэмплер (Kaiser, 16 нулей, ≈ −100 dB остаток). Задержка сообщается хосту.
- Сохранены все «характерные» детали: общая 16-битная память на 60 000 сэмплов,
  сглаживание ручек, лимитер, фоновые задачи (рандом-модуляция Lush, YIN-трекер Nyquist,
  пересчёт таблицы Old Tape) с тем же темпом, что на устройстве.
- На выходе — DC-фильтр ~3 Гц вместо разделительного конденсатора аналогового выхода
  (Old Tape оставляет постоянную составляющую).

### Где не бит-в-бит

- Три таблицы складывания Wavefolder и характеристики ленты Old Tape восстановлены
  аппроксимацией (ряд Фурье / Чебышёва) — отличие на уровне −90…−100 dB.
- Фоновые задачи на железе идут асинхронно, их фаза относительно звука случайна —
  здесь она фиксирована.
- Аналоговая часть (входной/выходной каскады, шум кодека) не моделируется.

## Сборка

GitHub Actions (`.github/workflows/build.yml`) собирает universal-бинарь на macOS-14,
прогоняет `auval` и `pluginval` и публикует релиз. Локально:

```
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --target Tuman_VST3 Tuman_AU
```

`tools/render_demo.cpp` — офлайн-рендер демо-WAV по всем алгоритмам без JUCE
(`tuman_render <папка> [вход.wav]`). `tools/ref_render.cpp` — эталонный рендер для сверки.
