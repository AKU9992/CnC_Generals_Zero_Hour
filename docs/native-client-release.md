Актуальное обновление: [AKU9992 Native12 Fix4](native12-fix4.md). Ниже сохранена история прежних выпусков.

# Тестовый клиент Native DirectX 12 / AMD64

Пакет от 8 октября 2026 включает Generals (CaCG) и Zero Hour (CaCGZH), игровые
ресурсы исходного клиента, нативный W3D/D3D12 backend, материалы, mip-текстуры,
воду, отражения, stencil-тени, DLAA/DLSS Quality и XAudio2.

Локальный выпуск находится в dist/Native12-2026-10-08:

- Generals-GPT-Setup.exe — установщик с проверкой SHA256 каждого файла.
  По умолчанию предлагает отдельную папку Generals GPT Native12.
- Generals-GPT-Client.zip — переносной пакет. Распакуйте CaCG и CaCGZH рядом,
  запускайте generals.exe в выбранной папке.
- release.json — размеры и SHA256 архива и установщика.

Launcher выбирает generals-native12.dll и передаёт Zero Hour путь к соседней
CaCG через GENERALS_BASE_GAME; регистрация старой игры не требуется для
загрузки базовых архивов. В пакет входят подписанные NVIDIA Streamline/DLSS
DLL и Visual C++ x64 runtime. Старый generals-d3d12.dll не включается.

Generals запускается в окне без рамки, Zero Hour — в оконном режиме.
Стартовое разрешение соответствует основному монитору. Режим сглаживания
выбирается в меню игры: Off, DLAA, DLSS Quality. DLAA/DLSS требуют
совместимую NVIDIA RTX. Сохранения и настройки остаются в Документах.

Для ручной проверки полезны кампании и длительные схватки, разные карты,
вода/отражения/тени, интерфейс, звук и смена разрешения. x64 Bink-видео ещё
не перенесены. Это экспериментальный выпуск; полная совместимость всех
карт и игровых сценариев пока не подтверждена.

При проблемах приложите GeneralsNative12.log и GeneralsTextureFailures.log
из папки игры, название карты, режим сглаживания и разрешение.

Сборка и проверка в отдельной папке:

    .\tools\Build-X64Game.ps1 -Native12 -Edition Generals
    .\tools\Build-X64Game.ps1 -Native12 -Edition ZeroHour
    .\tools\Test-X64Audio.ps1 -BuildOnly
    .\tools\Prepare-NativeClientRelease.ps1
    .\tools\Test-NativeClientRelease.ps1
    .\tools\Build-ClientPackage.ps1 -Native12 -GameRoot .build\native-client-release\client -OutputDirectory dist\Native12-2026-10-08

Проверка пакета использует реальные generals.exe и Client/generals-client.exe,
изолированные тестовые настройки, DLSS Quality, воду, stencil-тени, звуковую
сессию PID игры и штатное завершение. Во время теста проверяются модули D3D12
и отсутствие D3D8/D3D9. Перед упаковкой сверяются текущие суммы EXE,
launcher и backend с результатами этой проверки.

Установщик также поддерживает --verify и --extract <папка> без создания
ярлыков. После извлечения обе игры проверяются повторно через
Test-NativeClientRelease.ps1 -GameRoot <папка>.
Пакет содержит 10 419 файлов, 5 591 797 613 байт после распаковки.
Архив: 3 772 040 677 байт; установщик: 3 772 081 149 байт.
Проверка SHA256 всех встроенных файлов и распаковка установщика завершены.
Обе игры из установленной папки прошли проверку launcher, native D3D12,
DLSS Quality при 1920×1080, воды, stencil-теней, звука и штатного выхода.
Результаты и суммы бинарных файлов находятся в validation.json рядом с пакетом.
В Zero Hour остаётся одна отсутствующая текстура исходных игровых ресурсов:
trstrtholecvr.tga; в проверенной сцене Generals отсутствующих текстур нет.
Для исправлений скорости звука, недоступных иконок и полос GUI используйте
[обновлённый выпуск Native12 Fix1](native12-fix1.md).

Полная пересборка Native12 Fix2:
tools/Build-NativeClientRelease.ps1 -Clean

Выход: dist/Native12-2026-10-08-Fix2. Скрипт заново компилирует обе игры,
собирает текущий native backend, проверяет графику и звук, подготавливает
все ресурсы, тестирует обычные запускатели с настоящим QuitMenu,
создаёт ZIP и установщик, проверяет SHA256 всех встроенных файлов,
распаковывает установщик в отдельную папку и тестирует обе игры повторно.
Состояние: .build/native-client-release/fix2-release-state.json.
Полная сборка включает все исправления Fix1 и Fix2.