import os
import shutil
import subprocess
import sys
import time

# ================= 配置区域 =================
# 需要编译的游戏列表
GAMES = ["hl2", "episodic", "portal", "cstrike", "hl1", "hl1mp", "hl2mp"]

# 路径配置（会自动解析 ~ 为当前用户主目录）
HOME_DIR = os.path.expanduser("~")
BUILD_PREFIX = "../android_build"
TARGET_BASE_DIR = os.path.join(HOME_DIR, "SourceAppBuildGames")
# ============================================


def run_command(command, shell=True):
    """运行系统命令并检查返回值"""
    print(f"--> 正在执行: {command}")
    result = subprocess.run(command, shell=shell)
    if result.returncode != 0:
        print(f"❌ 错误: 命令执行失败，退出码 {result.returncode}")
        sys.exit(1)


def main():
    # 记录整个脚本开始的时间
    total_start_time = time.time()
    
    # 确保备份的目标根目录存在
    if not os.path.exists(TARGET_BASE_DIR):
        os.makedirs(TARGET_BASE_DIR)
        print(f"已创建目标根目录: {TARGET_BASE_DIR}")

    for game in GAMES:
        # 记录单个游戏开始的时间
        game_start_time = time.time()
        
        print("\n" + "=" * 60)
        print(f"🚀 开始处理游戏: {game}")
        print("=" * 60)

        # 1. 编译前清理
        print(f"[{game}] 正在执行编译前清理...")
        run_command("python waf clean")

        # 2. 配置环境 (Configure)
        configure_cmd = (
            f"python3 ./waf configure -T release "
            f"--prefix={BUILD_PREFIX} "
            f"--android=aarch64,llvm,29 "
            f"--target=android_build "
            f"--togl "
            f"--disable-warns "
            f"--togles "
            f"--use-ccache "
            f"--build-game={game}"
        )
        print(f"[{game}] 正在配置环境...")
        run_command(configure_cmd)

        # 3. 执行编译与安装 (Install)
        print(f"[{game}] 正在编译并执行安装...")
        run_command("python waf install --strip")

        # 4. 移动文件到备份目录
        game_target_dir = os.path.join(TARGET_BASE_DIR, game)
        print(f"[{game}] 正在将编译产物移动到: {game_target_dir}")

        if os.path.exists(game_target_dir):
            shutil.rmtree(game_target_dir)
        os.makedirs(game_target_dir)

        if os.path.exists(BUILD_PREFIX):
            files = os.listdir(BUILD_PREFIX)
            if not files:
                print(f"⚠️ 警告: {BUILD_PREFIX} 文件夹为空，请检查编译是否成功生成了文件！")
            for item in files:
                source_item = os.path.join(BUILD_PREFIX, item)
                target_item = os.path.join(game_target_dir, item)
                shutil.move(source_item, target_item)
            print(f"✨ [{game}] 产物已成功移动并归档。")
        else:
            print(f"❌ 错误: 未找到编译输出目录 {BUILD_PREFIX}，请确认编译步骤是否有误。")
            sys.exit(1)

        # 5. 编译后清理
        print(f"[{game}] 正在执行善后清理...")
        run_command("python waf clean")

        # 计算并打印单个游戏的耗时
        game_elapsed = time.time() - game_start_time
        print(f"⏱️  [{game}] 处理完毕，耗时: {game_elapsed:.2f} 秒")

    # 计算并打印总耗时
    total_elapsed = time.time() - total_start_time
    
    print("\n" + "=" * 60)
    print("🎉 所有游戏的 [配置 -> 编译 -> 归档 -> 清理] 任务已全部完成！")
    print(f"📊 任务总计耗时: {total_elapsed:.2f} 秒 (约 {total_elapsed / 60:.2f} 分钟)")
    print("=" * 60)


if __name__ == "__main__":
    main()