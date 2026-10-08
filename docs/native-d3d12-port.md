# Нативный D3D12: стадия от 8 октября 2026

Новый backend подключён к игровому W3D в отдельных сборках Generals и
Zero Hour AMD64. Код находится в `renderer/native12`, режим запуска —
`GENERALS_RENDERER=native12`. Игровые сцены, материалы, вода и тени работают
на собственных D3D12 ресурсах. Исходный клиент из ZIP и установленные игры
не заменены; это стадия интеграции, ещё не новый релизный установщик.

## Реализованный код

- `NativeDevice12`: аппаратный AMD64 D3D12 device, direct queue, DXGI flip
  swapchain, три allocator/upload-буфера с fence, PSO/root signature,
  нативная отрисовка геометрии, resize, readback пикселей, завершение и
  повторная инициализация. Нормальный кадр не ждёт полного GPU idle.
- `NativeDlss12`: DLSS Quality/DLAA/Off через зафиксированный Streamline
  2.14.1. Проверяет принадлежность ресурсов устройству, освобождает проход
  после ожидания GPU, передаёт реальный DXGI swapchain в common Present hooks.
- `NativeScene12`: RGBA16F цвет, RG16F rasterized motion, R32F глубина,
  отдельный D24S8 depth/stencil, jitter и текущая/предыдущая трансформация объекта.
  Нейросетевой результат рисуется в native target, после него можно рисовать HUD.
- `W3DNative8`: собственная реализация исходных COM-интерфейсов W3D.
  Имена ABI сохраняются для компилируемого движка; Direct3D8/9 устройства,
  runtime, interop и D3D9On12 в этой DLL не используются. Draw/Present,
  текстуры, поверхности, буферы и render targets выполняются через native12.
- `W3DMaterial12`: FVF/индексы, матрицы, diffuse/ambient/emissive/specular lighting,
  directional/point/spot lights, attenuation/range/cone и inverse-transpose normals,
  fog, alpha-test/blend, cull, clip planes, четыре текстурные стадии,
  texture matrices, projected/camera coordinates, signed bump mapping,
  PSO cache и SRV/sampler heaps. Upload-геометрия, immutable draw descriptors
  и ссылки на ресурсы сохраняются до frame fence. Индексы пока раскрываются
  на CPU; повторное использование статической GPU-геометрии требует оптимизации.
- `W3DResources12`: native texture/default heaps, BC1/2/3, mip footprints,
  component mapping для A8/L8/A8L8, dirty uploads и barrier states.
  Render targets сохраняют GPU contents и могут использоваться как SRV.
- `D3DX8CompatibilityNative12`: CPU-математика, DDS/TGA/обычные изображения,
  преобразование поверхностей и генерация mip-уровней. D3DX9 не загружается.
  DDS loader отклоняет повреждённые payload и неподдерживаемые форматы.
- Вода: отдельная native factory подключает к W3D HLSL-программы волн,
  отражения, речных бликов и grid/trapezoid воды с noise/sparkles/shroud.
  Геометрия, анимация, текстуры и константы остаются игровыми. Старый
  assembly/bytecode этих программ не исполняется. Stencil volume shadows
  используют D24S8 и native PSO; projected passes используют render targets.
- Переход между режимами до первого кадра не вызывает освобождение
  ещё не созданных DLSS ресурсов. Флаг успешного освобождения сохраняет
  возможность повторной попытки при ошибке SDK.

Здесь нет D3D8/D3D9On12 объектов, unwrap/return interop или D3DX9.
`Build-X64Game.ps1 -Native12` применяет воспроизводимый patch к зафиксированному
community-дереву, выбирает CPU D3DX helpers и собирает новый backend с игрой.
DLL и проверенный Streamline runtime копируются рядом с отдельным EXE.

## Воспроизводимая проверка

