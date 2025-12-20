import os

def concat_sources(root_dir, output_file):
    exts = {'.cpp', '.hpp', '.inl', '.glsl'}
    with open(output_file, 'w', encoding='utf-8') as out:
        for dirpath, _, filenames in os.walk(root_dir):
            for fname in filenames:
                if os.path.splitext(fname)[1] in exts:
                    fpath = os.path.join(dirpath, fname)
                    out.write(f'// file {fpath}\n')
                    with open(fpath, 'r', encoding='utf-8', errors='ignore') as src:
                        out.write(src.read())
                    out.write(f'\n// end file {fpath}\n\n')

if __name__ == "__main__":
    # 修改为你的目录路径和输出文件名
    concat_sources(r"Z:\\Source\\Repos\\NPGS\\NPGS\\Sources", "all_sources.txt")