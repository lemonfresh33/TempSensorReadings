arduino-cli compile --verbose --log-level debug --fqbn esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,PartitionScheme=app3M_fat9M_16MB,USBMode=hwcdc .
# arduino-cli upload -p COM5 --fqbn esp32:esp32:esp32s3 .
arduino-cli upload -p 192.168.1.64 --fqbn esp32:esp32:esp32s3 --upload-field password=your_password .