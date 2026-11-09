# PEPEL VOX

Вокальный плагин для FL Studio (VST3, Windows) + AU (macOS) + Standalone.
JUCE 8, C++17, CMake. Интерфейс полностью нарисован вручную: пиксельный 8-бит аниме + VHS,
палитра «ночное небо + кровь».

## Что внутри

Цепочка: Input → Auto-Tune (+ Syn-Lock) → Pitch Shift → Harmonizer → Granular → Exciter → Bitcrusher → Distortion → Phaser → Flanger → Tremolo → Dry/Wet → Output (мягкий лимитер).

| Блок | Регуляторы |
|---|---|
| **Auto-Tune** | Key, Scale (8 строев), Speed (0-400 мс, 0 = жёсткий T-Pain), Humanize (сохраняет вибрато и живость), Strength (сила), **Syn-Lock**, Lock Mode |
| **Syn-Lock** | 0-40% жёстко «приколачивает» голос к ноте (скорость 0, вибрато убрано, сила 100%). 25-100% подмешивает чистый 16-полосный вокодер на пилообразных несущих, строго на ноте автотюна (без искажений, как синтезатор). Режимы: Unison, Octave, Power (квинта), Triad (терция+квинта по выбранному строю) |
| Granular | Size, Density, Pitch, Scatter, Mix |
| Pitch + Harmony | Shift, Fine, Mix; два голоса гармонайзера в **ступенях строя** (Voice 1/2, +2 = терция, +4 = квинта), Harm Mix, Spread |
| Bitcrusher | Bits, Downsmp, Mix |
| Distortion | Drive, Tone, Mix (tanh с антиалиасингом ADAA) |
| Exciter | Freq, Drive, Mix |
| Phaser | Rate, Depth, Feedback, Mix (6 ступеней) |
| Flanger | Rate, Depth, Feedback, Mix |
| Tremolo | Rate, Depth, Shape (синус → квадрат) |
| Master | Input, Output, Dry/Wet |

Везде Mix/Depth = 0 значит «эффект выключен», 100 = перегруз.
Двойной клик по слайдеру сбрасывает значение. Верхняя шапка: стрелки `< >` переключают 15 пресетов
(они же видны в списке программ FL Studio).

**Задержка (latency): ~40 мс**, плагин сообщает её хосту, FL Studio компенсирует её сама (PDC).
Для записи «на лету» с мониторингом лучше включить мониторинг через звуковую карту.
Диапазон определения высоты голоса 80-1100 Гц, один голос (моно-источник).

## Пресеты
INIT · CLEAN POP TUNE · HARD TUNE T-PAIN · TRAP MELODIC · HYPERPOP GLITCH · CLOUD RAP DREAM · ROBOT CHOIR ·
VOCODER LEAD · PLUGG UNDERWATER · RAGE BLOWN OUT · PHONK TAPE · HYPER CHIPMUNK · WIDE DOUBLE · GRAIN SPACE · ALT-POP DARK

Для автотюна всегда ставьте **Key / Scale** под тональность трека.

## Погода внизу
Показывает, насколько «грязный» звук: чистый звук = ясное небо и солнце, дальше рассвет, облака, дождь,
гроза с кровавой луной. Силуэт «города» на горизонте пляшет под уровень входа.

---

## Как получить плагин через GitHub (только браузер, без установки компилятора)

1. Зайдите на https://github.com, нажмите **New repository**, назовите `pepel-vox`, **Public**, **Create repository**.
2. На странице пустого репозитория нажмите **uploading an existing file**.
3. Распакуйте архив `PepelVox.zip` у себя на компьютере. Перетащите **содержимое** папки `PepelVox` в окно браузера
   (папки `Source`, `Assets`, `Tests`, `docs`, `.github` и файлы `CMakeLists.txt`, `README.md`, `.gitignore`).
   - Папка `.github` на Windows скрыта: включите «Показывать скрытые элементы» в Проводнике.
   - Если `.github` не загрузилась: **Add file → Create new file**, имя `.github/workflows/build.yml`
     (косая черта создаст папки), вставьте туда содержимое файла `build.yml`.
4. Внизу страницы нажмите **Commit changes**.
5. Откройте вкладку **Actions**. Сборка стартует сама (если спросит, разрешите workflows). Ждите 15-30 минут (скачивается JUCE).
6. Когда все три задачи (Windows, macOS, Linux) позеленеют, откройте последний запуск, внизу в разделе **Artifacts** скачайте
   `PepelVox-Windows`.
7. Для публичного релиза: **Releases → Create a new release**, тег `v1.0.0` → архивы приложатся автоматически.

## Установка в FL Studio (Windows)
1. Распакуйте `PepelVox-Windows.zip`. Внутри `VST3/Pepel Vox.vst3`.
2. Скопируйте папку `Pepel Vox.vst3` в `C:\Program Files\Common Files\VST3\`.
3. FL Studio: **Options → Manage plugins**, внизу нажмите **Find plugins** (при необходимости **Verify plugins**).
4. В микшере: слот эффекта → **Pepel Vox** (или найдите в браузере плагинов, раздел Installed → Effects → VST3).
Если Windows ругается на SmartScreen при запуске Standalone, нажмите «Подробнее → Выполнить в любом случае» (сборка не подписана).

## Локальная сборка (необязательно)
```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DPEPEL_BUILD_TOOLS=ON
cmake --build build --config Release
```
`-DPEPEL_BUILD_TOOLS=ON` добавляет:
- `PepelDspTest`: проверки DSP без JUCE (точность тюнинга, вибрато, уровень, отсутствие NaN/перегрузов).
- `PepelScreenshot out.png [пресет] [dirt 0..1] [уровень]`: снимок интерфейса в PNG без DAW (на Linux через `xvfb-run`).

## Лицензии
Шрифты VT323 и Press Start 2P: SIL Open Font License 1.1 (файлы лицензий в `Assets/`).
JUCE: AGPLv3 / коммерческая лицензия. Если распространяете плагин, учитывайте лицензию JUCE.
