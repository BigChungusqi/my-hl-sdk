项目简介
半条命 升级版、针锋相对 升级版 与 蓝色行动 升级版 是一系列代码仓库，提供更新优化版的半条命初代 SDK，专门适配三款官方发行的《半条命 1》PC 原版游戏。
项目用途
每个仓库均提供兼容 Visual Studio 2019 / 2022 的工程文件，并修复大量原版代码漏洞。《针锋相对》与《蓝色行动》项目为对应游戏的标准参考实现：完整保留原版游戏全部功能、原生代码逻辑（包括原版存在的重复代码设计）。
该系列升级版仓库的核心目标：
供模组制作者基于三款原作开发自定义模组，同时修复大量官方原版游戏存在的 BUG；
附带完整模组安装文件，普通玩家可直接安装游玩，一键打上全部修复补丁；
内置基于该 SDK 制作模组所需的全部依赖文件与基础资源。
本项目允许修改范围
游戏漏洞、程序 BUG 修复
代码结构优化（重构、逻辑通用化、代码精简）
（不包含游戏系统大规模重做，避免提高模组入门门槛、导致模组代码难以兼容合并）
修复游戏资源内的致命问题（例如触发卡死、流程锁死的地图触发器异常）
本项目禁止修改范围
画面画质升级、高清材质 / 光影改造
物理引擎改动
引擎底层功能修改
原版游戏玩法、机制调整
如需配置 SDK 环境、开发模组相关帮助，可前往：
TWHL 社区官网 或 官方 Discord 群组
TWHL 维基百科提供完整模组开发入门教程：https://twhl.info/wiki/page/Half-Life_Programming_-_Getting_Started
Discord 请查看 #welcome 频道了解规则；请勿在统一 SDK 频道提问基础问题，社区设有专门的模组求助分区。
项目最新动态与进度更新：https://twhl.info/thread/view/20055
运行该 SDK 编译模组的要求
仅支持Steam 最新正式版《半条命》；
运行《针锋相对》《蓝色行动》相关模组，需在 Steam 拥有并安装对应正版游戏（调用原版游戏资源）。
SDK 编译教程
详见文档：BUILDING.md
模组安装教程
详见文档：INSTALL.md
不支持的内容
不兼容旧版 WON 平台、远古版本 Steam 半条命
不正式支持 Xash 引擎（可能可勉强运行，但无适配优化）
升级版客户端无法联机原版服务器，原版客户端也无法进入升级版服务器
禁止将升级版游戏 DLL 文件直接覆盖到原版游戏目录使用
项目定位精简保守，不会新增大型功能与大幅度改版内容
经典模组：生死决斗 & 弹跳模式
《生死对决经典版》与《弹跳》两款模组的源码收录在原版半条命 SDK中。
本系列升级版仓库仅聚焦《半条命》本体及两款官方资料片，因此已移除上述两个小型模组源码。
由于原版源码无法在新版 VS 编译器下正常编译，社区已单独拆分适配仓库：
生死对决升级版：https://github.com/twhl-community/dmc-updated
弹跳模式升级版：https://github.com/twhl-community/ricochet-updated
注：这两个衍生仓库仅做基础编译修复，无后续功能更新与技术维护。
更新日志
常规更新日志：CHANGELOG.md
完整修复记录：FULL_UPDATED_CHANGELOG.md
半条命 1 SDK 官方许可协议
《半条命 1》SDK 版权所有 © 维尔福集团（Valve Corp.）
本协议为你与维尔福公司之间的法律合约。
下载、使用本半条命 1 SDK 前请仔细阅读协议条款。
下载及使用本源码引擎 SDK 即代表你同意本许可协议；若不接受条款，请勿下载、使用本开发工具包。
你可免费下载并使用本 SDK，基于半条命引擎开发、修改维尔福旗下游戏；
可免费分发修改后的游戏源码与程序文件，仅限非商业免费用途。
维尔福游戏用户协议参考：http://store.steampowered.com/subscriber_agreement/
你可自由复制、修改、二次分发 SDK 及个人修改代码，仅限免费共享；
所有二次分发版本必须附带原始 license.txt 与第三方授权文件 third_party_licenses.txt。
分发本 SDK 或其核心内容时，必须保留原版版权声明及以下免责条款：
产品免责声明
本源码 SDK 及所有附带文件均按现状提供，无任何明示或暗示保障。
维尔福及其合作方不承担任何商品适配性、无侵权、场景适配性等隐性担保责任。
责任限制条款
在任何情况下，维尔福公司不对任何特殊损失、意外损失、间接损失、衍生经济损失负责；
包含但不限于商业利润亏损、业务中断、资料丢失及其他财产损失，
即使维尔福已提前告知该类损失的潜在风险，也不承担相关赔偿责任。
如需将本 SDK 用于商业用途，请通过邮箱联系维尔福官方：
sourceengine@valvesoftware.com
半条命 1 本体说明
本文档为《半条命 1》引擎及关联游戏的官方说明文档。
可通过本仓库提交半条命 1 系列产品的漏洞反馈与功能建议。
问题反馈规范
遇到游戏异常时，请先在问题列表检索历史反馈（包含已关闭工单），避免重复提交。
新建反馈工单需包含以下完整信息：
简洁明确的问题标题
详细故障描述、命令行报错信息
完整复现步骤
电脑系统配置信息
游戏控制台内输入 version 指令的版本信息
日志内容请使用代码块格式粘贴，或上传至 Gist 在线文本工具。
配置信息获取方式：Steam 菜单栏 → 帮助 → 系统信息 → 全选复制，粘贴至反馈内容中。
社区讨论规范
所有参与讨论的用户需遵守基础行为准则：
禁止人身攻击、辱骂、贬低他人
禁止无意义重复刷屏、重复提交相同问题
反馈标题与内容禁止全部大写
禁止反复留言催促问题修复
温馨提示：
工单提交不代表问题会立刻定位修复；
未即时解决不代表官方停止排查，BUG 修复需要测试与迭代，请保持耐心。
贡献者名单
感谢所有为本项目贡献代码、修复问题的开发者：
Sam Vanheer、JoelTroch、malortie、dtugend、Revenant100、fel1x-developer、LogicAndTrick、FreeSlave、zpl-zak、edgarbarney、Toodles2You、Jengerer、thefoofighter、Maxxiii、johndrinkwater、anchurcn、DanielOaks、MegaBrutal、suXinjke、IntriguingTiles、Oxofemple、YaLTeR、Ronin4862、the man、vasiavasiavasia95、NongBenz、Hezus、Anton、ArroganceJustified、a1batross、zaklaus、Uncle Mike、Bacontsu、L453rh4wk、P38TaKjYzY、hammermaps、LuckNukeHunter99、Veinhelm、jay!、BryanHaley、λλλλλλ、Streit、rbar1um43、LambdaLuke87、almix、sabian
特别致谢
维尔福软件（Valve Software）
盖博克斯软件（Gearbox Software）
Alfred Reynolds、mikela-valve
TWHL 中文及全球模组社区
Knockout、Gamebanana、ModDB 模组平台