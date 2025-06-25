# C2000 Bootloader & DFU
C2000 - F280039C Bootloader and deivce field upgrade, Firmware(C) and Software(C#)

# Introduction
# Ordinary MCU code without bootloader
When power up/MCU reset, CPU will start at address 0x80000.
Then, it will jump to the application code area.
0x80000 is the CPU default boot address. 

![image](https://github.com/user-attachments/assets/0b043ccc-3c78-411a-95ba-9fa7855eb4a6)

# MCU code with bootloader
1. MCU flash seperated into two region: Bootloader, App Code.
2. The Bootloader firwmare code occupied 3 sections of flash, 12k byte size.
3. When power up/MCU reset, CPU will start at address 0x80000.
4. CPU will jump to a bootloader firmware after boot up. (content start at 0x80002)
5. There is a PC software communicates with the C2000 MCU via UART link.
6. The bootloader firmware includes C2000 MCU flash API, it can erase, program the app code region.
7. The bootloader firmware can also jump to the app code codestart depends on user request.
8. Bootloader and app code flash region cannot be overlapped, otherwise it will damage the bootloader firmware.
   
![image](https://github.com/user-attachments/assets/20c78f4b-6743-4317-9eea-67f69c790d56)

# Bootloader firmware 
![image](https://github.com/user-attachments/assets/16c789e8-91b6-4a21-aedd-26af49aa03ff)

# Application code firmware flash setting

1. In CCS, please open the flash.cmd
2. Modify the BEGIN address area from 0x80000 to 0x83000. Once the BEGIN address is changed, the application firmware will not be able to boot through the C2000 default settings, but will rely on the bootloader to boot into the application firmware.
3. Ensure all the application code flash regions are not located from 0x80000 to 0x82FFF (12k bytes)
4. Compile the code

![image](https://github.com/user-attachments/assets/168e5bfb-c395-475a-be8c-25b5e7d19134)

# How to generate txt file from Application code
1. the compiled file format is .out and cannot directly applied on DFU, we need to convert to .txt.
2. Copy the compiled .out file to a folder with hex2000.exe
   ![image](https://github.com/user-attachments/assets/544effbe-df7e-4cc5-b783-666263b92e29)
3. Open the cmd in the same file location. Run “hex2000.exe –boot –sci8 –a –o <output file.txt> <input file.out>”
   ![image](https://github.com/user-attachments/assets/bf008e02-39a1-4242-be0b-981cd63403e3)
4. Then, a txt file is generated.
   
   ![image](https://github.com/user-attachments/assets/1540030a-17d2-4bc2-a050-9e08483fec6a)

# How to update the firmware 
The project is using LAUNCHXL-F280039C to demonstrate the bootloader firmware function

![image](https://github.com/user-attachments/assets/ed83fd24-0cfa-4f68-84ae-8efe955ec223)

1. Open the C2000 DFU Tool, click Browse and choose the app fw in txt format.
2. Plug in the USB, and lanuchpad power on
3. Press refresh and choose the lanuchpad COM port and press “connect”.
4. Press “DFU Start” to start the firmware upgrade
5. If lanuchpad without any DFU request for 10s after bootup, lanuchpad will boot to the application fw (address: 0x83000)

![image](https://github.com/user-attachments/assets/cf3f9d30-c5fc-4216-8cc4-b2d4d271d0fe)

