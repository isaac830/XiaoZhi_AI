#!/usr/bin/env python3
import os
import sys

from PIL.Image import Resampling

import spiffsgen
import esptool
import math
import tkinter as tk
from tkinter import ttk
import serial.tools.list_ports
from tkinter import filedialog
from tkinter import messagebox
from PIL import Image
import csv


def find_csv_file(folder):
    csv_files = []
    for root, _, files in os.walk(folder):  # 遍历目录树
        for file in files:
            if file.lower().endswith(".csv"):  # 检查文件扩展名
                csv_files.append(os.path.join(root, file))  # 拼接完整路径
    return csv_files


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


def has_alpha(image):
    return image.mode in ("RGBA", "LA") or (image.mode == "P" and "transparency" in image.info)


def convert_to_rgb565(image, width=None, height=None):
    # 将图像转换为 RGB 模式
    image = image.convert("RGB")
    # 创建一个新的图像对象，用于存储 RGB565 数据
    if width is not None and height is not None:
        image = image.resize((width, height), Resampling.LANCZOS)
    rgb565_image = Image.new("RGB", image.size)
    # 遍历每个像素
    for y in range(image.height):
        for x in range(image.width):
            # 获取像素的 RGB 值
            r, g, b = image.getpixel((x, y))
            # 将 RGB888 转换为 RGB565
            r = (r >> 3) & 0x1F  # 取红色高 5 位
            g = (g >> 2) & 0x3F  # 取绿色高 6 位
            b = (b >> 3) & 0x1F  # 取蓝色高 5 位
            rgb565 = (r << 11) | (g << 5) | b  # 合并为 RGB565
            # 将 RGB565 转换回 RGB888（用于保存）
            r = (rgb565 >> 11) & 0x1F
            g = (rgb565 >> 5) & 0x3F
            b = rgb565 & 0x1F
            r = (r << 3) | (r >> 2)  # 扩展为 8 位
            g = (g << 2) | (g >> 4)  # 扩展为 8 位
            b = (b << 3) | (b >> 2)  # 扩展为 8 位
            # 将像素写入新图像
            rgb565_image.putpixel((x, y), (r, g, b))
    return rgb565_image


def process_images_in_directory(directory):
    for root, _, files in os.walk(directory):
        for file in files:
            if file.lower().endswith(".png"):
                file_path = os.path.join(root, file)
                try:
                    with Image.open(file_path) as img:
                        # 检查是否有 Alpha 通道
                        if not has_alpha(img):
                            print(f"处理中: {file_path}")
                            # 转换为 16 位 PNG 格式
                            img_16bit = convert_to_rgb565(img)
                            # 覆盖原图片
                            img_16bit.save(file_path, "PNG", optimize=True)
                            print(f"已转换并覆盖: {file_path}")
                        else:
                            print(f"跳过（包含 Alpha 通道）: {file_path}")
                except Exception as e:
                    print(f"处理 {file_path} 时出错: {e}")


def get_folder_size(path):
    total_size = 0
    with os.scandir(path) as it:
        for entry in it:
            if entry.is_file():
                total_size += entry.stat().st_size
            elif entry.is_dir():
                total_size += get_folder_size(entry.path)
    return total_size


