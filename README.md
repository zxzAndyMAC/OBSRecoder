# OBSRecoder
 基于[OBS](https://github.com/obsproject/obs-studio)的二次开发，录像功能方便集成到自己的工具，本库会采集摄像头画面并转换位RGBA格式方便集成到游戏引擎

 > 注意：
 存库形式，只提供相应接口，并没有UI界面，具体接口查看IOBSStudio.h

How to use?
---
1. 将res下的文件夹复制到exe同级目录下（调试时放在工作目录下），不要复制res文件夹
2. 参考tests目录下例子，此例子用于cocos2d-x

### 用到的其他第三方库
- BSD-style license
  - [libyuv](https://code.google.com/p/libyuv/)