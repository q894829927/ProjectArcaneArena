"""保留在 Content/Python 根目录的拾取资产一键入口。"""

import importlib

import unreal

from pickup_items import setup_pickup_items


def main():
    """重新加载实现模块并执行全部拾取资产配置。"""
    pickup_setup = importlib.reload(setup_pickup_items)
    pickup_setup.main()


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        unreal.log_error(f"Pickup item setup entry failed: {error}")
        raise
