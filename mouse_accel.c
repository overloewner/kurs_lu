/*
 * Драйвер мыши с акселерацией (как в Windows)
 * Логика: Быстрое движение = Высокий DPI
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/input.h>
#include <linux/slab.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Student");
MODULE_DESCRIPTION("Mouse Acceleration Driver");
MODULE_VERSION("5.0");

/* === НАСТРОЙКИ === */
#define SPEED_SLOW  200    // Медленное движение: < 200 пикс/сек
#define SPEED_FAST  800    // Быстрое движение: > 800 пикс/сек

/* === ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ === */
static struct {
    struct input_handler handler;
    struct input_dev *virt_mouse;    // Виртуальная мышь

    // Для расчета скорости
    int last_dx, last_dy;            // Последнее движение
    ktime_t last_time;               // Время последнего события

    int current_dpi;                 // Текущий DPI: 50, 100, 200
} driver_data;

/* === ОПРЕДЕЛЕНИЕ DPI ПО СКОРОСТИ === */
static int calculate_dpi(int speed)
{
    if (speed < SPEED_SLOW)
        return 50;   // Медленно → точность
    else if (speed < SPEED_FAST)
        return 100;  // Средняя скорость
    else
        return 200;  // Быстро → скорость
}

/* === ОБРАБОТКА СОБЫТИЙ === */
static bool event_filter(struct input_handle *handle, unsigned int type,
                         unsigned int code, int value)
{
    static int dx = 0, dy = 0;

    /* Движение мыши */
    if (type == EV_REL) {
        if (code == REL_X) {
            dx = value;
        } else if (code == REL_Y) {
            dy = value;
        } else {
            // Колесико - пробрасываем
            if (driver_data.virt_mouse) {
                input_report_rel(driver_data.virt_mouse, code, value);
                input_sync(driver_data.virt_mouse);
            }
        }
        return true;  // Блокируем оригинальное событие
    }

    /* Кнопки мыши */
    if (type == EV_KEY) {
        if (driver_data.virt_mouse) {
            input_report_key(driver_data.virt_mouse, code, value);
            input_sync(driver_data.virt_mouse);
        }
        return true;
    }

    /* Синхронизация - обрабатываем событие */
    if (type == EV_SYN && code == SYN_REPORT) {
        ktime_t now;
        s64 time_diff_us;
        int distance, speed, new_dpi;
        int scaled_dx, scaled_dy;

        // Если мышь не двигалась - ничего не делаем
        if (dx == 0 && dy == 0)
            return true;

        // Текущее время
        now = ktime_get();

        // Расчет скорости
        time_diff_us = ktime_to_us(ktime_sub(now, driver_data.last_time));

        if (time_diff_us > 0 && time_diff_us < 1000000) {  // Максимум 1 секунда
            // Расстояние = |dx| + |dy|
            distance = abs(dx) + abs(dy);

            // Скорость = пиксели / секунда
            speed = (int)((s64)distance * 1000000 / time_diff_us);
        } else {
            // Пауза или первое событие - используем среднюю скорость
            speed = 300;
        }

        // Определяем новый DPI
        new_dpi = calculate_dpi(speed);

        // Если DPI изменился - логируем
        if (new_dpi != driver_data.current_dpi) {
            pr_info("mouse_dpi: CHANGE %d%% -> %d%% (speed=%d px/s)\n",
                    driver_data.current_dpi, new_dpi, speed);
            driver_data.current_dpi = new_dpi;
        }

        // Масштабируем движение
        scaled_dx = (dx * driver_data.current_dpi) / 100;
        scaled_dy = (dy * driver_data.current_dpi) / 100;

        // Отправляем в виртуальную мышь
        if (driver_data.virt_mouse) {
            input_report_rel(driver_data.virt_mouse, REL_X, scaled_dx);
            input_report_rel(driver_data.virt_mouse, REL_Y, scaled_dy);
            input_sync(driver_data.virt_mouse);
        }

        // Сохраняем для следующего раза
        driver_data.last_dx = dx;
        driver_data.last_dy = dy;
        driver_data.last_time = now;

        // Сбрасываем накопленные значения
        dx = dy = 0;

        return true;
    }

    return false;
}

/* === ПОДКЛЮЧЕНИЕ К МЫШИ === */
static int mouse_connect(struct input_handler *handler, struct input_dev *dev,
                         const struct input_device_id *id)
{
    struct input_handle *handle;
    int err;

    // Игнорируем виртуальные устройства
    if (dev->id.bustype == BUS_VIRTUAL)
        return -ENODEV;

    // Только мыши с координатами
    if (!test_bit(EV_REL, dev->evbit) || !test_bit(REL_X, dev->relbit))
        return -ENODEV;

    handle = kzalloc(sizeof(*handle), GFP_KERNEL);
    if (!handle)
        return -ENOMEM;

    handle->dev = dev;
    handle->handler = handler;
    handle->name = "mouse_accel";

    err = input_register_handle(handle);
    if (err)
        goto err_free;

    err = input_open_device(handle);
    if (err)
        goto err_unreg;

    // Захватываем эксклюзивно
    err = input_grab_device(handle);
    if (err)
        pr_warn("mouse_dpi: Cannot grab %s (not critical)\n", dev->name);

    pr_info("mouse_dpi: Connected to %s\n", dev->name);
    return 0;

err_unreg:
    input_unregister_handle(handle);
err_free:
    kfree(handle);
    return err;
}

