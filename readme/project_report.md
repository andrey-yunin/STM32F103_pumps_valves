# Исторический отчёт: происхождение проекта

Данный проект (`STM32F103_pumps_valves`) создан путём копирования
проекта шаговых двигателей (`STM32F103_step_motors`) в декабре 2025 года.

Полный отчёт по разработке исполнителя шаговых двигателей находится
в репозитории `STM32F103_step_motors/readme/project_report.md`.

Актуальная документация по исполнителю насосов и клапанов:

- **Архитектура** — `executor_architecture_guide.md` (общая для всех исполнителей)
- **План рефакторинга** — `refactoring_plan.md`
- **Отчёт о работе** — `PUMPS_VALVES_EXECUTOR_REPORT.md`
- **Интерфейс дирижера** — `conductor_interface/` (can_packer.h, device_mapping.h)
