sudo snap install qtdeployer
$qtdeployer -bin appdukto6 -qmake /home/qaleb/Qt/6.9.0/gcc_64/bin/qmake -qmlDir /home/qaleb/coding/dukto6/build/Desktop_Qt_6_9_0-Release/dukto6/src/qml/dukto6
$mkdir -p dukto6_1.0.0/DEBIAN
$mkdir -p dukto6_1.0.0/usr/local/dukto/bin
$mkdir -p dukto6_1.0.0/usr/local/dukto/lib
$mkdir -p dukto6_1.0.0/usr/local/dukto/plugins
$mkdir -p dukto6_1.0.0/usr/local/dukto/qml
$mkdir -p dukto6_1.0.0/usr/share/applications
$nano dukto6_1.0.0/usr/share/applications/dukto.desktop

$nano dukto6_1.0.0/DEBIAN/control
$cp DistributionKit/appdukto6.sh dukto6_1.0.0/usr/local/dukto/dukto.sh
$chmod +x dukto6_1.0.0/usr/local/dukto/dukto.sh
$dpkg-deb --build dukto6_1.0.0

# Installing the .deb app
sudo dpkg -i dukto6_1.0.0.deb

sudo apt update
sudo apt install imagemagick

for size in 16 24 32 48 64 128 256 512; do
  mkdir -p "usr/share/icons/hicolor/${size}x${size}/apps"
convert dukto-x4.png -resize ${size}x${size} "usr/share/icons/hicolor/${size}x${size}/apps/dukto.png"
