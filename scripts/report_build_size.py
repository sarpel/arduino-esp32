#!/usr/bin/env python3
"""
Build Artifact Size Reporter for ESP32 Audio Streamer
Generates detailed size reports for firmware binaries
"""

import os
import sys
from pathlib import Path
import json
from datetime import datetime

# ANSI colors for terminal output
class Colors:
    HEADER = '\033[95m'
    OKBLUE = '\033[94m'
    OKCYAN = '\033[96m'
    OKGREEN = '\033[92m'
    WARNING = '\033[93m'
    FAIL = '\033[91m'
    ENDC = '\033[0m'
    BOLD = '\033[1m'

def format_size(bytes_size):
    """
    Bayt cinsinden verilen büyüklüğü insan tarafından okunabilir birimle (B, KB, MB, GB) biçimlendirir.
    
    Parameters:
        bytes_size (int | float): Biçimlendirilecek değer, bayt cinsinden.
    
    Returns:
        str: İki ondalık basamak gösterimiyle biçimlendirilmiş büyüklük ve birim (ör. "1.23 KB").
    """
    for unit in ['B', 'KB', 'MB']:
        if bytes_size < 1024.0:
            return f"{bytes_size:.2f} {unit}"
        bytes_size /= 1024.0
    return f"{bytes_size:.2f} GB"

def calculate_percentage(used, total):
    """
    Belirtilen toplam üzerinden kullanılan miktarın yüzdesini hesaplar.
    
    Parameters:
        used (int | float): Kullanılan miktar.
        total (int | float): Yüzdelik hesaplamada referans alınan toplam değer.
    
    Returns:
        float: Kullanılan miktarın toplam içindeki oranı yüzde cinsinden; `total` sıfır ise `0.0`.
    """
    if total == 0:
        return 0.0
    return (used / total) * 100

def get_file_size(filepath):
    """
    Belirtilen dosyanın boyutunu bayt cinsinden döndürür.
    
    Returns:
        int: Dosya boyutu bayt cinsinden; dosya bulunamaz veya erişim hatası varsa `0`.
    """
    try:
        return os.path.getsize(filepath)
    except OSError:
        return 0

def find_build_artifacts(project_root):
    """
    PlatformIO proje kök dizinindeki .pio/build dizinini tarayarak her ortam için derleme artefaktlarının dosya yollarını toplar.
    
    Parameters:
        project_root (pathlib.Path): PlatformIO proje kök dizinine işaret eden Path nesnesi.
    
    Returns:
        dict: Ortam adıyla anahtarlanmış, her biri şu anahtarları içeren sözlükler: 
            - 'firmware_bin': firmware.bin dosya yolu (pathlib.Path)
            - 'firmware_elf': firmware.elf dosya yolu (pathlib.Path)
            - 'partitions_bin': partitions.bin dosya yolu (pathlib.Path)
        None: Eğer .pio/build dizini bulunamazsa None döner.
    """
    artifacts = {}
    pio_dir = project_root / ".pio" / "build"

    if not pio_dir.exists():
        print(f"{Colors.FAIL}Error: Build directory not found. Run 'pio run' first.{Colors.ENDC}")
        return None

    for env_dir in pio_dir.iterdir():
        if env_dir.is_dir():
            env_name = env_dir.name
            artifacts[env_name] = {
                'firmware_bin': env_dir / "firmware.bin",
                'firmware_elf': env_dir / "firmware.elf",
                'partitions_bin': env_dir / "partitions.bin"
            }

    return artifacts

def analyze_elf_sections(elf_path):
    """
    ELF dosyasındaki bölümlerin (sections) bayt cinsinden boyutlarını elde eder.
    
    Parameters:
        elf_path (str | pathlib.Path): Analiz edilecek ELF dosyasına işaret eden yol.
    
    Returns:
        dict: Anahtarları bölüm adları (ör. '.text', '.data', '.bss', '.rodata') ve değerleri her bölümün byte cinsinden boyutu olan sözlük. Eğer analiz başarısız olursa veya bölüm bulunmazsa boş sözlük döner.
    """
    try:
        import subprocess
        result = subprocess.run(
            ['xtensa-esp32-elf-size', '-A', str(elf_path)],
            capture_output=True,
            text=True,
            timeout=10
        )

        if result.returncode != 0:
            # Fallback: try without platform prefix
            result = subprocess.run(
                ['size', '-A', str(elf_path)],
                capture_output=True,
                text=True,
                timeout=10
            )

        sections = {}
        for line in result.stdout.splitlines():
            parts = line.split()
            if len(parts) >= 2 and parts[0].startswith('.'):
                section_name = parts[0]
                try:
                    section_size = int(parts[1])
                    sections[section_name] = section_size
                except ValueError:
                    continue

        return sections
    except Exception as e:
        print(f"{Colors.WARNING}Warning: Could not analyze ELF sections: {e}{Colors.ENDC}")
        return {}

