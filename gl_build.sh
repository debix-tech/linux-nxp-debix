echo "build kernel"

export PATH=$PATH:/opt/toolchain/gcc-arm-9.2-2019.12-x86_64-aarch64-none-linux-gnu/bin
export ARCH=arm64
export CROSS_COMPILE=aarch64-none-linux-gnu-

#make menuconfig
#return

make imx_v8_defconfig
find arch/arm64/boot/dts -name *.dtb | xargs rm
make -j128
#make -j32 Image
#make -j32 freescale/imx8mp-evk.dtb
#make -j32 freescale/imx8mp-evk-raspberrypi-touchscreen.dtb
#make -j32 freescale/imx8mp-debix-lora-board.dtb
#make -j32 freescale/imx8mp-debix-io-board.dtb
#make -j32 freescale/imx8mp-evk-1080p-lvds.dtb
#make -j32 freescale/imx8mp-evk-HC080IY28026-D60V.C.dtb
#make -j32 freescale/imx8mp-evk-HC050IG40029-D58V.C-lvds-panel.dtb
#make -j32 freescale/imx8mp-evk-lvds1080p-mipiHC080IY28026.dtb

rm -rf image-out*
mkdir -p image-out/boot

cp arch/arm64/boot/Image image-out/boot/.
#cp arch/arm64/boot/dts/freescale/imx8mp-evk.dtb image-out/boot/.
cp arch/arm64/boot/dts/freescale/*.dtb image-out/boot/.
return 
#cp arch/arm64/boot/dts/freescale/imx8mp-evk.dtb image-out
#cp arch/arm64/boot/dts/freescale/imx8mp-debix-lora-board.dtb image-out
#cp arch/arm64/boot/dts/freescale/imx8mp-debix-io-board.dtb image-out
#cp arch/arm64/boot/dts/freescale/imx8mp-evk-lvds1080p-mipiHC080IY28026.dtb image-out
#cp arch/arm64/boot/dts/freescale/imx8mp-evk-1080p-lvds.dtb image-out
#cp arch/arm64/boot/dts/freescale/imx8mp-evk-HC080IY28026-D60V.C.dtb image-out
#cp arch/arm64/boot/dts/freescale/imx8mp-evk-raspberrypi-touchscreen.dtb image-out
#cp arch/arm64/boot/dts/freescale/imx8mp-evk-HC050IG40029-D58V.C-lvds-panel.dtb image-out

make -j128 modules
make INSTALL_MOD_PATH=image-out INSTALL_MOD_STRIP=1 modules_install

tar cpf image-out.tar image-out
return


