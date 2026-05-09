[English](./README.md) | [简体中文](./README_CN.md)

### 编译服务器
推荐最低Ubuntu版本是22.04或者更高
必需的软件包：
```
sudo apt-get install build-essential chrpath cpio debianutils diffstat file gawk gcc git iputils-ping libacl1 liblz4-tool locales python3 python3-git python3-jinja2 python3-pexpect python3-pip python3-subunit socat texinfo unzip wget xz-utils zstd efitools
```

### 下载编译器
```shell
## L6.12.49 使用的是 gcc 14.3 
debix@polyhex:$ wget https://armkeil.blob.core.windows.net/developer/Files/downloads/gnu/14.3.rel1/binrel/arm-gnu-toolchain-14.3.rel1-x86_64-aarch64-none-linux-gnu.tar.xz

debix@polyhex:$ tar xpf arm-gnu-toolchain-14.3.rel1-x86_64-aarch64-none-linux-gnu.tar.xz -C /opt/toolchain
```

### 配置编译环境
```
export PATH=$PATH:/opt/toolchain/arm-gnu-toolchain-14.2.rel1-x86_64-aarch64-none-linux-gnu/bin
export ARCH=arm64
export CROSS_COMPILE=aarch64-none-linux-gnu-
```
  
### 编译dtb Image
```
make imx_v8_defconfig
make -j32
```
 
生成文件
arch/arm64/boot/dts/freescale/imx93-bmb-13-a1.dtb
arch/arm64/boot/dts/freescale/imx91-bmb-13-a1.dtb
arch/arm64/boot/Image

拷贝 dtb 和 Image 到Debix设备 /boot/ 目录里完成新内核更换。

 
