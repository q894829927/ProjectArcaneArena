"""保留根目录命令的构筑资产生成入口。"""

import importlib

import unreal

from build_assets import setup_build_assets


def main():
    """重新加载实现模块并执行 Fire/Lightning 构筑资产总流程。"""
    build_setup = importlib.reload(setup_build_assets)
    build_setup.main()


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        unreal.log_error(f"Build asset setup entry failed: {error}")
        raise
