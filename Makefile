# Makefile для драйвера мыши с акселерацией

obj-m += mouse_accel.o

KDIR := /lib/modules/$(shell uname -r)/build

all:
	@echo "========================================="
	@echo "Сборка драйвера с АКСЕЛЕРАЦИЕЙ v5.0"
	@echo "Логика: Быстро = ВЫСОКИЙ DPI"
	@echo "========================================="
	$(MAKE) -C $(KDIR) M=$(PWD) modules
	@echo ""
	@echo "✅ Собрано: mouse_accel.ko"
	@echo ""
	@echo "Команды:"
	@echo "  sudo insmod mouse_accel.ko   - Загрузить"
	@echo "  sudo rmmod mouse_accel       - Выгрузить"
	@echo "  dmesg | tail -50             - Посмотреть логи"

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

.PHONY: all clean
