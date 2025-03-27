import os
import sys
import ctypes
import winreg
from ctypes import wintypes
from pathlib import Path

def is_admin():
    """检查程序是否以管理员权限运行"""
    try:
        return ctypes.windll.shell32.IsUserAnAdmin()
    except:
        return False

def get_proper_case_path(path_str):
    """获取路径的正确大小写形式，并确保不以斜线结尾"""
    # 移除路径末尾的斜线
    path_str = path_str.rstrip('\\/')
    
    if not path_str:
        return path_str
        
    # 检查路径是否存在
    if not os.path.exists(path_str):
        return path_str
    
    try:
        # 尝试获取真实路径以获得正确的大小写
        real_path = str(Path(path_str).resolve())
        return real_path
    except Exception as e:
        print(f"警告: 处理路径 '{path_str}' 时出错: {e}")
        return path_str

def get_environment_paths(env_type="user"):
    """获取环境变量 Path 的值"""
    if env_type == "user":
        key = winreg.HKEY_CURRENT_USER
        subkey = "Environment"
    else:
        key = winreg.HKEY_LOCAL_MACHINE
        subkey = r"SYSTEM\CurrentControlSet\Control\Session Manager\Environment"
    
    try:
        with winreg.OpenKey(key, subkey) as reg_key:
            path_value, path_type = winreg.QueryValueEx(reg_key, "Path")
            return path_value, path_type
    except Exception as e:
        print(f"获取{env_type}环境变量 Path 时出错: {e}")
        return None, None

def set_environment_paths(paths, env_type="user"):
    """设置环境变量 Path 的值"""
    if env_type == "user":
        key = winreg.HKEY_CURRENT_USER
        subkey = "Environment"
    else:
        key = winreg.HKEY_LOCAL_MACHINE
        subkey = r"SYSTEM\CurrentControlSet\Control\Session Manager\Environment"
    
    try:
        with winreg.OpenKey(key, subkey, 0, winreg.KEY_WRITE) as reg_key:
            winreg.SetValueEx(reg_key, "Path", 0, winreg.REG_EXPAND_SZ, paths)
        return True
    except Exception as e:
        print(f"设置{env_type}环境变量 Path 时出错: {e}")
        return False

def fix_path_case(env_type="user"):
    """修复环境变量 Path 中的大小写问题"""
    path_value, path_type = get_environment_paths(env_type)
    if not path_value:
        return False
    
    paths = path_value.split(";")
    fixed_paths = []
    changes = 0
    
    print(f"\n开始修复 {env_type} 环境变量 Path...")
    
    for i, path in enumerate(paths):
        if not path.strip():
            continue
            
        fixed_path = get_proper_case_path(path)
        
        if fixed_path != path:
            changes += 1
            print(f"[{i+1}] 修改: {path} -> {fixed_path}")
        else:
            print(f"[{i+1}] 保持: {path}")
            
        fixed_paths.append(fixed_path)
    
    if changes > 0:
        new_path_value = ";".join(fixed_paths)
        if set_environment_paths(new_path_value, env_type):
            print(f"\n成功修复 {env_type} 环境变量 Path，共修改了 {changes} 个路径。")
            
            # 通知系统环境变量已更新
            HWND_BROADCAST = 0xFFFF
            WM_SETTINGCHANGE = 0x001A
            SMTO_ABORTIFHUNG = 0x0002
            result = ctypes.windll.user32.SendMessageTimeoutW(
                HWND_BROADCAST, WM_SETTINGCHANGE, 0, 
                "Environment", SMTO_ABORTIFHUNG, 5000, 
                ctypes.byref(wintypes.DWORD())
            )
            if result == 0:
                print("警告: 通知系统环境变量已更新失败。")
            
            return True
        else:
            print(f"\n保存 {env_type} 环境变量 Path 失败。")
            return False
    else:
        print(f"\n{env_type} 环境变量 Path 无需修改。")
        return True

def main():
    """主函数"""
    print("Windows 环境变量 Path 大小写修复工具")
    print("=" * 40)
    
    # 检查管理员权限
    if not is_admin():
        print("注意: 没有管理员权限，只能修复用户级环境变量。")
        print("      若要修复系统级环境变量，请以管理员身份运行。")
        fix_path_case("user")
    else:
        print("检测到管理员权限，可以修复用户级和系统级环境变量。")
        
        choice = input("请选择要修复的环境变量类型 [1=用户级, 2=系统级, 3=全部]: ")
        
        if choice == "1":
            fix_path_case("user")
        elif choice == "2":
            fix_path_case("system")
        elif choice == "3":
            fix_path_case("user")
            fix_path_case("system")
        else:
            print("无效的选择，退出程序。")
    
    print("\n处理完成。请重新启动应用程序以使更改生效。")
    input("按回车键退出...")

if __name__ == "__main__":
    # 如果没有管理员权限，尝试获取管理员权限重新运行
    if not is_admin() and "-noelevate" not in sys.argv:
        try:
            ctypes.windll.shell32.ShellExecuteW(
                None, "runas", sys.executable, " ".join(sys.argv) + " -noelevate", None, 1
            )
        except:
            # 如果获取管理员权限失败，则以当前权限运行
            main()
    else:
        main()