def _main():
    def select_directory():
        # 打开目录选择窗口
        directory = filedialog.askdirectory()
        if directory:
            # 将选择的目录显示在文本框中
            text_field.delete(0, tk.END)  # 清空文本框
            text_field.insert(0, directory)  # 插入选择的目录

    def scan_usb_ports():
        # 清空现有的 USB 列表
        usb_combobox['values'] = []  # 先清空值

        # 列出所有串口
        ports = serial.tools.list_ports.comports()
        port_names = [port.device for port in ports if 'usb' in port.device.lower()]

        # 将可用串口添加到下拉菜单中
        usb_combobox['values'] = port_names
        if len(port_names) > 0:
            usb_combobox.set(port_names[0])

    def on_select(event):
        selected_port = usb_combobox.get()
        usb_combobox.set(selected_port)  # 选择后清空选择

    def on_combobox_expand():
        selected_port = usb_combobox.get()
        if len(selected_port) == 0:
            scan_usb_ports()

    def burn():
        port_name = usb_combobox.get()
        if len(port_name) == 0:
            messagebox.showwarning("警告", "没有指定USB端口，请先选择一个端口")
            return
        spiffs_folder = text_field.get()
        if len(spiffs_folder) == 0:
            messagebox.showwarning("警告", "请先选择要烧录的目录")
            return
        # 读取partitions.csv
        part_Offset = get_item_value('../partitions.csv', "spiffs", "Offset")
        part_Size = get_item_value('../partitions.csv', "spiffs", "Size")
        if not part_Offset or not part_Size:
            messagebox.showwarning("警告", "partitions.csv配置文件缺少分区信息,无法烧录")
        size = get_folder_size(spiffs_folder)
        blockSize = math.ceil(size / 4096) * 4096
        if blockSize > int(part_Size, 16):
            messagebox.showwarning("警告", f"文件夹中的文件大小大于{part_Size},无法烧录")
            return
        # csv_files = find_csv_file(spiffs_folder)
        # if len(csv_files) == 0:
        #     messagebox.showwarning("警告", "缺少csv配置文件,无法烧录")
        #     return
        # bgImg = get_item_value(csv_files[0], "背景", "资源文件")
        # if len(bgImg) > 0:
        #     fullpath = spiffs_folder + "/" + bgImg
        #     with Image.open(fullpath) as img:
        #         # 转换为 16 位 PNG 格式
        #         img_16bit = convert_to_rgb565(img, width=320, height=240)
        #         img_16bit.save(fullpath, "PNG", optimize=True)
        # process_images_in_directory(spiffs_folder)
        sys.argv = ["spiffsgen.py", part_Size, spiffs_folder, "spiffs_image.bin"]
        spiffsgen.main()
        sys.argv = [
            "esptool.py",
            "--chip", "esp32s3",
            "--port", port_name,
            "--baud", "115200",
            "write_flash",
            "-z", part_Offset,
            "spiffs_image.bin"
        ]
        try:
            esptool.main()
        except Exception as e:
            messagebox.showwarning("错误", e)
            return
        messagebox.showinfo("信息", "烧录成功")
        burn_btn.config(text="开始烧录")

    # 创建主窗口
    root = tk.Tk()
    root.title("Cubicat烧录器")
    # 获取屏幕的宽度和高度
    screen_width = root.winfo_screenwidth()
    screen_height = root.winfo_screenheight()
    width = 320
    height = 180
    # 计算窗口居中的坐标
    x = (screen_width - width) // 2
    y = (screen_height - height) // 2

    # 设置窗口的位置
    root.geometry(f"{width}x{height}+{x}+{y}")

    frame_port = tk.Frame(root)
    frame_port.pack(anchor="nw", padx=10, pady=0)

    label = ttk.Label(frame_port, text="端口:")
    label.pack(side=tk.LEFT, pady=0)
    # 创建下拉菜单（Combobox）
    usb_combobox = ttk.Combobox(frame_port, state="readonly")
    usb_combobox.pack(side=tk.RIGHT, padx=10, pady=20)
    usb_combobox.bind("<<ComboboxSelected>>", on_select)  # 绑定选择事件
    usb_combobox.configure(postcommand=on_combobox_expand)

    # 创建一个框架以组合文本框和按钮
    frame = tk.Frame(root)
    frame.pack(anchor="nw", padx=10, pady=0)

    label1 = ttk.Label(frame, text="目录:")
    label1.pack(side=tk.LEFT, pady=0)

    # 创建文本框
    text_field = tk.Entry(frame, width=20)
    text_field.pack(side=tk.LEFT, padx=(10, 0))  # 文本框在左侧

    # 创建按钮以打开目录选择窗口
    select_button = tk.Button(frame, text="选择目录", command=select_directory, width=4)
    select_button.pack(side=tk.RIGHT)  # 按钮在右侧

    # 创建烧录按钮
    burn_btn = tk.Button(root, text="开始烧录", command=burn)
    burn_btn.pack(pady=20)

    scan_usb_ports()
    # 运行主事件循环
    root.mainloop()


if __name__ == '__main__':
    _main()
