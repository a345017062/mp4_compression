# mp4_compression
compress mp4 file faster

# 使用前提
使用这个工具的前提，是你已经在目标平台（如Android、iOS、Linux等）成功编译了ffmpeg，且集成了libx264，libfaac两个codec。

# 参考来源
工具源自于雷霄骅的CSDN博客（http://blog.csdn.net/leixiaohua1020/article/details/47072673）<br>
这里先向这位刚刚去逝不到一个月的年青码农致敬，他在ffmpeg应用方面贡献了大量的文档、代码。

# 工具介绍
这个工具目标是在保证质量几乎没有损失的前提下，最大化压缩iOS、Android等手机录制的mp4视频文件，再想办法尽量提高压缩速度。video encoder使用了libx264，audio encoder使用了libfaac。如有效率更高的codec，欢迎邮件交流。345017062@qq.com<br>
  在雷霄骅的代码基础上，做了以下几点优化：<br>
  1、把codec换成libx264、libfaac，并设置libx264的参数为-crf 26。<br>
  2、去除filter。<br>
  3、frame不再每次都alloc、free。<br>
  4、去掉codec每一个frame时都在print的log。<br>
  5、缩小fps到film级别（24）。这项尚未完成，正在努力中。<br>
  6、x264参数调配对编码速度、压缩率的影响。已经完成，对码率影响明显，对压缩速度影响不明显<br>
  7、多线程。探索中。整体时间都消耗在encode过程中，I/O及内存分配消耗的时间可以忽略不计，而encode具有严格的线性特点。

# 交流
这个工具应该使用范围挺广的，发布上来的主要目的，是希望有人能提出更多的优化意见，共同进步。<br>
随时欢迎邮件交流。345017062@qq.com<br>
