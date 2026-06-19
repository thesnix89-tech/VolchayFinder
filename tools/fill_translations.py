#!/usr/bin/env python3
"""Fill Russian and Ukrainian translations in Qt .ts files."""

from __future__ import annotations

import re
import xml.etree.ElementTree as ET
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
I18N = ROOT / "i18n"

RU: dict[str, str] = {
    "Wi-Fi": "Wi-Fi",
    "Off": "Выкл.",
    "Bluetooth": "Bluetooth",
    "AirDrop": "AirDrop",
    "Receiving: Off": "Получение: Выкл.",
    "Focus": "Фокусирование",
    "Screen Mirroring": "Повтор экрана",
    "Display": "Дисплей",
    "Sound": "Звук",
    "Music": "Музыка",
    "Finder Preferences": "Настройки Finder",
    "Settings": "Настройки",
    "Desktop & Dock": "Рабочий стол и Док",
    "Explorer": "Проводник",
    "Interface language": "Язык интерфейса",
    "Choose the language for menus and settings": "Выберите язык меню и настроек",
    "Automatically hide the Windows taskbar": "Автоматически скрывать панель задач Windows",
    "Hides the standard taskbar for a cleaner look": "Скрывает стандартную панель задач для лучшего вида",
    "Keep the Windows taskbar hidden after exit": "Оставить панель задач Windows скрытой после выхода",
    'On exit, keeps Windows "Automatically hide the taskbar" enabled': "При выходе сохраняет включённой настройку «Автоматически скрывать панель задач» в Windows",
    "Pin apps from the Windows taskbar": "Прикрепить приложения из панели задач Windows",
    "One-time action: click the button, then Apply. Repeat on the next launch or when reopening settings if you want to sync the dock with the taskbar again": "Одноразовое действие: нажмите кнопку, затем «Применить». При следующем запуске или открытии настроек нужно повторить, если снова хотите синхронизировать док с панелью задач",
    "Pin from Windows taskbar": "Прикрепить из панели задач Windows",
    "Show macOS menu bar": "Показывать строку меню macOS",
    "Displays the status bar at the top of the screen": "Отображает статус-бар в верхней части экрана",
    "Dock icon size": "Размер иконок дока",
    "Choose the dock icon size in pixels": "Выбор размера значков панели (в пикселях)",
    "Bounce icons on hover": "Подпрыгивание иконок при наведении",
    "Icons lift when you hover over them": "Иконки приподнимаются, когда вы наводите на них курсор",
    "Fade icons while dragging": "Делать иконки прозрачными во время перетаскивания",
    "Outside the dock the icon is semi-transparent; inside it stays opaque, like on macOS": "Вне области дока иконка полупрозрачная, внутри — непрозрачная, как на macOS",
    "Appearance": "Оформление",
    "Choose the look of the dock and menu bar": "Выберите внешний вид дока и строки меню",
    "Auto": "Авто",
    "Light": "Светлая",
    "Dark": "Тёмная",
    "Dark theme": "Тёмная тема",
    "Dark dock and top bar in macOS Monterey style": "Тёмный док и верхняя панель в стиле macOS Monterey",
    "Dock background (light theme)": "Фон дока (светлая тема)",
    "On dark theme the dock is always black. This choice applies only to the light theme.": "На тёмной теме док всегда чёрный. Этот выбор применяется только к светлой теме.",
    "White": "Белый",
    "Current default": "Как сейчас",
    "macOS 27": "macOS 27",
    "Gray like on Mac": "Серый как на Mac",
    "Static dock icons": "Статичные иконки дока",
    "Icons do not move or magnify on hover": "Иконки не двигаются и не увеличиваются при наведении",
    "Pin new apps in a separate section": "Закреплять новые приложения в отдельной стороне",
    "Unpinned running apps appear between pinned icons and the trash, like on macOS": "Незакреплённые запущенные программы появляются между закреплёнными иконками и корзиной, как на macOS",
    "Start with Windows": "Запускать с Windows",
    "Automatically start the shell when you sign in": "Автоматически запускать оболочку при входе в систему",
    "Menu bar icon": "Иконка меню",
    "Icon on the left side of the top bar": "Иконка слева в верхней панели",
    "Like on Mac": "Как на Mac",
    "Star": "Звёздочка",
    "Grid": "Сетка",
    "Custom": "Своя",
    "From file": "Из файла",
    "Import…": "Загрузить…",
    "Explorer icon": "Иконка проводника",
    "How to display File Explorer in the dock": "Как отображать File Explorer в доке",
    "Default": "Стандартная",
    "Trash icon": "Иконка корзины",
    "How to display the trash in the dock": "Как отображать корзину в доке",
    "Downloads folder in dock": "Папка «Загрузки» в доке",
    "Show the downloads folder next to the trash": "Показывать папку загрузок рядом с корзиной",
    "Quit": "Выход",
    "Apply": "Применить",
    "Downloads": "Загрузки",
    "Trash": "Корзина",
    "Finder": "Finder",
    "File Explorer": "Проводник",
    "Settings…": "Системные настройки…",
    "Exit shell": "Выход из оболочки",
    "Remove": "Удалить",
    "%1 (unpinned)": "%1 (не закреплено)",
    "Options": "Параметры",
    "Show in Explorer": "Показать в Проводнике",
    "Show all windows": "Показать все окна",
    "Hide": "Скрыть",
    "Open Downloads folder": "Открыть папку «Загрузки»",
    "Open": "Открыть",
    "Empty Trash": "Очистить корзину",
    "No recent downloads": "Нет недавних загрузок",
    "Choose menu bar icon": "Выберите иконку меню",
    "System": "Системный",
    "File": "Файл",
    "Edit": "Правка",
    "View": "Вид",
    "Go": "Переход",
    "Window": "Окно",
    "Help": "Справка",
}

