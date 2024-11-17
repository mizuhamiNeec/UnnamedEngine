import os
import xml.etree.ElementTree as ET
import argparse

# 除外するディレクトリ
EXCLUDED_DIRS = {
    '.git',
    '.vs',
    '.idea', 
    '.vscode',
    'node_modules',
    'Build',
    'GE3_GameEngine',
    'Resources\\Fonts',  # 除外するフォルダ
    'Resources\\Models',
    'Resources\\Sounds',
    'Resources\\Textures'
}

# 除外するファイル
EXCLUDED_FILES = {
    '.gitignore', 
    '.editorconfig',
    'LICENSE.txt'
}

# 引数の設定
def parse_args():
    parser = argparse.ArgumentParser(description='Update Visual Studio project filters to match the folder structure.')
    parser.add_argument('project_dir', help='Path to the project directory')
    parser.add_argument('filters_file', help='Path to the .vcxproj.filters file')
    return parser.parse_args()

# GUIDを生成する関数
def generate_guid():
    import uuid
    return str(uuid.uuid4()).upper()

# フィルターファイルを更新する関数
def update_filters(project_dir, filters_file):
    # フィルターファイルのXMLテンプレートを準備
    root = ET.Element('Project', attrib={'ToolsVersion': '4.0', 'xmlns': 'http://schemas.microsoft.com/developer/msbuild/2003'})
    item_group_filters = ET.SubElement(root, 'ItemGroup')
    item_group_cl_include = ET.SubElement(root, 'ItemGroup')
    item_group_cl_compile = ET.SubElement(root, 'ItemGroup')

    filters_set = set()  # フィルターの重複を防ぐためのセット

    # ディレクトリを再帰的に走査し、フィルタを作成
    for foldername, subfolders, filenames in os.walk(project_dir):
        # 除外するフォルダを削除
        subfolders[:] = [d for d in subfolders if d not in EXCLUDED_DIRS]

        # プロジェクトフォルダの相対パスをフィルタに追加
        relative_folder = os.path.relpath(foldername, project_dir)

        # 除外されないフォルダの場合、フィルタを追加
        if relative_folder != "." and relative_folder not in EXCLUDED_DIRS:
            # 階層ごとにフィルタを作成する
            parts = relative_folder.split(os.sep)
            for i in range(1, len(parts) + 1):
                partial_filter = os.sep.join(parts[:i])
                if partial_filter not in filters_set:
                    filter_element = ET.SubElement(item_group_filters, 'Filter', attrib={'Include': partial_filter})
                    ET.SubElement(filter_element, 'UniqueIdentifier').text = '{' + generate_guid() + '}'
                    filters_set.add(partial_filter)

        # フォルダ内のファイルをフィルタに追加
        for filename in filenames:
            if filename in EXCLUDED_FILES:
                continue  # 除外するファイルは無視

            print(filename)

            file_relative_path = os.path.relpath(os.path.join(foldername, filename), project_dir)
            if filename.endswith('.h'):  # ヘッダーファイル
                cl_include = ET.SubElement(item_group_cl_include, 'ClInclude', attrib={'Include': file_relative_path})
                ET.SubElement(cl_include, 'Filter').text = relative_folder.replace("/", "\\")
            elif filename.endswith('.cpp') or filename.endswith('.c'):  # ソースファイル
                cl_compile = ET.SubElement(item_group_cl_compile, 'ClCompile', attrib={'Include': file_relative_path})
                ET.SubElement(cl_compile, 'Filter').text = relative_folder.replace("/", "\\")
            elif filename.endswith('.hlsl') or filename.endswith('.fx'):  # シェーダーファイル
                cl_compile = ET.SubElement(item_group_cl_compile, 'CustomBuild', attrib={'Include': file_relative_path})
                # シェーダーファイルをResources/Shadersフィルタに追加
                ET.SubElement(cl_compile, 'Filter').text = "Resources\\Shaders"  # バックスラッシュに修正

    # XMLツリーを作成してフィルターファイルに書き込む
    tree = ET.ElementTree(root)
    tree.write(filters_file, encoding='utf-8', xml_declaration=True)
    print(f"Filters file '{filters_file}' has been updated.")

# メイン処理
if __name__ == "__main__":
    args = parse_args()  # 引数を取得
    update_filters(args.project_dir, args.filters_file)  # フィルターファイルを更新
