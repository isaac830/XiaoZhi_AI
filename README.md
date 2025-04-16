# CUBICAT框架实现的XiaoZhi AI客户端

## 使用

1. Cubicat s3 开发板 编译即可使用, 其他开发板需要自己修改cubicat.cpp里的引脚定义，并且修改一些硬件设置代码
2. 屏幕目前只支持ST7789驱动
3. ENABLE_JS 可以决定是否使用JavaScript编写逻辑功能，目前只导出了少量接口，主要是图形创建接口和loop onTouch这些系统接口，JS只支持ES5.1严格模式的subset，如果需要自己导出接口可以在.h文件中将想要导出的函数用[JS_BINDING_BEGIN]和[JS_BINDING_END]进行包裹，编译时会自动生成JS代码绑定，但需要注意的是导出函数的参数只支持基本类型（int, float, bool, std::string等）和指针类型，无法支持自定义数据结构，返回值也是一样
4. 使用JS编写逻辑可以不编译c++代码，写好的js代码放入spiffs_img文件夹，然后执行burn_tool目录下的spiffs_make_and_burn.py文件，会弹出烧录界面，选择好烧录的文件夹就可以直接烧录了
5. 网络连接，第一次连接使用SmartConfig连接，你只需要在微信里搜索小程序：一键配网，然后点击连接就可以了，设备会自己寻找网络并连接，需要注意的是：！！！必 须 使 用 2.4G 网 络！！！，连接成功后第二次连接会使用上一次成功连接的Wifi ssid和password自动连接无需再次配网。如果网络改变，无法连接上次的网络，在等待5次自动重连后会再进入配网流程，你只需要再用小程序：一键配网操作一次就可以了，如果不想使用SmartConfig进行配网，只需要在CUBICAT.Wifi.connect后面填入自己的WI-FI 名字和密码就行了
