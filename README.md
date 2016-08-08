# mp4_compression
compress mp4 file faster

# 使用前提
使用这个工具的前提，是你已经在目标平台（如Android、iOS、Linux等）成功编译了ffmpeg，且集成了libx264，libfaac两个codec。

# 参考来源
  工具源自于雷霄骅的CSDN博客（http://blog.csdn.net/leixiaohua1020/article/details/47072673）
  这里先向这位刚刚去逝不到一个月的年青码农致敬，他在ffmpeg应用方面贡献了大量的文档、代码。

# 工具介绍
这个工具目标是在保证质量几乎没有损失的前提下，最大化压缩iOS、Android等手机录制的mp4视频文件，再想办法尽量提高压缩速度。video encoder使用了libx264，audio encoder使用了libfaac。如有效率更高的codec，欢迎邮件交流。345017062@qq.com
  在雷霄骅的代码基础上，做了以下几点优化：
  1、把codec换成libx264、libfaac，并设置libx264的参数为-crf 26。
  2、去除filter。
  3、frame不再每次都alloc、free。
  4、去掉codec每一个frame时都在print的log。
  5、缩小fps到film级别（24）。这项尚未完成，正在努力中。
