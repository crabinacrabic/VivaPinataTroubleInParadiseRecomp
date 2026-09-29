<div align="center">

<img src="docs/images/icon.png" width="96" alt="Иконка Viva Piñata: Trouble in Paradise">

# Viva Piñata: Trouble in Paradise Recomp

**Viva Piñata: Trouble in Paradise (Xbox 360, 2008), запущенная на Windows без эмулятора**

[English](README.md) · **Русский**

[![Platform](https://img.shields.io/badge/platform-Windows%20x64-0078D6?logo=windows&logoColor=white)](#что-нужно)
[![Title ID](https://img.shields.io/badge/Title%20ID-4D53085F-107C10?logo=xbox&logoColor=white)](#какая-версия-игры-нужна)
[![ReXGlue SDK](https://img.shields.io/badge/ReXGlue%20SDK-v0.10.0.8-8A2BE2)](https://github.com/rexglue/rexglue-sdk)
[![Renderer](https://img.shields.io/badge/render-Direct3D%2012-blue)](#настройки)
[![Status](https://img.shields.io/badge/status-playable-2ea44f)](#что-работает)
[![Languages](https://img.shields.io/badge/text-English%20%7C%20Русский-orange)](#русский-язык)

<img src="docs/images/title_screen.webp" width="49%" alt="Титульный экран на русском"> <img src="docs/images/garden.webp" width="49%" alt="Сад">

</div>

---

## Что это?

Оригинальная игра с Xbox 360, превращённая в обычную программу для Windows. Это **не эмулятор**: код игры переведён в C++ с помощью [ReXGlue SDK](https://github.com/rexglue/rexglue-sdk) («статическая рекомпиляция»).

Это проект [SolarCookies/TiP-Recomp](https://github.com/SolarCookies/TiP-Recomp) (ReTiP: лаунчер, управление мышью, меню инструментов в игре и моды), перенесённый на ReXGlue SDK 0.10, с добавленным русским переводом.

> [!IMPORTANT]
> **Файлов игры здесь нет.** Нужна своя копия игры, причём именно той версии, что указана ниже.

## Что нужно

- ПК с Windows 10 или 11 (64-бит) и видеокартой с DirectX 12.
- Около **25 ГБ** свободного места (Visual Studio ~12 ГБ, игра ~5 ГБ, образ диска ~8 ГБ на время распаковки).
- Свой образ диска (`.iso`) **Viva Piñata: Trouble in Paradise** для Xbox 360.

## Какая версия игры нужна

| | |
| :-- | :-- |
| **Диск (название Redump)** | `Viva Pinata - Trouble in Paradise (World)` |
| **Title ID / Media ID** | `4D53085F` / `76A44A5D` |
| **Версия** | `0.0.0.4`, оригинальный диск, обновление не нужно |
| **Контрольная сумма ISO** | MD5 `aad50e5418e22e8a9b92d2c0c258079e`, CRC32 `04843ff9` |
| **SHA-1 `default.xex`** | `c5970941fcb201dda99de0d35b623827241d6b8b` |

Другие издания не проверялись. *Viva Piñata* (2006) и *Viva Piñata: Party Animals* — другие игры; первая часть есть в проекте [Viva Piñata Recomp](https://github.com/crabinacrabic/VivaPinataRecomp).

## Простой способ: установка через ИИ-агента

1. Установите ИИ-ассистента, который умеет выполнять команды на компьютере, например [Claude Code](https://claude.com/claude-code), OpenAI Codex или Cursor.
2. Вставьте в него это сообщение, указав настоящий путь к ISO:

   > Установи Viva Piñata: Trouble in Paradise Recomp на этот ПК: https://github.com/crabinacrabic/VivaPinataTroubleInParadiseRecomp — следуй AGENTS.md из этого репозитория. Мой образ диска лежит в `C:\путь\к\Viva Pinata - Trouble in Paradise (World).iso`. Русский язык: не нужен / нужен.

3. Агент всё скачает и проверит. Когда он попросит, пройдите установщик Visual Studio и нажмите **Сборка** в Visual Studio.
4. Когда он закончит, запустите `out\build\local-win-relwithdebinfo\retip.exe`.

## Установка своими руками (6 шагов)

1. **Установите инструменты.** Нужны:
   - [Visual Studio 2026 Community](https://visualstudio.microsoft.com/) (бесплатная) с рабочей нагрузкой **Разработка классических приложений на C++** и компонентом **Средства C++ Clang для Windows**;
   - [Git](https://git-scm.com/).
2. **Скачайте проект.** Путь к папке должен содержать только английские буквы:
   ```bash
   git clone https://github.com/crabinacrabic/VivaPinataTroubleInParadiseRecomp.git C:\Games\VivaPinataTiPRecomp
   ```
3. **Распакуйте игру** программой [extract-xiso](https://github.com/XboxDev/extract-xiso/releases/latest) (файл `extract-xiso-Win64_Release.zip`):
   ```bash
   extract-xiso -x -d C:\Games\VivaPinataTiPRecomp\assets "C:\путь\к\Viva Pinata - Trouble in Paradise (World).iso"
   ```
   После этого в `assets` должны лежать `default.xex` и папка `Beta`.
4. **Соберите.**
   - В Visual Studio: **Файл → Открыть → Папка…** → выберите `C:\Games\VivaPinataTiPRecomp`.
   - Дождитесь в окне «Вывод» сообщения о завершении генерации CMake. В первый раз это займёт несколько минут: скачивается SDK и преобразуется код игры.
   - На панели инструментов выберите конфигурацию **`local-win-relwithdebinfo`** (не `local-win-debug`, см. [ниже](#если-что-то-не-так)).
   - Нажмите **Сборка → Собрать все** (`F7`).
5. **Запустите:** `Ctrl+F5` в Visual Studio или двойной щелчок по `out\build\local-win-relwithdebinfo\retip.exe`. Игра запускается в окне.
6. **В лаунчере** нажмите **PLAY** (или `Enter`).
   - В **OPTIONS**: соотношение сторон (от 16:9 до 32:9 или своё), разрешение (720p / 1440p), качество графики, полный экран, счётчик и ограничение FPS, пропуск вступительных роликов и язык текста игры.
   - Кнопка в правом верхнем углу переключает полный экран и окно. Снимите галочку **Show this launcher on startup**, чтобы сразу попадать в игру.

## Русский язык

На диске Xbox русского нет, и перевода этой игры не существовало, поэтому он сделан для этого проекта:
- около 4 400 строк и названия (пинаты, персонажи, места) взяты из перевода ПК-версии первой части от **ZoG Team** ([zoneofgames.ru](https://www.zoneofgames.ru/));
- остальные 4 600 строк переведены с помощью Google Gemini с теми же названиями;
- [`tools/tip_text.py`](tools/tip_text.py) проверяет все 9 019 строк: разметку, значки кнопок, имена, которые подставляет игра, и то, что текст помещается в блоки текста игры.

Названия и строки ZoG Team используются с разрешения команды. Установка:

1. Скачайте **[VivaPinataTiP_Russian_v1.zip](https://disk.yandex.ru/d/BMFSpKGapzwuaw)** (Яндекс Диск, 319 КБ). В нём только русский текст, файлов игры нет.
2. Распакуйте его в папку проекта, чтобы появился файл `translation\tip_russian.json`.
3. Установите [Python 3](https://www.python.org/), затем выполните в папке проекта:
   ```bash
   python tools/tip_text.py build
   ```
   За несколько секунд появится `assets/Beta/bundles/russian.bnl`.
4. В лаунчере выберите **OPTIONS → Language → Game text → Russian**.

Выбор English возвращает оригинальный файл.

## Управление

Игра рассчитана на геймпад Xbox, он работает сразу. Можно играть и мышью с клавиатурой:

| Мышь / клавиатура | Геймпад | | Мышь / клавиатура | Геймпад |
| :-- | :-- | :-- | :-- | :-- |
| Движение мыши | поворот камеры | | `E` | **RB** |
| Колесо мыши | приближение | | `Num +` / `Num −` | **LT** / **RT** |
| Левая кнопка | **A** | | `1` `2` `3` `Q` | крестовина вверх / вправо / влево / вниз |
| Правая кнопка | **B** | | `Shift` / `M` | **L3** / **R3** |
| Средняя кнопка, `Enter` | **X** | | `C` | медленное движение |
| Кнопка мыши 5, `I` | **Y** | | `Esc`, кнопка мыши 4 | **Back** |

Все клавиши меняются в `retip.toml` рядом с `retip.exe` (`keybind_*`).

Дополнительные клавиши проекта:
- удерживайте **Back** на геймпаде полсекунды: меню инструментов ReTiP (появление пинат и растений, игрок, графика, масштабирование, настройки);
- `Shift+Esc` или `End`: выход из игры (с подтверждением).

## Настройки

Почти всё настраивается в лаунчере. Остальное — в `retip.toml` рядом с `retip.exe` (создаётся из [`packaged/retip.toml`](packaged/retip.toml) при первой сборке).

Там же перечислены моды из [`packaged/mods`](packaged/mods). С ReXGlue SDK 0.10 работают только моды данных (PureTagColors); моды текстур и шейдеров (WaterFix, DitherFix, IceShader, BorderlessTag, CustomProfileTag) требовали GPU-слоя исходного форка SDK и пока неактивны.

## Что работает

| | |
| :-- | :-- |
| ✅ | Титульный экран, меню и сад |
| ✅ | Звук, геймпад Xbox, мышь и клавиатура |
| ✅ | Лаунчер с настройками экрана и качества, меню инструментов ReTiP |
| ✅ | Русский текст |
| 🚧 | Моды текстур и шейдеров неактивны (моды данных работают) |
| 🚧 | Только Direct3D 12 |
| 🚧 | Камера Xbox Live Vision и сетевая игра не поддерживаются |

## Если что-то не так

| Проблема | Что делать |
| :-- | :-- |
| В лаунчере «Game files not found», **PLAY** серая | Проверьте, что есть `assets\default.xex` и `assets\Beta` |
| Игра закрывается через несколько секунд после **PLAY** в сборке `local-win-debug` | Ошибка в отладочной версии SDK (его отрисовщик интерфейса использует недействительный итератор, когда освобождает текстуру). Собирайте `local-win-relwithdebinfo` |
| Visual Studio останавливается на «нарушении прав доступа» при запуске через `F5` | Это не падение. Отключите остановку на `0xC0000005` (Отладка → Окна → Параметры исключений) или запускайте через `Ctrl+F5` |
| Ошибка сборки `Microsoft Visual C/C++ Version differs in precompiled file` | Обновилась Visual Studio. Удалите `out\build\local-win-relwithdebinfo` и соберите заново |
| Генерация кода падает с `0xC0000409` | В пути к проекту есть русские буквы. Перенесите проект, например в `C:\Games\VivaPinataTiPRecomp` |

Логи пишутся в `out\build\local-win-relwithdebinfo\logs\`.

## Для разработчиков

- [`docs/PORTING_0.10.md`](docs/PORTING_0.10.md): что изменилось при переносе с исходного форка SDK на ReXGlue SDK 0.10.
- [`tools/tip_text.py`](tools/tip_text.py): `extract`, `check` и `build` для текста игры. Файл текста — контейнер Rare CAFF: каждый блок сохраняет свой размер, а поток текста — точный сжатый размер, потому что игра распаковывает его на месте.
- [`translation/GEMINI_BRIEF.md`](translation/GEMINI_BRIEF.md): задание, по которому делался перевод.

## Благодарности

- [SolarCookies/TiP-Recomp](https://github.com/SolarCookies/TiP-Recomp): исходный проект (ReTiP)
- [ReXGlue SDK](https://github.com/rexglue/rexglue-sdk): рекомпилятор и среда выполнения
- [Xenia](https://github.com/xenia-project/xenia) и [Xenia Canary](https://github.com/xenia-canary/xenia-canary): графический движок
- ZoG Team ([Zone of Games](https://www.zoneofgames.ru/)): русский перевод первой части, основа для названий и многих строк
- Rare: за чудесную игру

## Правовая информация

Проект не связан с Microsoft или Rare и не одобрен ими. Файлов игры в нём нет; нужна своя законно полученная копия. Viva Piñata — товарный знак Microsoft Corporation. У исходного TiP-Recomp не указана лицензия, поэтому его код остаётся под авторским правом автора.
