# -*- coding: utf-8 -*-
"""
EnemyAI 插件 Content 目录重组脚本（在 UE5 编辑器内执行）

执行方式：
  Output Log（窗口 -> 开发者工具 -> 输出日志）输入：
  py "D:/Unreal Projects/LyraFork/Plugins/GameFeatures/EnemyAI/reorganize_content.py"

执行前必做：
  1) 关闭编辑器中所有已打开的资产标签页（尤其是 Lvl_ThirdPerson 关卡），
     否则引擎会拒绝移动/删除该资产。
  2) 建议先 git commit 当前状态，便于回滚。
  3) 默认 DRY_RUN=True（只打印计划，不改动任何文件）。
     确认日志无误后，把下面的 DRY_RUN 改为 False，再执行一次。

重要说明：
  - 本插件 Content 的 UE 挂载点是 /EnemyAI（不是 /Game）。
  - 根目录 EnemyAI.uasset 保持不动。
  - __ExternalActors__ / __ExternalObjects__ 由引擎自动管理，脚本绝不触碰；
    主关卡 Lvl_ThirdPerson -> L_ThirdPerson 改名时，引擎会自动迁移其外部 Actor 目录。
  - 所有移动均通过 EditorAssetLibrary 完成，会自动产生 Redirector；
    脚本末尾统一删除 Redirector，即等价于执行 Fix Up Redirectors。
  - 目标结构中当前不存在的资产（整套 AI 行为树、动画、多数音频、Enemies/Player 蓝图等）一律忽略。
  - 除主关卡按要求改名外，其余资产只移动、不改名。
"""

import unreal

# ==================== 执行开关 ====================
DRY_RUN = True
# =================================================

EAL  = unreal.EditorAssetLibrary
REG  = unreal.AssetRegistryHelpers.get_asset_registry()
ROOT = "/EnemyAI"

STAT = {"moved": 0, "move_fail": 0, "dir_moved": 0, "dir_fail": 0,
        "deleted": 0, "del_fail": 0, "redir": 0, "redir_fail": 0, "skip": 0}


def info(msg):
    unreal.log("[Reorg] " + str(msg))


def warn(msg):
    unreal.log_warning("[Reorg] " + str(msg))


def fail(msg):
    unreal.log_error("[Reorg] " + str(msg))


def move_asset(src, dst):
    """移动单个资产；引擎自动生成 Redirector。"""
    if not EAL.does_asset_exist(src):
        warn("源资产不存在，跳过: %s" % src)
        STAT["skip"] += 1
        return
    if EAL.does_asset_exist(dst):
        warn("目标资产已存在，跳过: %s" % dst)
        STAT["skip"] += 1
        return
    info("MOVE   %s  ->  %s" % (src, dst))
    if DRY_RUN:
        return
    if EAL.rename_asset(src, dst):
        STAT["moved"] += 1
    else:
        fail("移动失败（是否资产仍被打开？）: %s" % src)
        STAT["move_fail"] += 1


def move_dir(src, dst):
    """移动整个目录（含其下所有资产）；引擎自动生成 Redirector。"""
    if not EAL.does_directory_exist(src):
        warn("源目录不存在，跳过: %s" % src)
        STAT["skip"] += 1
        return
    info("MOVEDIR  %s  ->  %s" % (src, dst))
    if DRY_RUN:
        return
    if EAL.rename_directory(src, dst):
        STAT["dir_moved"] += 1
    else:
        fail("目录移动失败: %s" % src)
        STAT["dir_fail"] += 1


def delete_asset(path, reason):
    """删除资产（去重）；引用它的包会留下 Redirector，末尾统一修复。"""
    if not EAL.does_asset_exist(path):
        warn("待删资产不存在，跳过: %s" % path)
        STAT["skip"] += 1
        return
    info("DELETE  %s   (%s)" % (path, reason))
    if DRY_RUN:
        return
    if EAL.delete_asset(path):
        STAT["deleted"] += 1
    else:
        fail("删除失败（是否资产仍被打开？）: %s" % path)
        STAT["del_fail"] += 1