def generate_report(project_root):
    """
    Belirtilen PlatformIO proje kök dizinine göre tüm yapı ortamları için ayrıntılı bir boyut raporu yazdırır.
    
    Parameters:
    	project_root (str | pathlib.Path): PlatformIO proje kök dizininin yolu; fonksiyon .pio/build altında ortam dizinlerini arar.
    
    Returns:
    	int: Çıkış kodu — `0` başarıyla rapor oluşturuldu, `1` .pio/build dizini veya yapı eserleri bulunamadığında.
    """
    print(f"\n{Colors.HEADER}{Colors.BOLD}=" * 70)
    print(f"ESP32 Audio Streamer - Build Artifact Size Report")
    print(f"=" * 70 + Colors.ENDC)
    print(f"Generated: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n")

    artifacts = find_build_artifacts(project_root)
    if not artifacts:
        return 1

    # ESP32 memory limits
    FLASH_SIZE = 4 * 1024 * 1024  # 4MB
    RAM_SIZE = 520 * 1024  # 520KB total (but only ~200KB available for app)

    for env_name, files in artifacts.items():
        print(f"{Colors.OKCYAN}{Colors.BOLD}Environment: {env_name}{Colors.ENDC}")
        print(f"{'-' * 70}")

        # Firmware binary
        firmware_size = get_file_size(files['firmware_bin'])
        if firmware_size > 0:
            flash_percent = calculate_percentage(firmware_size, FLASH_SIZE)
            color = Colors.OKGREEN if flash_percent < 70 else Colors.WARNING if flash_percent < 90 else Colors.FAIL

            print(f"\n{Colors.BOLD}Firmware Binary:{Colors.ENDC}")
            print(f"  Size: {color}{format_size(firmware_size)}{Colors.ENDC}")
            print(f"  Flash Usage: {color}{flash_percent:.1f}%{Colors.ENDC} ({format_size(firmware_size)} / {format_size(FLASH_SIZE)})")

            if flash_percent > 90:
                print(f"  {Colors.FAIL}⚠️  WARNING: Flash usage > 90%!{Colors.ENDC}")
            elif flash_percent > 70:
                print(f"  {Colors.WARNING}⚠️  NOTICE: Flash usage > 70%{Colors.ENDC}")

        # ELF analysis
        if files['firmware_elf'].exists():
            sections = analyze_elf_sections(files['firmware_elf'])
            if sections:
                print(f"\n{Colors.BOLD}Memory Sections:{Colors.ENDC}")

                # Calculate RAM usage (.data + .bss)
                data_size = sections.get('.data', 0)
                bss_size = sections.get('.bss', 0)
                ram_usage = data_size + bss_size
                ram_percent = calculate_percentage(ram_usage, RAM_SIZE)

                print(f"  .text (code):    {format_size(sections.get('.text', 0)):>12}")
                print(f"  .data (init):    {format_size(data_size):>12}")
                print(f"  .bss (uninit):   {format_size(bss_size):>12}")
                print(f"  .rodata (const): {format_size(sections.get('.rodata', 0)):>12}")

                color = Colors.OKGREEN if ram_percent < 50 else Colors.WARNING if ram_percent < 80 else Colors.FAIL
                print(f"\n  {Colors.BOLD}Estimated RAM Usage:{Colors.ENDC} {color}{format_size(ram_usage)}{Colors.ENDC} ({ram_percent:.1f}% of {format_size(RAM_SIZE)})")

                if ram_percent > 80:
                    print(f"  {Colors.FAIL}⚠️  WARNING: RAM usage > 80%!{Colors.ENDC}")

        # Partitions
        partitions_size = get_file_size(files['partitions_bin'])
        if partitions_size > 0:
            print(f"\n{Colors.BOLD}Partitions Table:{Colors.ENDC} {format_size(partitions_size)}")

        print(f"\n{Colors.BOLD}Build Artifacts:{Colors.ENDC}")
        print(f"  firmware.bin: {files['firmware_bin']}")
        print(f"  firmware.elf: {files['firmware_elf']}")

        print(f"\n{'-' * 70}\n")

    print(f"{Colors.OKGREEN}✓ Size report complete{Colors.ENDC}\n")
    return 0

def main():
    """
    PlatformIO proje kök dizinini tespit eder ve proje için build boyutu raporunu üretir.
    
    Çalışma dizininden başlayarak yukarı doğru her üst dizinde `platformio.ini` arar; bir proje kökü bulunamazsa hata mesajı yazdırır ve 1 döner. Proje kökü bulunduğunda `generate_report` çağrılır ve onun döndürdüğü çıkış kodu geri verilir.
    
    Returns:
        int: `generate_report` tarafından döndürülen çıkış kodu; PlatformIO projesi bulunamazsa `1`.
    """
    # Find project root (directory containing platformio.ini)
    current = Path.cwd()
    project_root = None

    for parent in [current] + list(current.parents):
        if (parent / "platformio.ini").exists():
            project_root = parent
            break

    if not project_root:
        print(f"{Colors.FAIL}Error: Not in a PlatformIO project directory{Colors.ENDC}")
        return 1

    return generate_report(project_root)

if __name__ == "__main__":
    sys.exit(main())