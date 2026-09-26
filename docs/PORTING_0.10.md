# Перенос TiP-Recomp на ReXGlue SDK 0.10

> Дата: 2026-09-26. Исходник: `C:\Recompiles\reference_repos\viva_pinata_recomp_tip`, клон [SolarCookies/TiP-Recomp](https://github.com/SolarCookies/TiP-Recomp), коммит `757f550` (2026-08-28). История сохранена в этом репозитории, удалённый репозиторий оригинала называется `upstream`.

## 1. Что это за проект

**ReTiP**: рекомпиляция *Viva Piñata: Trouble in Paradise* (Xbox 360, 2008) на ReXGlue. Автор оригинала — SolarCookies.
- В оригинале есть лаунчер, оверлеи TiPTools (спавн пинат, растения, игрок, хулиганы, лопата, графика, апскейл, настройки), курсор мыши и сырой ввод, FPS-оверлей, хуки камеры и рендера (соотношение сторон, frustum, letterbox), моды (текстуры, шейдеры HLSL, данные `.vdat`), Discord Rich Presence.
- Объём: 56 файлов исходников, около 13 200 строк C++; 24 mid-asm хука; 46 именованных функций; 20 записей `[rexcrt]` (куча и файловый ввод-вывод).

### Сборка оригинала
- Собирался на **форке SDK** `SolarRecomps/rexglue-ostentation`, ветка `dev`, версия 0.8.1.60-dev. CI клонировал и собирал SDK из исходников.
- **Форк сейчас недоступен:** GitHub отвечает 404, так что собрать оригинал как раньше нельзя.
- Сам код уже написан на API, близком к 0.10: `REX_HOOK`, `REX_EXTERN`, `REX_HOOK_RAW`, `rex::ppc::GuestToHostFunction`, `REX_DEFINE_APP`. Основная работа при переносе — функции форка, которых нет в upstream.

### Лицензия
В оригинале **нет файла лицензии**. По умолчанию это значит «все права у автора». Выкладывать этот код в новый публичный репозиторий без разрешения SolarCookies нельзя. Варианты: приватный репозиторий, форк на GitHub или договориться с автором.

## 2. Игра

| | |
| :-- | :-- |
| Диск (Redump) | `Viva Pinata - Trouble in Paradise (World).iso`, MD5 `aad50e5418e22e8a9b92d2c0c258079e` (совпадает с Redump) |
| Title ID / Media ID | `4D53085F` / `76A44A5D` |
| XEX | версия `0.0.0.4`, образ с `0x82000000`, точка входа `0x82B0AEA8`, SHA-1 `c5970941…6b8b` |
| Где лежит | `assets/` (5,0 ГБ, распаковано), ISO — в `C:\Recompiles\VivaPinata_TroubleInParadise_ISO_Backup\` |

## 3. Что сделано в этом коммите (первый проход)

| Область | Изменение |
| :-- | :-- |
| SDK | Prebuilt ReXGlue `0.10.0.8-dev.g1406e1b` в `rexglue/win-amd64` (как у Viva Piñata); `cmake/fetch-rexglue-sdk.cmake` скачивает его, если папки нет |
| CMake | `CMakeLists.txt` в стиле 0.10: скачивание SDK, однократный codegen при первой настройке, без опций форка `REXGLUE_ENABLE_TEXTURES/SHADERS`. После сборки `packaged/` (retip.toml, моды, фон лаунчера) копируется к exe; существующий `retip.toml` не перезаписывается. `CMakePresets.json` и `generated/rexglue.cmake` взяты из шаблона 0.10, добавлен `CMakeUserPresets.json` (`local-win-relwithdebinfo`) |
| Манифест | `sdk_version = "0.10.0"`; `config/retip_variables.toml` (`[globals]`, функция форка) больше не подключается; валидатор `tools/validate_manifest.py` проходит чисто |
| Флаги кодогенерации | Все по умолчанию (`false`) для первого запуска; значения форка (`skip_lr`, `ctr_as_local` и др. = true) оставлены в комментариях. Границы функций с пометками «crash fix» сохранены |
| Discord RPC | `tip_compat::discord_rpc` — заглушка (`src/tip_engine/Compat.h`) |
| Моды | `tip_compat::GetEnabledModDirs` и настройки `mods_data_root`, `enabled_mods`, `default_mods` (`Compat.cpp`). Моды данных (`mods/<имя>/data/*.vdat`) работают через собственный хук `assetManOpen`. Моды текстур и шейдеров требуют GPU-слоя форка и пока неактивны |
| SolarRenderer | Настройки `OverlaySolarRenderer`, `SolarRendererPreview`, `rgb_cursor` определены выключенными |
| `retip_globals.h` | Убран (его генерировал форк); единственное место использования уже читает адрес напрямую |
| Диалоги | В 0.10 `ImGuiDialog` регистрируется сам в конструкторе: 4 лишних `AddDialog` убраны, иначе всё рисовалось бы дважды |
| Immediate drawer | В 0.10 у `ImGuiDrawer` нет доступа к нему: `g_immediate_drawer = immediate_drawer()` в `OnCreateDialogs`, используется лаунчером |
| Пути | В 0.10 `game_data_root()` заполняется только в `ConstructRuntime`, после создания диалогов. Проверка «игра установлена» перенесена в `OnConfigurePaths`, иначе лаунчер застревал на «Install with Goopie». `assets/` ищется вверх от папки exe, а не от текущей папки |

## 4. Что проверить при первой сборке

1. **Компиляция.** Я не собирал проект (сборка только в Visual Studio). Возможны ошибки в местах, где API 0.10 отличается от форка. Их правим по логу сборки.
2. **SDL3.** Лаунчер вызывает `SDL_OpenURL`. Если линковщик не найдёт SDL3, нужно добавить библиотеку явно.
3. **Mid-asm хуки** (24 шт., `config/retip_midasm.toml`): все функции есть в `src/`, их сигнатуры должны совпасть с объявлениями, которые генерирует 0.10.
4. **Известная ошибка SDK 0.10:** `vpkd3d128` FLOAT16 при записи в тот же регистр теряет знак. В Viva Piñata из-за неё земля была белой. В TiP, скорее всего, будет то же самое. Проверить в сгенерированном коде (`// vpkd3d128 vN,vN,5,`) и перенести хук из `VivaPinata_xbox360` (`src/game_fixes.h`, раздел 1).
5. **Распаковка CAFF на месте:** если будут править `.bnl`/CAFF-файлы (например, перевод), поток должен быть ровно исходного размера. См. `VivaPinata_xbox360/tools/make_russian_bnl.py`.

## 5. Как запустить

1. Visual Studio 2026 → **Файл → Открыть → Папка…** → `C:\Recompiles\VivaPinata_TroubleInParadise_xbox360`.
2. Дождаться конца генерации CMake. В первый раз запустится codegen по `assets/default.xex`, это несколько минут.
3. Выбрать конфигурацию **`local-win-relwithdebinfo`**, нажать **F7**.
4. Перед первым **F5**: Отладка → Параметры исключений → Win32 Exceptions → снять галку `0xC0000005` (это не падения, а защита памяти GPU в SDK).
5. **F5** — откроется лаунчер ReTiP → PLAY.
