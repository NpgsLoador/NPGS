#!/usr/bin/env python
# -*- coding: utf-8 -*-

import os
import re
import sys

def transform_identifiers(file_path):
    """
    将文件中的 _Identifier 模式替换为 Identifier_ 模式
    要求：
    1. 标识符必须有且只有一个下划线
    2. 标识符首字母必须大写，第二个字母必须小写
    3. 处理特殊情况如 _bBool, _kStatic, _kbBoolStatic
    4. 下划线之前不能有除空白字符或非字母数字字符以外的任何字符
    """
    # 读取文件内容
    with open(file_path, 'r', encoding='utf-8', errors='ignore') as file:
        try:
            content = file.read()
        except UnicodeDecodeError:
            print(f"警告: 无法读取文件 {file_path}，可能是二进制文件或编码问题")
            return False

    # 使用正则表达式查找所有符合条件的标识符模式
    modified_content = content
    modified = False
    
    # 修改正则表达式，确保下划线前只能是空白字符或非字母数字字符
    # (?<!\w) 断言，确保下划线前不是字母数字字符
    
    # 1. 处理标准的 _Identifier 模式 (首字母大写，第二个字母小写)
    pattern1 = r'(?<!\w)_([A-Z][a-z][a-zA-Z0-9]*)\b(?![_])'
    matches1 = re.findall(pattern1, content)
    valid_matches1 = [match for match in matches1 if '_' not in match]
    
    for match in valid_matches1:
        modified_content = re.sub(r'(?<!\w)_' + match + r'\b(?![_])', match + '_', modified_content)
        modified = True
    
    # 2. 处理 _bBool 模式 (前缀b表示布尔变量)
    pattern2 = r'(?<!\w)_(b[A-Z][a-z][a-zA-Z0-9]*)\b(?![_])'
    matches2 = re.findall(pattern2, content)
    valid_matches2 = [match for match in matches2 if '_' not in match]
    
    for match in valid_matches2:
        modified_content = re.sub(r'(?<!\w)_' + match + r'\b(?![_])', match + '_', modified_content)
        modified = True
    
    # 3. 处理 _kStatic 模式 (前缀k表示静态变量)
    pattern3 = r'(?<!\w)_(k[A-Z][a-z][a-zA-Z0-9]*)\b(?![_])'
    matches3 = re.findall(pattern3, content)
    valid_matches3 = [match for match in matches3 if '_' not in match]
    
    for match in valid_matches3:
        modified_content = re.sub(r'(?<!\w)_' + match + r'\b(?![_])', match + '_', modified_content)
        modified = True
    
    # 4. 处理 _kbBoolStatic 模式 (前缀kb表示静态布尔变量)
    pattern4 = r'(?<!\w)_(kb[A-Z][a-z][a-zA-Z0-9]*)\b(?![_])'
    matches4 = re.findall(pattern4, content)
    valid_matches4 = [match for match in matches4 if '_' not in match]
    
    for match in valid_matches4:
        modified_content = re.sub(r'(?<!\w)_' + match + r'\b(?![_])', match + '_', modified_content)
        modified = True
    
    # 如果内容有变化，则写回文件
    if modified and modified_content != content:
        with open(file_path, 'w', encoding='utf-8') as file:
            file.write(modified_content)
        print(f"已更新: {file_path}")
        return True
    
    return False

def process_directory(directory):
    """
    处理指定目录下的所有 cpp、h、inl 文件
    """
    modified_count = 0
    file_count = 0
    
    for root, _, files in os.walk(directory):
        for file in files:
            # 只处理 cpp、h、inl 文件
            if file.endswith(('.cpp', '.h', '.inl', '.hpp')):
                file_path = os.path.join(root, file)
                file_count += 1
                if transform_identifiers(file_path):
                    modified_count += 1
    
    return file_count, modified_count

def main():
    # 使用脚本所在目录作为起始目录
    script_directory = os.path.dirname(os.path.abspath(__file__))
    
    # 如果提供了命令行参数，则使用该参数作为目录
    if len(sys.argv) > 1:
        directory = sys.argv[1]
    else:
        directory = script_directory
    
    print(f"开始处理目录: {directory}")
    
    # 处理目录
    file_count, modified_count = process_directory(directory)
    
    # 打印结果
    print(f"\n处理完成!")
    print(f"检查的文件总数: {file_count}")
    print(f"修改的文件总数: {modified_count}")

if __name__ == "__main__":
    main()