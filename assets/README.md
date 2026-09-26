# assets/

Файлы игры, распакованные из собственного образа диска. Содержимое не коммитится (кроме этого файла).

| | |
| :-- | :-- |
| Диск (Redump) | `Viva Pinata - Trouble in Paradise (World).iso` |
| ISO | MD5 `aad50e5418e22e8a9b92d2c0c258079e`, CRC32 `04843ff9`, SHA-1 `41313f89b413897888b2bdf80b0d88a9d503ac02` |
| Title ID / Media ID | `4D53085F` / `76A44A5D` |
| Версия XEX | `0.0.0.4` (базовая `0.0.0.4`), диск 1 из 1 |
| `default.xex` | SHA-1 `c5970941fcb201dda99de0d35b623827241d6b8b` |

Распаковать заново (extract-xiso):

```bash
extract-xiso -x -d assets "Viva Pinata - Trouble in Paradise (World).iso"
```

Должно получиться:

```
assets/default.xex
assets/Beta/          данные Rare, гостевой путь game:\Beta\...
assets/$SystemUpdate/ системное обновление с диска, не используется
assets/*.png          иконки сохранений
```

Распаковать нужно **до** первой настройки CMake: при настройке запускается кодогенерация по `assets/default.xex`.
