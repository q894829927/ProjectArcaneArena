"""为 Project Arcane Arena 提供可重复执行的 Unreal 编辑器资产工具。"""

import unreal


def make_tag(tag_name):
    """从已注册名称构造 GameplayTag，避免写入只读 TagName 属性。"""
    if not tag_name:
        raise ValueError("GameplayTag name cannot be empty.")

    tag = unreal.GameplayTag()
    serialized_value = f'(TagName="{tag_name}")'
    if not tag.import_text(serialized_value):
        raise RuntimeError(
            f"Failed to import GameplayTag '{tag_name}' from '{serialized_value}'."
        )
    if not unreal.GameplayTagLibrary.is_gameplay_tag_valid(tag):
        raise RuntimeError(f"GameplayTag is invalid or not registered: {tag_name}")
    return tag


def make_optional_tag(tag_name):
    """把可选标签名转换为 GameplayTag，空值返回无效标签。"""
    return make_tag(tag_name) if tag_name else unreal.GameplayTag()


def make_tag_container(tag_names):
    """从标签名序列构造只包含精确标签的 GameplayTagContainer。"""
    tags = [make_tag(tag_name) for tag_name in tag_names]
    return unreal.GameplayTagContainer(gameplay_tags=tags)


def require_unreal_type(type_name):
    """读取 Unreal Python 反射类型，缺失时提示编译并重启编辑器。"""
    unreal_type = getattr(unreal, type_name, None)
    if unreal_type is None:
        raise RuntimeError(
            f"Required Unreal Python type is unavailable: {type_name}. "
            "Compile the project and restart Unreal Editor."
        )
    return unreal_type


def require_asset(asset_path, expected_class=None):
    """加载必需资产，并在缺失或类型不匹配时立即终止。"""
    asset = unreal.EditorAssetLibrary.load_asset(asset_path)
    if asset is None:
        raise RuntimeError(f"Required asset was not found: {asset_path}")
    if expected_class is not None and not isinstance(asset, expected_class):
        raise RuntimeError(
            f"Asset {asset_path} is {asset.get_class().get_name()}, "
            f"expected {expected_class.__name__}."
        )
    return asset


def canonical_asset_path(asset_or_path):
    """统一为不带对象名的包路径，便于稳定比较和去重。"""
    raw_path = (
        asset_or_path.get_path_name()
        if hasattr(asset_or_path, "get_path_name")
        else str(asset_or_path)
    )
    object_separator_index = raw_path.find(".", raw_path.rfind("/"))
    return (
        raw_path[:object_separator_index]
        if object_separator_index >= 0
        else raw_path
    )


def save_asset(asset_path):
    """保存指定资产，保存失败时抛出明确错误。"""
    if not unreal.EditorAssetLibrary.save_asset(asset_path, only_if_is_dirty=False):
        raise RuntimeError(f"Failed to save asset: {asset_path}")


def set_localized_text_property(asset, native_property_name, source_string):
    """像详情面板一样写入 FText，并生成稳定的本地化文本身份。"""
    if not unreal.TextLibrary.edit_text_property_source_string(
        asset,
        unreal.Name(native_property_name),
        source_string,
        True,
    ):
        raise RuntimeError(
            f"Failed to set localized text property "
            f"{asset.get_path_name()}.{native_property_name}"
        )


def load_blueprint_class(asset_path):
    """加载 Blueprint GeneratedClass，缺失时返回明确错误。"""
    generated_class = unreal.EditorAssetLibrary.load_blueprint_class(asset_path)
    if generated_class is None:
        raise RuntimeError(f"Failed to load generated class: {asset_path}")
    return generated_class


def require_blueprint(asset_path, parent_class=None):
    """验证 Blueprint 资产，并可选验证其 GeneratedClass 父类。"""
    blueprint = require_asset(asset_path, unreal.Blueprint)
    generated_class = load_blueprint_class(asset_path)
    if parent_class is not None and not unreal.MathLibrary.class_is_child_of(
        generated_class,
        parent_class,
    ):
        parent_name = getattr(parent_class, "__name__", str(parent_class))
        raise RuntimeError(
            f"Blueprint {asset_path} is not a child of {parent_name}."
        )
    return blueprint, generated_class


def create_or_load_blueprint(asset_name, destination_path, parent_class):
    """幂等创建指定原生父类的 Blueprint，并拒绝复用错误父类资产。"""
    asset_path = f"{destination_path}/{asset_name}"
    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        blueprint, generated_class = require_blueprint(asset_path, parent_class)
        return blueprint, generated_class, asset_path

    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", parent_class)
    blueprint = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        asset_name,
        destination_path,
        unreal.Blueprint,
        factory,
    )
    if blueprint is None:
        raise RuntimeError(f"Failed to create Blueprint: {asset_path}")

    _, generated_class = require_blueprint(asset_path, parent_class)
    return blueprint, generated_class, asset_path


def duplicate_or_load_blueprint(asset_name, destination_path, template_path, parent_class=None):
    """幂等复制 Blueprint 模板，并验证复用资产的类型和可选父类。"""
    asset_path = f"{destination_path}/{asset_name}"
    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        blueprint, generated_class = require_blueprint(asset_path, parent_class)
        return blueprint, generated_class, asset_path

    require_blueprint(template_path, parent_class)
    blueprint = unreal.EditorAssetLibrary.duplicate_asset(template_path, asset_path)
    if blueprint is None:
        raise RuntimeError(f"Failed to duplicate Blueprint: {asset_path}")

    _, generated_class = require_blueprint(asset_path, parent_class)
    return blueprint, generated_class, asset_path


def duplicate_or_load_asset(asset_path, template_path, expected_class):
    """幂等复制普通资产，已存在时只验证类型而不覆盖用户后续修改。"""
    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        return require_asset(asset_path, expected_class)

    require_asset(template_path, expected_class)
    asset = unreal.EditorAssetLibrary.duplicate_asset(template_path, asset_path)
    if asset is None:
        raise RuntimeError(f"Failed to duplicate asset: {template_path} -> {asset_path}")
    return require_asset(asset_path, expected_class)


def resolve_enum_value(enum_type, value_name):
    """按配置字符串解析 UE Python 枚举值。"""
    if not value_name or not hasattr(enum_type, value_name):
        raise RuntimeError(
            f"Enum value '{value_name}' does not exist on {enum_type}."
        )
    return getattr(enum_type, value_name)


def resolve_optional_asset(asset_path, expected_class):
    """解析可选资产路径；None 表示清空引用。"""
    return require_asset(asset_path, expected_class) if asset_path else None


def resolve_optional_blueprint_class(asset_path, parent_class):
    """解析可选 Blueprint Class 路径；None 表示清空 Class 引用。"""
    if not asset_path:
        return None
    _, generated_class = require_blueprint(asset_path, parent_class)
    return generated_class