UK: dict[str, str] = {
    "Wi-Fi": "Wi-Fi",
    "Off": "Вимк.",
    "Bluetooth": "Bluetooth",
    "AirDrop": "AirDrop",
    "Receiving: Off": "Отримання: Вимк.",
    "Focus": "Фокусування",
    "Screen Mirroring": "Дублювання екрана",
    "Display": "Дисплей",
    "Sound": "Звук",
    "Music": "Музика",
    "Finder Preferences": "Налаштування Finder",
    "Settings": "Налаштування",
    "Desktop & Dock": "Робочий стіл і Dock",
    "Explorer": "Провідник",
    "Interface language": "Мова інтерфейсу",
    "Choose the language for menus and settings": "Оберіть мову меню та налаштувань",
    "Automatically hide the Windows taskbar": "Автоматично ховати панель завдань Windows",
    "Hides the standard taskbar for a cleaner look": "Ховає стандартну панель завдань для кращого вигляду",
    "Keep the Windows taskbar hidden after exit": "Залишати панель завдань Windows прихованою після виходу",
    'On exit, keeps Windows "Automatically hide the taskbar" enabled': "Після виходу зберігає увімкненим параметр Windows «Автоматично ховати панель завдань»",
    "Pin apps from the Windows taskbar": "Закріпити програми з панелі завдань Windows",
    "One-time action: click the button, then Apply. Repeat on the next launch or when reopening settings if you want to sync the dock with the taskbar again": "Одноразова дія: натисніть кнопку, потім «Застосувати». Повторіть під час наступного запуску або відкриття налаштувань, якщо знову хочете синхронізувати dock із панеллю завдань",
    "Pin from Windows taskbar": "Закріпити з панелі завдань Windows",
    "Show macOS menu bar": "Показувати рядок меню macOS",
    "Displays the status bar at the top of the screen": "Показує рядок стану у верхній частині екрана",
    "Dock icon size": "Розмір іконок dock",
    "Choose the dock icon size in pixels": "Вибір розміру значків панелі (у пікселях)",
    "Bounce icons on hover": "Підстрибування іконок при наведенні",
    "Icons lift when you hover over them": "Іконки піднімаються, коли ви наводите на них курсор",
    "Fade icons while dragging": "Робити іконки прозорими під час перетягування",
    "Outside the dock the icon is semi-transparent; inside it stays opaque, like on macOS": "Поза dock іконка напівпрозора; всередині — непрозора, як на macOS",
    "Appearance": "Оформлення",
    "Choose the look of the dock and menu bar": "Виберіть зовнішній вигляд dock і рядка меню",
    "Auto": "Авто",
    "Light": "Світла",
    "Dark": "Темна",
    "Dark theme": "Темна тема",
    "Dark dock and top bar in macOS Monterey style": "Темний dock і верхня панель у стилі macOS Monterey",
    "Dock background (light theme)": "Фон dock (світла тема)",
    "On dark theme the dock is always black. This choice applies only to the light theme.": "На темній темі dock завжди чорний. Цей вибір застосовується лише до світлої теми.",
    "White": "Білий",
    "Current default": "Як зараз",
    "macOS 27": "macOS 27",
    "Gray like on Mac": "Сірий як на Mac",
    "Static dock icons": "Статичні іконки dock",
    "Icons do not move or magnify on hover": "Іконки не рухаються і не збільшуються при наведенні",
    "Pin new apps in a separate section": "Закріплювати нові програми в окремій секції",
    "Unpinned running apps appear between pinned icons and the trash, like on macOS": "Незакріплені запущені програми з’являються між закріпленими іконками та кошиком, як на macOS",
    "Start with Windows": "Запускати з Windows",
    "Automatically start the shell when you sign in": "Автоматично запускати оболонку під час входу в систему",
    "Menu bar icon": "Іконка меню",
    "Icon on the left side of the top bar": "Іконка зліва у верхній панелі",
    "Like on Mac": "Як на Mac",
    "Star": "Зірочка",
    "Grid": "Сітка",
    "Custom": "Власна",
    "From file": "З файлу",
    "Import…": "Завантажити…",
    "Explorer icon": "Іконка провідника",
    "How to display File Explorer in the dock": "Як показувати File Explorer у dock",
    "Default": "Стандартна",
    "Trash icon": "Іконка кошика",
    "How to display the trash in the dock": "Як показувати кошик у dock",
    "Downloads folder in dock": "Папка «Завантаження» в dock",
    "Show the downloads folder next to the trash": "Показувати папку завантажень поруч із кошиком",
    "Quit": "Вихід",
    "Apply": "Застосувати",
    "Downloads": "Завантаження",
    "Trash": "Кошик",
    "Finder": "Finder",
    "File Explorer": "Провідник",
    "Settings…": "Системні налаштування…",
    "Exit shell": "Вихід з оболонки",
    "Remove": "Видалити",
    "%1 (unpinned)": "%1 (не закріплено)",
    "Options": "Параметри",
    "Show in Explorer": "Показати у Провіднику",
    "Show all windows": "Показати всі вікна",
    "Hide": "Сховати",
    "Open Downloads folder": "Відкрити папку «Завантаження»",
    "Open": "Відкрити",
    "Empty Trash": "Очистити кошик",
    "No recent downloads": "Немає недавніх завантажень",
    "Choose menu bar icon": "Оберіть іконку меню",
    "System": "Системна",
    "File": "Файл",
    "Edit": "Редагувати",
    "View": "Перегляд",
    "Go": "Перехід",
    "Window": "Вікно",
    "Help": "Довідка",
}


def apply_translations(ts_path: Path, mapping: dict[str, str] | None) -> None:
    tree = ET.parse(ts_path)
    root = tree.getroot()
    for message in root.iter("message"):
        source_el = message.find("source")
        translation_el = message.find("translation")
        if source_el is None or translation_el is None or source_el.text is None:
            continue
        source = source_el.text
        if mapping is None:
            translation_el.text = source
        else:
            translation_el.text = mapping.get(source, source)
        if "type" in translation_el.attrib:
            del translation_el.attrib["type"]
    tree.write(ts_path, encoding="utf-8", xml_declaration=True)


def main() -> None:
    apply_translations(I18N / "MacDockShell_en.ts", None)
    apply_translations(I18N / "MacDockShell_ru.ts", RU)
    apply_translations(I18N / "MacDockShell_uk.ts", UK)
    print("Translations filled.")


if __name__ == "__main__":
    main()
