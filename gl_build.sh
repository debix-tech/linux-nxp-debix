source /opt/fsl-imx-xwayland/6.12-styhead/environment-setup-armv8a-poky-linux
export ARCH=arm64
#make distclean
#make menuconfig
make imx_v8_defconfig
rm arch/arm64/boot/dts/freescale/*.dtb
make -j32

if [ $? != 0 ] ; then
	echo "build kernel err !!!"
	return
fi
rm -rf image_out
mkdir -p image_out/boot

cp arch/arm64/boot/dts/freescale/*.dtb image_out/boot/.

cp arch/arm64/boot/Image image_out/boot/.

#return

make -j32 modules

if [ $? != 0 ] ; then
	echo "build modules err !!!"
	return
fi

make INSTALL_MOD_PATH=image_out INSTALL_MOD_STRIP=1 modules_install
#mv image_out/lib/modules image_out/.
#rm -rf image_out/lib
tar cpf image_out.tar image_out
