# UI Stack Decision

Выбран стек первого визуального этапа:
- C++17
- Qt 6
- Qt Quick / QML
- модульная структура компонентов интерфейса

Причины выбора:
- QML быстрее для сборки насыщенного desktop UI с анимацией
- легко реализовать glass, blur-подобные поверхности, transitions и hover-эффекты
- удобно отделить визуальную часть от будущей Windows-интеграции
- дальнейший C++ backend можно подключить без переписывания UI

Структура UI-слоя:
- main.cpp — инициализация QGuiApplication и QQmlApplicationEngine
- Main.qml — корневая сцена
- components/TopBar.qml — верхняя панель
- components/Dock.qml — нижний dock
- components/DockItem.qml — элемент dock
- components/GlassPanel.qml — универсальная glass-подложка
- components/StatusArea.qml — блок системных индикаторов
- theme/Theme.qml — цвета, размеры, радиусы, отступы

Следующий этап после GUI:
- подключить backend-модель приложений
- реализовать нативный blur/mica на стороне Windows
- связать dock с процессами и окнами ОС
