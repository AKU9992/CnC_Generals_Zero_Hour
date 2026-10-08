# Нативный D3D12: стадия от 8 октября 2026

Полный перенос игрового W3D ещё не завершён. Новый код находится в
`renderer/native12`; он собирается отдельно от действующего моста.
Игровой клиент из `Generals-GPT-Client.zip` не заменён этой сборкой.

## Реализованный код

- `NativeDevice12`: аппаратный AMD64 D3D12 device, direct queue, DXGI flip
  swapchain, три allocator/upload-буфера с fence, PSO/root signature,
  нативная отрисовка геометрии, resize, readback пикселей, завершение и
  повторная инициализация. Нормальный кадр не ждёт полного GPU idle.
- `NativeDlss12`: DLSS Quality/DLAA/Off через зафиксированный Streamline
  2.14.1. Проверяет принадлежность ресурсов устройству, освобождает проход
  после ожидания GPU, передаёт реальный DXGI swapchain в common Present hooks.
- `NativeScene12`: RGBA16F цвет, RG16F rasterized motion, R32F глубина,
  отдельный D32 Z-buffer, jitter и текущая/предыдущая трансформация объекта.
  Нейросетевой результат рисуется в native target, после него можно рисовать HUD.
  Пока поддерживается геометрия position/color; материалы W3D не реализованы.
- Переход между режимами до первого кадра не вызывает освобождение
  ещё не созданных DLSS ресурсов. Флаг успешного освобождения сохраняет
  возможность повторной попытки при ошибке SDK.

Здесь нет D3D8/D3D9On12 объектов, unwrap/return interop или D3DX9.
Это ядро нового renderer, а не переключатель существующей игры на DX12.

## Воспроизводимая проверка

```powershell
.\tools\Get-StreamlineSdk.ps1
.\tools\Build-NativeRenderer12.ps1 -Test -Neural
.\tools\Get-ClientArchitecture.ps1 -ClientRoot .build\original-client -OutputPath .build\original-client-architecture.json
```

Среда: MSVC 19.51, Windows SDK 26100, Windows 11,
NVIDIA RTX A1000 6GB Laptop GPU, driver 32.0.15.9641.
SDK и runtime проверяются по SHA256/цифровым подписям.
Архив исходного клиента совпал с release.json:
`2DE1EE787E1AB377CF85C624FBC4C1BD1B2C6F9675220EF1DCB07EB19A2AC765`.
Его отдельная копия находится в `.build/original-client`.

Три отдельные AMD64 программы:

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

Журналы и exit codes: `.build/native12/*Tests.log`,
`.build/native12/validation.json`. Это тестовые сцены, не игровые карты.
Новый код компилируется с `/W4 /WX`. Опция `-DebugLayer` требует отдельно
установленного Windows Graphics Tools; на данном ПК слой отсутствует
(DXGI_ERROR_SDK_COMPONENT_MISSING). Проверки обычного драйвера прошли,
проверка debug layer не выполнена.

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
Это конфигурация прежнего игрового renderer, не интеграция нового backend.

Оставшаяся обязательная работа:

1. Нативные vertex/index/default heaps и DDS-texture upload, SRV/sampler
   descriptors, dirty state/PSO cache и fixed-function материалы W3D.
2. Подключить компилируемое community-дерево через воспроизводимый patch;
   заменить DX8Wrapper и связанные классы ресурсов. Простое изменение
   опубликованного EA-дерева не меняет реально собираемый клиент.
3. Реализовать terrain, multi-stage texturing, lighting, fog, alpha-test,
   transparency, shadows, water/shaders, particles и render-to-texture.
4. Передавать реальные камеры и историю mesh/анимации; reset при смене
   карты/камеры/разрешения. Проверить shader displacement/transparent motion.
5. Удалить из линковки/запуска игры графические зависимости D3D8/9 и
   адаптировать оставшиеся x86 видео/DSP/утилиты. XAudio2 уже существует.
6. Собрать и проверить обе игры по отдельности: игровые сцены, HUD, звук,
   настройки, сохранения, длительные сражения, shutdown и новый чистый пакет.

Нет готового нового установщика, тестов игровых сражений, измерения FPS,
Frame Generation или Ray Reconstruction. Не устанавливать тестовые EXE
вместо действующего `generals-client.exe`.

Архитектурные источники: [Microsoft pipeline state](https://learn.microsoft.com/en-us/windows/win32/direct3d12/managing-graphics-pipeline-state-in-direct3d-12),
[NVIDIA DLSS 2.14.1](https://github.com/NVIDIA-RTX/Streamline/blob/v2.14.1/docs/ProgrammingGuideDLSS.md),
[NVIDIA manual hooking 2.14.1](https://github.com/NVIDIA-RTX/Streamline/blob/v2.14.1/docs/ProgrammingGuideManualHooking.md).