/* === ОТКЛЮЧЕНИЕ ОТ МЫШИ === */
static void mouse_disconnect(struct input_handle *handle)
{
    pr_info("mouse_dpi: Disconnected from %s\n", handle->dev->name);
    input_close_device(handle);
    input_unregister_handle(handle);
    kfree(handle);
}

/* === ТАБЛИЦА УСТРОЙСТВ === */
static const struct input_device_id mouse_ids[] = {
    {
        .flags = INPUT_DEVICE_ID_MATCH_EVBIT | INPUT_DEVICE_ID_MATCH_RELBIT,
        .evbit = { BIT_MASK(EV_REL) },
        .relbit = { BIT_MASK(REL_X) | BIT_MASK(REL_Y) },
    },
    { }
};

MODULE_DEVICE_TABLE(input, mouse_ids);

/* === HANDLER === */
static struct input_handler mouse_handler = {
    .filter     = event_filter,
    .connect    = mouse_connect,
    .disconnect = mouse_disconnect,
    .name       = "mouse_accel",
    .id_table   = mouse_ids,
};

/* === СОЗДАНИЕ ВИРТУАЛЬНОЙ МЫШИ === */
static int create_virtual_mouse(void)
{
    int err;

    driver_data.virt_mouse = input_allocate_device();
    if (!driver_data.virt_mouse)
        return -ENOMEM;

    driver_data.virt_mouse->name = "Virtual Mouse (Accelerated)";
    driver_data.virt_mouse->id.bustype = BUS_VIRTUAL;
    driver_data.virt_mouse->id.vendor = 0x1234;
    driver_data.virt_mouse->id.product = 0x5678;

    // Поддержка движения
    set_bit(EV_REL, driver_data.virt_mouse->evbit);
    set_bit(REL_X, driver_data.virt_mouse->relbit);
    set_bit(REL_Y, driver_data.virt_mouse->relbit);
    set_bit(REL_WHEEL, driver_data.virt_mouse->relbit);

    // Поддержка кнопок
    set_bit(EV_KEY, driver_data.virt_mouse->evbit);
    set_bit(BTN_LEFT, driver_data.virt_mouse->keybit);
    set_bit(BTN_RIGHT, driver_data.virt_mouse->keybit);
    set_bit(BTN_MIDDLE, driver_data.virt_mouse->keybit);

    err = input_register_device(driver_data.virt_mouse);
    if (err) {
        input_free_device(driver_data.virt_mouse);
        driver_data.virt_mouse = NULL;
        return err;
    }

    pr_info("mouse_dpi: Virtual mouse created\n");
    return 0;
}

/* === ИНИЦИАЛИЗАЦИЯ МОДУЛЯ === */
static int __init mouse_accel_init(void)
{
    int err;

    pr_info("=========================================\n");
    pr_info("mouse_dpi: Acceleration Driver v5.0\n");
    pr_info("mouse_dpi: Logic: Fast = High DPI\n");
    pr_info("mouse_dpi: Slow < %d < Fast < %d < Very Fast\n",
            SPEED_SLOW, SPEED_FAST);
    pr_info("=========================================\n");

    // Инициализация
    memset(&driver_data, 0, sizeof(driver_data));
    driver_data.current_dpi = 100;           // Стартовый DPI
    driver_data.last_time = ktime_get();     // Текущее время

    // Создаем виртуальную мышь
    err = create_virtual_mouse();
    if (err) {
        pr_err("mouse_dpi: Failed to create virtual mouse: %d\n", err);
        return err;
    }

    // Регистрируем обработчик
    err = input_register_handler(&mouse_handler);
    if (err) {
        pr_err("mouse_dpi: Failed to register handler: %d\n", err);
        input_unregister_device(driver_data.virt_mouse);
        return err;
    }

    pr_info("mouse_dpi: Driver loaded successfully\n");
    return 0;
}

/* === ВЫГРУЗКА МОДУЛЯ === */
static void __exit mouse_accel_exit(void)
{
    pr_info("mouse_dpi: Unloading driver\n");

    // Отключаем обработчик
    input_unregister_handler(&mouse_handler);

    // Удаляем виртуальную мышь
    if (driver_data.virt_mouse) {
        input_unregister_device(driver_data.virt_mouse);
        driver_data.virt_mouse = NULL;
    }

    pr_info("mouse_dpi: Driver unloaded\n");
}

module_init(mouse_accel_init);
module_exit(mouse_accel_exit);
