deps_config := \
	/home/kolja/esp/esp-adf/esp-idf/components/app_trace/Kconfig \
	/home/kolja/esp/esp-adf/esp-idf/components/aws_iot/Kconfig \
	/home/kolja/esp/esp-adf/esp-idf/components/bt/Kconfig \
	/home/kolja/esp/esp-adf/esp-idf/components/driver/Kconfig \
	/home/kolja/esp/esp-adf/esp-idf/components/esp32/Kconfig \
	/home/kolja/esp/esp-adf/esp-idf/components/esp_adc_cal/Kconfig \
	/home/kolja/esp/esp-adf//components/esp_http_client/Kconfig \
	/home/kolja/esp/esp-adf/esp-idf/components/ethernet/Kconfig \
	/home/kolja/esp/esp-adf/esp-idf/components/fatfs/Kconfig \
	/home/kolja/esp/esp-adf/esp-idf/components/freertos/Kconfig \
	/home/kolja/esp/esp-adf/esp-idf/components/heap/Kconfig \
	/home/kolja/esp/esp-adf//components/http_server/Kconfig \
	/home/kolja/esp/esp-adf/esp-idf/components/libsodium/Kconfig \
	/home/kolja/esp/esp-adf/esp-idf/components/log/Kconfig \
	/home/kolja/esp/esp-adf/esp-idf/components/lwip/Kconfig \
	/home/kolja/esp/esp-adf/esp-idf/components/mbedtls/Kconfig \
	/home/kolja/esp/esp-adf/esp-idf/components/nvs_flash/Kconfig \
	/home/kolja/esp/esp-adf/esp-idf/components/openssl/Kconfig \
	/home/kolja/esp/esp-adf/esp-idf/components/pthread/Kconfig \
	/home/kolja/esp/esp-adf/esp-idf/components/spi_flash/Kconfig \
	/home/kolja/esp/esp-adf/esp-idf/components/spiffs/Kconfig \
	/home/kolja/esp/esp-adf/esp-idf/components/tcpip_adapter/Kconfig \
	/home/kolja/esp/esp-adf/esp-idf/components/vfs/Kconfig \
	/home/kolja/esp/esp-adf/esp-idf/components/wear_levelling/Kconfig \
	/home/kolja/esp/esp-adf/esp-idf/components/bootloader/Kconfig.projbuild \
	/home/kolja/esp/esp-adf//components/esp-adf-libs/Kconfig.projbuild \
	/home/kolja/esp/esp-adf/esp-idf/components/esptool_py/Kconfig.projbuild \
	/home/kolja/esp/esp-adf/esp-idf/components/partition_table/Kconfig.projbuild \
	/home/kolja/esp/esp-adf/esp-idf/Kconfig

include/config/auto.conf: \
	$(deps_config)

ifneq "$(IDF_CMAKE)" "n"
include/config/auto.conf: FORCE
endif

$(deps_config): ;