def ensure_dir(dirpath):
    """确保目标资产目录存在（DRY_RUN 时只打印）。"""
    if EAL.does_directory_exist(dirpath):
        return
    info("MKDIR  %s" % dirpath)
    if not DRY_RUN:
        if not EAL.make_directory(dirpath):
            fail("创建目录失败: %s" % dirpath)


def is_redirector(asset_data):
    try:
        return str(asset_data.asset_class) == "ObjectRedirector"
    except Exception:
        try:
            return str(asset_data.asset_class_path.asset_name) == "ObjectRedirector"
        except Exception:
            return False


def fixup_redirectors():
    """等价于内容浏览器的 Fix Up Redirectors：删除所有重定向器并修复引用。"""
    assets = REG.get_assets_by_path(ROOT, recursive=True)
    redirs = [a for a in assets if is_redirector(a)]
    info("发现 Redirector 数量: %d" % len(redirs))
    for a in redirs:
        pkg = str(a.package_name)
        info("FIXUP  %s" % pkg)
        if DRY_RUN:
            continue
        if EAL.delete_asset(pkg):
            STAT["redir"] += 1
        else:
            fail("Redirector 删除失败: %s" % pkg)
            STAT["redir_fail"] += 1


# ============================================================
info("=" * 72)
info("目录重组脚本启动   DRY_RUN=%s" % DRY_RUN)
info("=" * 72)

# ---- [0/7] 预先创建所有目标父目录 ----
info("--- [0/7] 确保目标目录存在 ---")
# 注意：整目录移动（rename_directory）的目标目录不能预先创建，
#       否则会因目标已存在子目录而失败；这些目录在对应步骤中按需创建。
for d in (
    ROOT + "/ThirdParty",
    ROOT + "/Prototyping",
    ROOT + "/Game/Audio/Mix",
    ROOT + "/Game/Audio/Waves/Magic",
    ROOT + "/Game/Core",
    ROOT + "/Game/Input/Actions",
    ROOT + "/Game/Input/Contexts",
    ROOT + "/Game/Input/Touch",
    ROOT + "/Game/Weapons/Axe/Meshes",
    ROOT + "/Game/Weapons/Sword/Blueprints",
    ROOT + "/Game/Maps",
):
    ensure_dir(d)

# ---- [1/7] 第三方素材 -> ThirdParty ----
info("--- [1/7] 第三方素材 -> ThirdParty ---")
for name in ("FXVarietyPack", "InfinityBladeEffects",
             "ParagonFengMao", "ParagonGideon", "ParagonSparrow"):
    move_dir(ROOT + "/" + name, ROOT + "/ThirdParty/" + name)

# ---- [2/7] 白盒原型 -> Prototyping ----
info("--- [2/7] 白盒原型 -> Prototyping ---")
move_dir(ROOT + "/LevelPrototyping", ROOT + "/Prototyping/LevelPrototyping")
move_dir(ROOT + "/StarterContent",   ROOT + "/Prototyping/StarterContent")
# LevelPrototyping 就位后再建其 Materials 子目录，供后面材质实例归位使用
ensure_dir(ROOT + "/Prototyping/LevelPrototyping/Materials")

# ---- [3/7] Mannequins -> Game/Characters/Mannequins ----
info("--- [3/7] Mannequins -> Game/Characters/Mannequins ---")
move_dir(ROOT + "/Characters/Mannequins", ROOT + "/Game/Characters/Mannequins")
# 材质实例归入 Instances/（符合官方标准结构；资产名保持不变）
# 预演时源目录仍在原位；正式执行时源已随 Mannequins 移到新位，故动态取源。
MQ_SRC = ROOT + "/Characters/Mannequins/Materials"
MQ     = ROOT + "/Game/Characters/Mannequins/Materials"
MQ_BASE = MQ if EAL.does_directory_exist(MQ + "/Manny") else MQ_SRC
if EAL.does_directory_exist(MQ_BASE + "/Manny"):
    ensure_dir(MQ + "/Instances")
    move_dir(MQ_BASE + "/Manny", MQ + "/Instances/Manny")
    move_dir(MQ_BASE + "/Quinn", MQ + "/Instances/Quinn")

