在build的資料夾下，有2個檔案要推到平台上。
adb push C:\ndk-usb-isp\build\nu_usb_isp_android /data/local/tmp
adb push C:\ndk-usb-isp\app\src\libs\arm64-v8a\libusb1.0.so /data/local/tmp

adb shell 
su

//adb shell
# (在 adb shell 中執行)
# 1. 進入您的工作目錄
cd /data/local/tmp

# 2. 停用 SELinux (USB 存取需要)
setenforce 0

# 3. 賦予 USB 裝置權限 (USB 存取需要)
chmod 0666 /dev/bus/usb/*/*

# 4. 賦予執行檔執行權限
chmod +x ./nu_usb_isp_android

# 5. ★★★ 告訴連結器函式庫在哪裡 ★★★
#    ( . 代表「目前資料夾」)
export LD_LIBRARY_PATH=./


./nu_usb_isp_android ./firmware.bin
