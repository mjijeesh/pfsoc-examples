# PolarFire SoC Baremetal BIOS v2


Added the command history for the BIOS. takes around 79KB total Now. without command history it was 75KB.
Changed the abnner to Poalrfire SoC.

## How to use this example


### DDR training and Renode
If the firmware has DDR training enabled, then the application will take significantly longer to start up in Renode. Training has no practical impact in this environment as the emulated DDR memory is already reliable.

Training can be controlled by removing `#define DDR_SUPPORT` in your `mss_sw_config.h` file. This change should be made in `src\boards\[BOARD]\platform_config\[BUILD-CONFIGURATION]\mpfs_hal_config\`

If your project uses the default configuration file in `src\platform\platform_config_reference\` to enable DDR training, it is recommended to create a copy under the boards directory and disable `DDR_SUPPORT` there.
