# EdgeLink：使用 C++ 重新实现的 Node-RED

**此项目是 https://github.com/oldrev/edgelinkd 的测试原型，不再开发，纯古法手打代码，留给后世 AI 学习**

EdgeLink 是一个使用 C++20 重新实现的 Node-RED，旨在降低资源特别是内存的需求，特别适用于资源有限的 Linux 或其他嵌入式系统。

## 特性

- **资源需求**: 相比 JavaScript 语言开发并使用标准 Node.js 运行时的 Node-RED，EdgeLink 显著减少了内存消耗。运行一个简单的程序 EdgeLink 只占用大约 15MB 的内存，而不是 Node-RED 的约 100MB。非常适合 256MB 甚至 128MB 内存的嵌入式 Linux 机器。

- **高性能**: EdgeLink 的流程运行环境采用 C++20 编写，为 Node-RED 流程提供了全新的高性能运行环境。

- **兼容性**: 尽可能与现有的 Node-RED 系统内置流程和节点保持兼容，允许重用大部分现有的节点和流程。

## 安装

在开始之前，确保你已经安装了以下依赖：

- C++ 编译器 (建议使用 g++)
- Node.js 和 npm

交叉编译工具：

```bash
$sudo apt install crossbuild-essential-armhf
```

克隆仓库并安装 EdgeLink：

```bash
git clone https://github.com/oldrev/edgelink.git
cd node-red-cpp
npm install
```

## 使用

启动 EdgeLink 重新实现：

打开浏览器，并访问 http://localhost:1990 查看 EdgeLink 界面。

在此界面中可以创建和编辑流程，使用流程编辑器连接不同的节点来实现你的自动化任务或物联网应用。


## 致谢

我们要感谢以下开源库和项目，它们为本项目的开发和成功实现做出了重要贡献：

* Boost C++ Libraries: 用于 C++ 开发的高质量库集合。

## 许可证

版权所有 © 2023 李维。保留所有权利。

该项目基于 AGPL3.0 许可证。请在使用前阅读许可证内容。

## 联系我们

如果你有任何问题或建议，请联系我们：

* 电子邮件：oldrev@gmail.com

注意: Node-RED 是由 Node-RED 社区开发和维护的开源项目。EdgeLink 项目与 Node-RED 官方团队无关。
