





zephyr_library()

# 指定目标的头文件路径
zephyr_library_include_directories(src)
zephyr_library_include_directories(tinyusb/src)
zephyr_library_include_directories(tinyusb/src/common)
zephyr_library_include_directories(tinyusb/src/host)
zephyr_library_include_directories(tinyusb/src/class/hid)
zephyr_library_include_directories(tinyusb/src/class/msc)
zephyr_library_include_directories(tinyusb/src/class/cdc)
zephyr_library_include_directories(tinyusb/src/class/cdc/serial)
zephyr_library_include_directories(tinyusb/src/portable/nxp/khci)
zephyr_library_include_directories(tinyusb/src/portable/nxp/lpc17_40)
zephyr_library_include_directories(tinyusb/src/portable/nxp/lpc_ip3511)
zephyr_library_include_directories(tinyusb/src/portable/ehci)
zephyr_library_include_directories(tinyusb/src/portable/chipidea/ci_hs)

zephyr_library_include_directories_ifdef(CONFIG_TINYUSB_CHIP_1060 tinyusb/hw/bsp/imxrt/boards/mimxrt1060_evk/board)
zephyr_library_include_directories_ifdef(CONFIG_TINYUSB_CHIP_1170 tinyusb/hw/bsp/imxrt/boards/mimxrt1170_evkb/board)


# 包含一个目录下的所有.c源文件
aux_source_directory(tinyusb/src tinyusb_root)
aux_source_directory(tinyusb/src/common tinyusb_common)
aux_source_directory(tinyusb/src/host tinyusb_host)
aux_source_directory(tinyusb/src/class/hid tinyusb_class_hid)
aux_source_directory(tinyusb/src/class/msc tinyusb_class_msc)
aux_source_directory(tinyusb/src/class/cdc tinyusb_class_cdc)
# aux_source_directory(tinyusb/src/class/cdc/serial tinyusb_class_cdc_serial)
aux_source_directory(tinyusb/src/portable/nxp/khci tinyusb_portable_nxp_khci)
aux_source_directory(tinyusb/src/portable/nxp/lpc17_40 tinyusb_portable_nxp_lpc17_40)
aux_source_directory(tinyusb/src/portable/nxp/lpc_ip3511 tinyusb_portable_nxp_lpc_ip3511)

aux_source_directory(tinyusb/src/portable/ehci tinyusb_portable_ehci)
aux_source_directory(tinyusb/src/portable/chipidea/ci_hs/ tinyusb_portable_chipidea_ci_hs)

aux_source_directory(tinyusb/hw/bsp/imxrt/boards/mimxrt1060_evk/board/ tinyusb_board)


# 将所有源文件列表添加到目标源文件中
zephyr_library_sources(src/cdc_app.c)
zephyr_library_sources(src/hid_app.c)
zephyr_library_sources(src/msc_app.c)
zephyr_library_sources(src/tinyusb_host_driver.c)
zephyr_library_sources(${tinyusb_root})
zephyr_library_sources(${tinyusb_common})
zephyr_library_sources(${tinyusb_host})
zephyr_library_sources(${tinyusb_class_hid})
zephyr_library_sources(${tinyusb_class_msc})
zephyr_library_sources(${tinyusb_class_cdc})
# zephyr_library_sources(${tinyusb_class_cdc_serial})
zephyr_library_sources(${tinyusb_portable_nxp_khci})
zephyr_library_sources(${tinyusb_portable_nxp_lpc17_40})
zephyr_library_sources(${tinyusb_portable_nxp_lpc_ip3511})
zephyr_library_sources(${tinyusb_portable_ehci})
zephyr_library_sources(${tinyusb_portable_chipidea_ci_hs})
# zephyr_library_sources(${tinyusb_board})


zephyr_library_link_libraries(FS)
target_link_libraries_ifdef(CONFIG_FAT_FILESYSTEM_ELM   FS INTERFACE ELMFAT)