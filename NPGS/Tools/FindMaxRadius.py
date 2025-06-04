import pandas as pd
import os
import numpy as np # 导入 numpy 用于计算 10 的幂

def process_csv_file(file_path):
    """
    处理单个CSV文件，找出log_R最大行的star_age与最后一行star_age的差值，并计算R值。

    Args:
        file_path (str): CSV文件的路径。

    Returns:
        tuple: (log_R最大时的star_age, 文件底部行的star_age, age差值, 最大log_R值, 计算得到的R值)
               如果处理失败则返回 (None, None, None, None, None)。
    """
    try:
        # 读取CSV文件
        df = pd.read_csv(file_path)

        # 确保数据列存在且为数值类型
        if 'log_R' not in df.columns or 'star_age' not in df.columns:
            print(f"  错误: 文件 {os.path.basename(file_path)} 缺少 'log_R' 或 'star_age' 列。")
            return None, None, None, None, None
        
        if df.empty:
            print(f"  错误: 文件 {os.path.basename(file_path)} 为空。")
            return None, None, None, None, None

        df['log_R'] = pd.to_numeric(df['log_R'], errors='coerce')
        df['star_age'] = pd.to_numeric(df['star_age'], errors='coerce')

        # 删除包含NaN的行（转换失败或原始数据中存在）
        df.dropna(subset=['log_R', 'star_age'], inplace=True)

        if df.empty:
            print(f"  错误: 文件 {os.path.basename(file_path)} 在数值转换和NaN移除后为空。")
            return None, None, None, None, None

        # 找到 log_R 最大值的行
        # 如果有多个最大值，idxmax() 会返回第一个出现的索引
        max_log_R_row_index = df['log_R'].idxmax()
        max_log_R_row = df.loc[max_log_R_row_index]
        age_at_max_log_R = max_log_R_row['star_age']
        max_log_R_value = max_log_R_row['log_R']
        
        # 计算 R 值 (10^log_R)
        r_value = np.power(10, max_log_R_value)

        # 获取最后一行的 star_age
        # iloc[-1] 用于获取最后一行
        age_at_bottom = df['star_age'].iloc[-1]

        # 计算差值
        age_difference = age_at_max_log_R - age_at_bottom
        
        return age_at_max_log_R, age_at_bottom, age_difference, max_log_R_value, r_value
    except FileNotFoundError:
        print(f"  错误: 文件 {file_path} 未找到。")
        return None, None, None, None, None
    except pd.errors.EmptyDataError:
        print(f"  错误: 文件 {os.path.basename(file_path)} 为空或格式不正确。")
        return None, None, None, None, None
    except Exception as e:
        print(f"  处理文件 {os.path.basename(file_path)} 时发生错误: {e}")
        return None, None, None, None, None

def main():
    """
    主函数，用于获取目录并遍历处理CSV文件。
    """
    directory = input("请输入包含CSV文件的目录路径 (例如 D:\\data\\csv_files, 默认为当前脚本运行目录): ")
    if not directory:
        directory = '.'  # 如果用户未输入，则默认为当前目录

    if not os.path.isdir(directory):
        print(f"错误: 目录 '{directory}' 不存在或不是一个有效的目录。")
        return

    print(f"\n正在扫描目录: {os.path.abspath(directory)}\n")

    found_csv_files = False
    for filename in os.listdir(directory):
        if filename.lower().endswith(".csv"):
            found_csv_files = True
            file_path = os.path.join(directory, filename)
            print(f"处理文件: {filename}")
            
            age_max_log_R, age_bottom, diff, max_log_R, r_val = process_csv_file(file_path)
            
            if diff is not None:
                print(f"  log_R 最大值: {max_log_R}")
                print(f"  计算得到的 R 值 (10^log_R): {r_val}")
                print(f"  log_R 最大值所在行的 star_age: {age_max_log_R}")
                print(f"  文件末尾行的 star_age: {age_bottom}")
                print(f"  二者 star_age 差值: {diff}\n")
            else:
                # process_csv_file 函数内部已打印具体错误信息
                print(f"  未能成功处理文件 {filename}。\n")
    
    if not found_csv_files:
        print(f"在目录 '{os.path.abspath(directory)}' 中未找到任何CSV文件。")

if __name__ == "__main__":
    main()