```powershell
.\tools\Get-StreamlineSdk.ps1
.\tools\Build-CommunityBaseline.ps1 -ConfigureOnly
.\tools\Build-X64Game.ps1 -ConfigureOnly -Native12 -Edition Generals
.\tools\Build-NativeRenderer12.ps1 -Test -Neural -W3D
.\tools\Build-X64Game.ps1 -Native12 -Edition Generals
.\tools\Build-X64Game.ps1 -Native12 -Edition ZeroHour
.\tools\Test-NativeW3DSuite.ps1 -Seconds 10
.\tools\Get-ClientArchitecture.ps1 -ClientRoot .build\original-client -OutputPath .build\original-client-architecture.json
```

Среда: MSVC 19.51, Windows SDK 26100, Windows 11,
NVIDIA RTX A1000 6GB Laptop GPU, driver 32.0.15.9641.
SDK и runtime проверяются по SHA256/цифровым подписям.
Архив исходного клиента совпал с release.json:
`2DE1EE787E1AB377CF85C624FBC4C1BD1B2C6F9675220EF1DCB07EB19A2AC765`.
Его отдельная копия находится в `.build/original-client`.

Четыре отдельные AMD64 программы:

1. `NativeRenderer12Tests`: readback цвета и coverage треугольника,
   смена разрешения 640×480 → 3440×1440 → 3840×2160 → 641×479,
   повторное использование allocator, проверка ошибочных вызовов,
   полное уничтожение/повторное создание устройства.
2. `NativeDlss12Tests`: реальные DLSS/DLAA evaluation и чтение GPU output
   на синтетическом статическом кадре, смена Quality/DLAA/Off.
   Quality: 2293×960 → 3440×1440 и 2560×1440 → 3840×2160.
3. `NativeScene12Tests`: по 40 кадров Off/DLAA/Quality при 640×480,
   3440×1440 и 3840×2160. Двигающаяся геометрия и jitter, Z-occlusion,
   сравнение фактических motion/depth с расчётными значениями через
   readback; проверка цвета сцены и отдельного зелёного HUD после DLSS.
4. `W3DNative12Tests`: вызовы реального W3D ABI, texture lock/upload/SRV,
   GPU render-to-texture, stencil shadow mask с проверкой пикселя вне маски,
   все четыре HLSL water passes и wave vertex constants, signed V8U8
   искажение координат, DDS compressed mip upload, TGA generated mip
   и отказ загрузки обрезанного DDS. Цвет проверяется через native GPU readback.
   Также проверяются diffuse/ambient/emissive/specular материал, затухание
   и range точечного источника, внутренний/внешний конус прожектора.

Журналы и exit codes: `.build/native12/*Tests.log`,
`.build/native12/validation.json`. Это тестовые сцены, не игровые карты.
Ядро компилируется с `/W4 /WX`, W3D ABI target — `/W4`. Опция `-DebugLayer` требует отдельно
установленного Windows Graphics Tools; на данном ПК слой отсутствует
(DXGI_ERROR_SDK_COMPONENT_MISSING). Проверки обычного драйвера прошли,
проверка debug layer не выполнена.

## Проверка в игровом W3D

`Test-NativeW3DSuite.ps1` проверяет шесть отдельных запусков при 1280×720:
Generals и Zero Hour × Off/DLAA/DLSS Quality. Generals загружает Tournament
Lake, Zero Hour — свою shell map с кораблями и водой. Обе программы имеют
PE Machine AMD64. Проверяются реальные world frames, water/stencil draws,
DXGI Present, native output capture и штатное завершение с exitCode 0.
Для нейросетевых режимов требуется не менее 32 успешных world frames.
Quality использует 853×480 → 1280×720, DLAA — 1280×720.