# ---- [4/7] 音频 -> Game/Audio ----
info("--- [4/7] 音频 -> Game/Audio ---")
move_asset(ROOT + "/Audio/DYN_LowMultibandDynamics",
           ROOT + "/Game/Audio/Mix/DYN_LowMultibandDynamics")
move_asset(ROOT + "/Audio/sci-fi-explosion",
           ROOT + "/Game/Audio/Waves/Magic/sci-fi-explosion")

# ---- [5/7] 核心框架 / 输入 / 武器 -> Game/ ----
info("--- [5/7] Core / Input / Weapons -> Game ---")
move_asset(ROOT + "/ThirdPerson/Blueprints/BP_ThirdPersonGameMode",
           ROOT + "/Game/Core/BP_ThirdPersonGameMode")

move_asset(ROOT + "/Input/Actions/IA_Look",
           ROOT + "/Game/Input/Actions/IA_Look")
move_asset(ROOT + "/Input/Actions/IA_MouseLook",
           ROOT + "/Game/Input/Actions/IA_MouseLook")
move_asset(ROOT + "/Input/IMC_Default",
           ROOT + "/Game/Input/Contexts/IMC_Default")
move_asset(ROOT + "/Input/IMC_MouseLook",
           ROOT + "/Game/Input/Contexts/IMC_MouseLook")
move_asset(ROOT + "/Input/Touch/BPI_TouchInterface",
           ROOT + "/Game/Input/Touch/BPI_TouchInterface")
move_asset(ROOT + "/Input/Touch/UI_TouchSimple",
           ROOT + "/Game/Input/Touch/UI_TouchSimple")
move_asset(ROOT + "/Input/Touch/UI_Thumbstick",
           ROOT + "/Game/Input/Touch/UI_Thumbstick")

move_asset(ROOT + "/Weapons/Meshes/Axe/SM_Axe",
           ROOT + "/Game/Weapons/Axe/Meshes/SM_Axe")
move_asset(ROOT + "/Weapons/Meshes/Axe/BP_Sword",
           ROOT + "/Game/Weapons/Sword/Blueprints/BP_Sword")

# ---- [6/7] 地图：主关卡改名归位 + 材质实例移出 + 去重 ----
info("--- [6/7] Maps -> Game/Maps（主关卡改名，ExternalActors 由引擎自动迁移） ---")
move_asset(ROOT + "/Maps/Lvl_ThirdPerson",
           ROOT + "/Game/Maps/L_ThirdPerson")
move_asset(ROOT + "/Maps/MI_ThirdPersonColWay",
           ROOT + "/Prototyping/LevelPrototyping/Materials/MI_ThirdPersonColWay")
# 重复副本（ThirdPerson 目录下，无 ExternalActors），删除去重
delete_asset(ROOT + "/ThirdPerson/Lvl_ThirdPerson", "重复关卡，去重")
delete_asset(ROOT + "/ThirdPerson/MI_ThirdPersonColWay", "重复材质实例，去重")

# ---- [7/7] Fix Up Redirectors ----
info("--- [7/7] Fix Up Redirectors ---")
fixup_redirectors()

# ---- 保存 ----
if not DRY_RUN:
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    info("已保存所有脏包。")

# ---- 汇总 ----
info("=" * 72)
info("汇总(DRY_RUN=%s): 资产移动=%d(失败%d) 目录移动=%d(失败%d) "
     "删除=%d(失败%d) Redirector修复=%d(失败%d) 跳过=%d"
     % (DRY_RUN, STAT["moved"], STAT["move_fail"], STAT["dir_moved"],
        STAT["dir_fail"], STAT["deleted"], STAT["del_fail"],
        STAT["redir"], STAT["redir_fail"], STAT["skip"]))
if DRY_RUN:
    info("当前为预演模式，未改动任何文件。确认无误后请将 DRY_RUN 改为 False 再执行。")
info("=" * 72)
