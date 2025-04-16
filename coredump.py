#!/usr/bin/env python3
import csv
import os
import sys
from tkinter import messagebox

import serial.tools.list_ports
import esptool
import espcoredump


def get_item_value(csv_file, item_id, key):
    try:
        with open(csv_file, "r", newline="", encoding="utf-8") as file:
            reader = csv.DictReader(file)  # 将 CSV 文件解析为字典
            cleaned_data = [
                {key.strip(): value for key, value in row.items()}
                for row in reader
            ]
            for row in cleaned_data:
                idName = reader.fieldnames[0]
                if row[idName] == item_id:
                    if key in row:
                        return row[key].strip()
    except Exception as e:
        messagebox.showwarning("警告", f"找不到 csv 文件: {csv_file}")
    return None


def find_elf_files(root_dir):
    elf_files = []
    for root, _, files in os.walk(root_dir):
        for file in files:
            if file.endswith(".elf"):
                elf_files.append(os.path.join(root, file))
    return elf_files


def main():
    dump_Offset = get_item_value('partitions.csv', "coredump", "Offset")
    dump_Size = get_item_value('partitions.csv', "coredump", "Size")
    if not dump_Offset or not dump_Size:
        messagebox.showwarning("警告", "partitions.csv配置文件缺少coredump分区信息,无法dump")
        return

    ports = serial.tools.list_ports.comports()
    port_names = [port.device for port in ports if 'usb' in port.device.lower()]
    if len(port_names) == 0:
        messagebox.showwarning("警告", " 找不到端口, 设备是否连接?")
        return

    print(port_names)
    sys.argv = [
        "esptool.py",
        "-p", port_names[0],
        "read_flash",
        dump_Offset, dump_Size,
        "coredump.bin"
    ]
    esptool.main()
    elfs = find_elf_files("./build")
    if len(elfs) == 0:
        messagebox.showwarning("警告", "./build 目录下没有找到elf文件")
        return
    sys.argv = [
        "espcoredump.py",
        "info_corefile",
        "-t", "elf",
        "coredump.bin",
        elfs[0]
    ]
    espcoredump.main()


if __name__ == '__main__':
    main()
