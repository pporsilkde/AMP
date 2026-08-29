# X032 — Launcher preset separation / Auto + shadows fix

## Главное

- Графические пресеты снова отдельные: Auto / Minimum / Low / Balanced / Medium / High / Ultra.
- Никакого MMO в Graphics и OSG Patch presets.
- CO-OP / MMO — только игровой/серверный preset на главной Play-странице.
- MMO является дефолтом для нового ArenaMP server config.
- Применение CO-OP/MMO изменяет только server/scripts/config.lua и не обращается к GraphicsPage/Settings::Manager.

## Исправление Auto graphics

- autoSelect по умолчанию снова true.
- qualityMode по умолчанию снова auto.
- первый запуск больше не принуждает graphics level 2; применяется реальная hardware recommendation.
- если hardware-auto вручную выключен, Auto использует Balanced fallback.
- unknown GPU больше не считается software renderer: базово используется Balanced (или Low на слабом CPU), с обычной поправкой на 4K.
- одноразовая миграция исправляет X031 default pair qualityMode=2 + autoSelect=false обратно в независимый Auto mode.

## Исправление теней

- UI учитывает master [Shadows] enable shadows. Старые stale actor/object flags больше не показываются как активные при глобально выключенных тенях.
- Shadow distance / resolution / bounds controls активируются только если выбран хотя бы один caster.
- При linked shadow distance значение остаётся min(viewing distance, 16384).
- Minimum: все realtime shadows выключены.
- Low: player + actor.
- Balanced: player + actor + indoor.
- Medium: + object.
- High/Ultra: + terrain.

## Главная Play-страница ArenaMP

- Gameplay preset: MMO (default), CO-OP.
- Определение режима идёт по itemData, а не по индексу списка.
- Удалены элементы Connect to vanilla-build server и Hide chat messages.
- Удалены связанные PlayPage/MainDialog signals, settings и launch arguments.
- Старые launcher keys General/Server/vanillaBuild и General/Chat/hideHistory очищаются при сохранении.
- build.ini vanillaServerCompatibility при следующей записи принудительно false.

## MMO default server config

Новый config.lua использует gameMode = "ArenaMP MMO" и по умолчанию не шарит journal/faction ranks/faction reputation/topics/reputation/videos. CO-OP preset включает эти поля обратно через основную страницу launcher.
