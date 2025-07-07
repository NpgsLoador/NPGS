# --- START OF FILE CompileShaders.py ---

import subprocess
import time
from pathlib import Path
from datetime import datetime
import json
import hashlib

# ... (所有常量定义和辅助函数都与上一版完全相同，这里省略以保持简洁) ...
# --- 常量定义 ---
SOURCE_DIR = Path(__file__).parent.parent.parent / 'Sources' / 'Engine' / 'Shaders'
TARGET_DIR = SOURCE_DIR.parent.parent.parent / 'Assets' / 'Shaders'
GLSLC_PATH = 'glslc.exe'
META_FILE = Path(__file__).parent / 'ShaderMeta.json'

SHADER_TYPES = {
    '.comp': 'compute',
    '.frag': 'fragment',
    '.geom': 'geometry',
    '.mesh': 'mesh',
    '.rahit': 'ray any hit',
    '.rcall': 'ray callable',
    '.rchit': 'ray closest hit',
    '.rgen': 'ray generation',
    '.rint': 'ray intersection',
    '.rmiss': 'ray miss',
    '.task': 'task',
    '.tesc': 'tessellation control',
    '.tese': 'tessellation evaluation',
    '.vert': 'vertex',
    '.glsl': 'glsl', 
}

# ... (ShaderVariant, parse_includes, get_shader_hash, 等所有函数都保持不变) ...
class ShaderVariant:
    def __init__(self, input_path: str, output_name: str, macros: list[str]):
        self.input_path = input_path
        self.output_name = output_name
        self.macros = sorted(macros)

def parse_includes(shader_file: Path, included_files: set = None) -> set:
    if included_files is None: included_files = set()
    if shader_file in included_files: return included_files
    included_files.add(shader_file)
    shader_dir = shader_file.parent
    try:
        with open(shader_file, 'r', encoding='utf-8') as f:
            for line in f:
                if line.strip().startswith('#include'):
                    try:
                        include_path_str = line.split('"')[1]
                        include_path = (shader_dir / include_path_str).resolve()
                        if include_path.exists():
                            parse_includes(include_path, included_files)
                        else:
                             # 静默处理找不到的include，因为这在大型项目中可能常见
                             pass
                    except IndexError: pass
    except Exception as e: print(f"警告: 解析包含文件时出错 {shader_file.name}: {e}")
    return included_files

def get_shader_hash(source_file: Path, macros: list[str] = None) -> str:
    hasher = hashlib.md5()
    try:
        all_files = parse_includes(source_file)
    except Exception as e:
        print(f"错误: 无法解析 {source_file} 的依赖: {e}")
        return ""
    for file_path in sorted(all_files, key=lambda p: str(p)):
        try:
            with open(file_path, 'rb') as f: hasher.update(f.read())
        except IOError: continue
    if macros: hasher.update(''.join(sorted(macros)).encode('utf-8'))
    return hasher.hexdigest()

def check_needs_int64_support(source_file: Path) -> bool:
    try:
        files_to_check = parse_includes(source_file)
        for file in files_to_check:
            with open(file, 'r', encoding='utf-8') as f:
                if "#extension GL_EXT_shader_explicit_arithmetic_types_int64" in f.read():
                    return True
        return False
    except Exception as e:
        print(f"警告: 检查int64支持时出错 {source_file.name}: {str(e)}")
        return True

def load_meta_data() -> dict:
    if META_FILE.exists():
        try:
            with open(META_FILE, 'r', encoding='utf-8') as f: return json.load(f)
        except (json.JSONDecodeError, IOError) as e:
            print(f"警告: 无法加载或解析元数据文件 {META_FILE}: {e}")
    return {}

def save_meta_data(meta_data: dict):
    try:
        with open(META_FILE, 'w', encoding='utf-8') as f: json.dump(meta_data, f, indent=2)
    except IOError as e: print(f"警告: 无法保存元数据文件 {META_FILE}: {e}")

def needs_recompile(source_file: Path, spv_file: Path, macros: list[str] = None) -> bool:
    if not spv_file.exists(): return True
    current_hash = get_shader_hash(source_file, macros)
    if not current_hash: return True 
    meta_data = load_meta_data()
    rel_path_key = str(spv_file.relative_to(TARGET_DIR)).replace('\\', '/')
    saved_hash = meta_data.get(rel_path_key)
    return current_hash != saved_hash