Процесс загружает `generals-native12.dll` и `d3d12.dll`; `d3d8.dll` и
`d3d9.dll` отсутствуют и в module audit, и в imports EXE/backend.
HUD рисуется после нейросетевого прохода. Камера, viewport, near/far,
Halton jitter и предыдущие mesh/camera transforms передаются из W3D.
Целевые отчёты: `.build/native12/game-validation.json` и `game-*-*.json`;
фактические кадры: `game-*-*.native.bmp`. Они проверяются визуально отдельно
от наличия успешного Draw: один успешный вызов сам по себе не доказывает,
что игровые текстуры найдены.

Тестовые параметры/настройки находятся в `.build/native12/user-*` и
используются только с `GENERALS_TEST_QUIT_SECONDS`. Для ZIP-копии Zero Hour
тест передаёт базовый путь Generals с завершающим `\` в штатный archive loader;
приоритет expansion сохраняется, реестр и игровые архивы не изменяются.
В Zero Hour диагностируется один отсутствующий исходный asset
`trstrtholecvr.tga`; его нет в BIG-архивах обоих каталогов ZIP. Остальные
проверяемые текстуры, включая воду, загружаются. Долгие матчи и кампании
этот короткий прогон не заменяет.

## Архитектура x64 и остаток переноса

Все 24 PE-файла в активных `CaCG/Client` и `CaCGZH/Client`, включая NVIDIA
и CRT DLL, имеют machine AMD64. Это уже рабочая x64 база. Аудит не доказывает
завершение миграции всех вспомогательных программ или динамических загрузок.
Оба корневых launcher `generals.exe` также имеют machine AMD64.
Если добавляются managed программы, их архитектура требует проверки CLR flags,
а не оценки только PE Machine. Старое retail Bink-видео пока не поддерживается.

Зафиксированные community-исходники и зависимости подготовлены через
`Build-CommunityBaseline.ps1 -ConfigureOnly`; AMD64 конфигурация Generals —
через `Build-X64Game.ps1 -ConfigureOnly -Edition Generals`.
Ключ `-Native12` выбирает отдельные каталоги `.build/game-generals-native12`
и `.build/game-zerohour-native12` и подключает новый backend.

Оставшаяся обязательная работа:

1. Проверить кампании, длительные матчи, сохранения, загрузку/смену карт,
   fullscreen/resize и смену графических режимов в игровом UI.
2. Оптимизировать статическую геометрию, раскрытие индексов на CPU и
   синхронные dirty uploads; провести измерения FPS и времени кадра.
3. Расширить ресурсы и специальные эффекты: остальные texture ops,
   cube/volume textures, MSAA и оставшиеся shader binaries. Они не
   объявлены поддерживаемыми; legacy shader loader использует игровой
   fallback из текстурных стадий. Общего транслятора shader assembly нет.
4. Дополнительно проверить animated/transparent/displaced motion,
   несколько камер и качество истории DLSS после переходов.
5. Проверить native audio в полном игровом прогоне и подготовить новый
   чистый пакет. Старое retail Bink-видео требует отдельного переноса.

Нет готового нового установщика, тестов игровых сражений, измерения FPS,
Frame Generation или Ray Reconstruction. Не устанавливать тестовые EXE
вместо действующего `generals-client.exe`.

Архитектурные источники: [Microsoft pipeline state](https://learn.microsoft.com/en-us/windows/win32/direct3d12/managing-graphics-pipeline-state-in-direct3d-12),
[NVIDIA DLSS 2.14.1](https://github.com/NVIDIA-RTX/Streamline/blob/v2.14.1/docs/ProgrammingGuideDLSS.md),
[NVIDIA manual hooking 2.14.1](https://github.com/NVIDIA-RTX/Streamline/blob/v2.14.1/docs/ProgrammingGuideManualHooking.md).
Формулы освещения сверены с [Microsoft lighting model](https://learn.microsoft.com/en-us/windows/win32/direct3d9/mathematics-of-lighting)
и [attenuation/spotlight](https://learn.microsoft.com/en-us/windows/win32/direct3d9/attenuation-and-spotlight-factor).
