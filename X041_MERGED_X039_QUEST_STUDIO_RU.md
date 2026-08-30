# ArenaMP X041 MERGED — X039 Quest Studio + X040/X041

Основа: ArenaMP X039 MyGUI Quest Studio cumulative.

Применено из пользовательского ArenaMP_X041_LAUNCHER_LAYOUT_CUMULATIVE:
- X040: применение [OSG] threading model к osgViewer;
- X040: отложенное освобождение CharacterPreview/CharGen RTT-графа;
- X041: XP progression перенесён из Advanced -> Arena Settings в Play -> Server Settings;
- X041: Streaming/occlusion перенесён в Graphics -> Quality;
- X041: XP и occlusion теперь имеют рабочий путь сохранения в settings.cfg;
- X041: обновлена русская локализация launcher.

Сохранено без изменений:
- X039 MyGUI Quest Studio;
- X037 Choices + Quest Manager;
- X036 зелёные server-quest topics и пример Кая;
- X034 Combat AI state sync;
- X033 water/chat;
- X031 occlusion stage2;
- предыдущие cumulative fixes.

Важно:
- после изменения .ui нужна чистая пересборка openmw-launcher/uic;
- X040 особенно проверять с threading model = DrawThreadPerContext.
