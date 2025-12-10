idf_component_register(
    SRCS 
    	"src/wifi_managment.c" 
    	"src/nvs_managment.c" 
    	"src/web_managment.c" 
    	"src/mqtt_protocol.c" 
    	"src/telegram_bot.c" 
  
    INCLUDE_DIRS  
    	"include" 
    REQUIRES   
    	# Librerias ESP-IDF necesarias
    	esp_wifi            # Para wifi_managment
        esp_event           # Para eventos WiFi/IP
        nvs_flash           # Para guardar credenciales
        esp_http_server     # Para el portal cautivo y OTA
        esp_http_client     # Para el Bot de Telegram
        esp_https_ota       # Para la actualización remota
        app_update          # Contiene esp_ota_ops (operaciones OTA de bajo nivel)
        mqtt         # Cliente MQTT
        json                # cJSON para formatear los mensajes
        mbedtls      # Para certificados SSL (Telegram/HTTPS)
        
        # Otros componentes (Dependencias internas)
        sensors
        logic
)