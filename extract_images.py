#!/usr/bin/env python3
"""
Скрипт для извлечения изображений из презентаций
"""
import os
from pathlib import Path
from pptx import Presentation
from pdf2image import convert_from_path
import PyPDF2

def extract_images_from_pptx(pptx_path, slide_numbers, output_dir):
    """Извлекает изображения из PPTX"""
    prs = Presentation(pptx_path)
    images = []

    for slide_num in slide_numbers:
        if slide_num <= len(prs.slides):
            slide = prs.slides[slide_num - 1]
            # Конвертируем весь слайд в изображение
            # Для этого сохраним как PDF и конвертируем
            pass

    return images

def extract_pages_from_pdf(pdf_path, page_numbers, output_dir):
    """Извлекает страницы из PDF как изображения"""
    output_dir = Path(output_dir)
    output_dir.mkdir(exist_ok=True, parents=True)

    images = []

    # Конвертируем указанные страницы в изображения
    for page_num in page_numbers:
        imgs = convert_from_path(
            pdf_path,
            first_page=page_num,
            last_page=page_num,
            dpi=300  # высокое качество
        )

        if imgs:
            filename = f"{Path(pdf_path).stem}_page_{page_num}.png"
            filepath = output_dir / filename
            imgs[0].save(filepath, 'PNG')
            images.append(filepath)
            print(f"Извлечена страница {page_num} из {pdf_path}")

    return images

def main():
    # Создаем директорию для изображений
    img_dir = Path("/home/user/kurs_lu/extracted_images")
    img_dir.mkdir(exist_ok=True)

    # Извлекаем из PDF
    print("Извлечение из PDF файлов...")

    # 1. Боксинг сетевых пакетов (страницы 3-4)
    extract_pages_from_pdf(
        "/home/user/kurs_lu/14_СПО_Сетевые_устройства_и_протоколы.pdf",
        [3, 4],
        img_dir
    )

    # 2. PORT-MAPPED I/O и Memory Mapped (страница 2)
    extract_pages_from_pdf(
        "/home/user/kurs_lu/10.СПО_Работа с оборудованием.pdf",
        [2, 4, 5, 6, 12],
        img_dir
    )

    # 3. Регистры общего назначения (страницы 16-17)
    extract_pages_from_pdf(
        "/home/user/kurs_lu/1-2.СПО_Введение.pdf",
        [16, 17],
        img_dir
    )

    print("\nГотово! Изображения сохранены в:", img_dir)

if __name__ == "__main__":
    main()