def compile_shader(source_file: Path, target_file: Path, macros: list[str] = None) -> bool:
    target_file.parent.mkdir(parents=True, exist_ok=True)
    try:
        cmd = [GLSLC_PATH, '--target-env=vulkan1.3', '-O']
        if check_needs_int64_support(source_file):
            cmd.append('-DSPV_KHR_shader_explicit_arithmetic_types_int64=1')
        if macros:
            for macro in macros: cmd.append(f'-D{macro}')
        cmd.extend([str(source_file), '-o', str(target_file)])
        result = subprocess.run(cmd, capture_output=True, text=True, encoding='utf-8')
        if result.returncode != 0:
            # 只在失败时打印错误
            macros_str = f" with macros {macros}" if macros else ""
            print(f"错误: 编译 {source_file.relative_to(SOURCE_DIR)}{macros_str} 失败:\n{result.stderr}")
            return False
        current_hash = get_shader_hash(source_file, macros)
        meta_data = load_meta_data()
        rel_path_key = str(target_file.relative_to(TARGET_DIR)).replace('\\', '/')
        meta_data[rel_path_key] = current_hash
        save_meta_data(meta_data)
        return True
    except Exception as e:
        print(f"严重错误: 在编译 {source_file.name} 时发生异常: {e}")
        return False

def load_shader_config(config_path: Path) -> list[ShaderVariant]:
    variants = []
    if not config_path.exists(): return variants
    try:
        with open(config_path, 'r', encoding='utf-8') as f:
            for line in f:
                line = line.strip()
                if not line or line.startswith('//'): continue
                parts = [p for p in line.split() if p]
                if len(parts) >= 2:
                    variants.append(ShaderVariant(parts[0], parts[1], parts[2:]))
    except Exception as e: print(f"错误: 无法读取配置文件 {config_path}: {e}")
    return variants

def is_compilable_shader(shader_file: Path) -> bool:
    try:
        with open(shader_file, 'r', encoding='utf-8') as f:
            return 'void main' in f.read()
    except Exception as e:
        print(f"警告: 检查 {shader_file.name} 内容时出错: {e}")
        return True

# --- *** main 函数最终简化输出版 *** ---
def main() -> tuple:
    """主函数"""
    print(f"生成开始于 {datetime.now().strftime('%H:%M:%S')}...")
    print("------ 正在编译着色器 ------")
    
    compiled_count, failed_count, skipped_count = 0, 0, 0
    
    compile_tasks = {}

    source_files_to_scan = set()
    for ext in SHADER_TYPES:
        source_files_to_scan.update(SOURCE_DIR.rglob(f'*{ext}'))

    for source_file in source_files_to_scan:
        if not is_compilable_shader(source_file):
            continue

        rel_path = source_file.relative_to(SOURCE_DIR)
        default_target_name = source_file.name.replace(source_file.suffix, f"{source_file.suffix}.spv", 1)
        target_file = TARGET_DIR / rel_path.with_name(default_target_name)
        
        task_key = (source_file, frozenset())
        compile_tasks[task_key] = target_file

    config_file = Path(__file__).parent / 'CompileShaders.cfg'
    variants = load_shader_config(config_file)
    for variant in variants:
        source_file = SOURCE_DIR / variant.input_path
        if not source_file.exists():
            print(f"错误: 配置文件中的源文件不存在 {source_file}")
            failed_count += 1
            continue
        
        target_file = TARGET_DIR / variant.output_name
        task_key = (source_file, frozenset(variant.macros))
        compile_tasks[task_key] = target_file

    # *** 这里是主要的改动 ***
    sorted_tasks = sorted(compile_tasks.items(), key=lambda item: (str(item[0][0]), sorted(list(item[0][1]))))

    for (source_file, macros_fset), target_file in sorted_tasks:
        if needs_recompile(source_file, target_file, list(macros_fset)):
            macros = list(macros_fset)
            # 只有需要编译时才打印信息
            macros_str = f" with macros {macros}" if macros else ""
            print(f"编译: {source_file.relative_to(SOURCE_DIR)}{macros_str} -> {target_file.relative_to(TARGET_DIR)}")
            
            if compile_shader(source_file, target_file, macros):
                compiled_count += 1
            else:
                failed_count += 1
        else:
            skipped_count += 1
            
    return compiled_count, failed_count, skipped_count

if __name__ == "__main__":
    start_time = time.time()
    compiled, failed, skipped = main()
    elapsed_time = time.time() - start_time

    print(f"========== 生成: {compiled} 成功，{failed} 失败，{skipped} 最新，{skipped} 已跳过 ==========")
    print(f"========== 生成 于 {datetime.now().strftime('%H:%M')} 完成，耗时 {elapsed_time:.3f} 秒 ==========")