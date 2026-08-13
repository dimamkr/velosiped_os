import os
from pathlib import Path

def simple_collect(root_dir, output_file="code.txt"):
    """Простая версия сбора файлов"""
    root = Path(root_dir)

    with open(output_file, 'w', encoding='utf-8') as out:
        for filepath in root.rglob("*"):
            if filepath.is_file() and not filepath.suffix == '.uid' and not filepath.suffix == '.log':
                try:
                    rel_path = filepath.relative_to(root)
                    out.write(f"#{rel_path}\n")

                    with open(filepath, 'r', encoding='utf-8') as f:
                        out.write(f.read())

                    out.write("\n" + "="*60 + "\n\n")
                    print(f"✓ {rel_path}")

                except UnicodeDecodeError:
                    out.write(f"#{rel_path} - БИНАРНЫЙ ФАЙЛ\n")
                    out.write("="*60 + "\n\n")
                    print(f"✗ {rel_path} (бинарный)")
                except Exception as e:
                    print(f"! {rel_path} (ошибка: {e})")

# Использование
simple_collect("./src", "./ETC/code.txt")
