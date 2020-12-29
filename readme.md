# Updater

利用GIT进行软件更新的命令行工具

命令行：

```
  -h [ --help ]         显示帮助
  -c [ --config ] arg   指定配置文件，不指定的话依据查找：1. updater.cfg文件，2. 自包含
  -o [ --output ] arg   将配置文件和更新工具打包成一个exe文件
  -f [ --fetch ]        类似于git fetch命令，从远端下载仓库
  -g [ --get-files ]    显示当前文件状态
  -l [ --get-log ]      显示当前版本状态
  -u [ --do-update ]    执行更新，不删除多余文件
  -r [ --do-reset ]     执行更新，删除多余文件，即与远端一致
  -b [ --before ] arg   在更新启动前执行指定程序
  -a [ --after ] arg    在更新后执行指定程序
  -w [ --wait ] arg     等待指定的process id结束再开始更新（最多等5秒）.
  --gui                 出错时显示弹出框
  --restart             更新完成后再次启动--wait指定的进程，需与--wait配合
```



配置文件：

```yaml
git:
  remote: git仓库地址
  branch: git仓库分支
  local: 本地目录，可以用相对路径
```



## 用法

程序可以在线程中定时执行`updater -f` 拉取然后用`updater -l`得到状态，状态以`yaml`的格式打印在stdout中，程序需要自己重定向管道读取stdout。

如果软件有更新（即git仓库更新），执行`updater -w <当前processId> -r --gui --restart`更新并立即退出当前软件。