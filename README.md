# ESP32 Solar & Battery Monitor Firmware

Este proyecto implementa un sistema de monitorización robusto para instalaciones solares autónomas basado en ESP32. El firmware gestiona la lectura de sensores de corriente/potencia (INA219), luminosidad (LDRs) y estima el Estado de Carga (SoC) de la batería, enviando toda la telemetría a un broker MQTT (compatible con ThingsBoard).

Incluye un sistema de **Aprovisionamiento WiFi** mediante Portal Cautivo para configurar credenciales sin recompilar el código.

---

## 📋 Características Principales

* **Monitorización de Energía Dual:** Lectura precisa de Voltaje, Corriente y Potencia para Panel Solar y Batería mediante dos sensores **INA219** sobre bus I2C.
* **Sensores Ambientales:** Lectura de 4 resistencias dependientes de la luz (LDR) utilizando el ADC del ESP32 con calibración OneShot.
* **Estimación Inteligente de Batería:** Algoritmo híbrido que combina Tabla de Voltaje (LUT) para reposo y Conteo de Coulomb (Ah counting) para dinámicas de carga/descarga.
* **Conectividad Robusta:**
    * **Modo AP (Configuración):** Si no hay credenciales o falla la conexión, levanta un Punto de Acceso con Portal Cautivo para configurar WiFi vía web.
    * **Cliente MQTT:** Reconexión automática y envío de telemetría JSON optimizada para ThingsBoard.
* **Arquitectura RTOS:** Tareas independientes para sensores y comunicaciones sincronizadas mediante Mutex para la integridad de datos.

---

## 🛠️ Hardware y Conexiones

El sistema está diseñado para el SoC **ESP32**. A continuación se detalla el mapa de conexiones (Pinout) configurado por defecto en `Kconfig`.

### Bus I2C (Sensores de Energía)
| Dispositivo | Dirección I2C | Pin SDA | Pin SCL | Notas |
| :--- | :--- | :--- | :--- | :--- |
| **ESP32 Master** | N/A | GPIO 21 | GPIO 22 | Configurable en Menuconfig |
| **INA219 (Panel)** | `0x40` | - | - | Puente A0/A1 abierto (Default) |
| **INA219 (Batería)**| `0x41` | - | - | **Requiere soldar puente A0** |

### ADC (Sensores de Luz)
Se utilizan los canales del ADC1 con atenuación de 11dB (Rango 0-3.3V).

| Sensor | Canal ADC | Pin ESP32 | Descripción |
| :--- | :--- | :--- | :--- |
| **LDR 1** | ADC1_CH4 | GPIO 32 | Sensor de luz cuadrante 1 |
| **LDR 2** | ADC1_CH5 | GPIO 33 | Sensor de luz cuadrante 2 |
| **LDR 3** | ADC1_CH6 | GPIO 34 | Sensor de luz cuadrante 3 (Input Only) |
| **LDR 4** | ADC1_CH7 | GPIO 35 | Sensor de luz cuadrante 4 (Input Only) |

---

## 📂 Estructura del Proyecto

El código sigue la estructura estándar de componentes de **ESP-IDF**:

```text
├── main/
│   ├── main.c              # Punto de entrada, orquestación de tareas RTOS
│   ├── Kconfig.projbuild   # Opciones de configuración del menú (menuconfig)
│   ├── protect.h           # Definición de Mutex global
│   │
│   ├── modules/
│   │   ├── adc.c/.h            # Driver para lectura de LDRs y conversión a Ohms
│   │   ├── ina.c/.h            # Driver para sensores INA219 (I2C)
│   │   ├── battery.c/.h        # Algoritmo de cálculo de SoC
│   │   ├── mqtt_protocol.c/.h  # Cliente MQTT y serialización JSON
│   │   ├── nvs_managment.c/.h  # Gestión de almacenamiento no volátil (Flash)
│   │   ├── wifi_managment.c/.h # Máquina de estados WiFi (STA/AP)
│   │   └── web_managment.c/.h  # Servidor Web y API para configuración
│   │
│   └── CMakeLists.txt
├── CMakeLists.txt
└── README.md
```

## ⚙️ Configuración e Instalación

### 1. Requisitos Previos
* ESP-IDF (v5.0 o superior recomendado).
* Toolchain para ESP32 configurado.

### 2. Clonar y Configurar
```bash
git clone <url-del-repo>
cd <nombre-del-repo>
```

## 3. Configuración del Firmware (Menuconfig)

Es crucial configurar los parámetros antes de compilar. Ejecuta:
Bash
```bash
idf.py menuconfig
```
Navega a "Configuración del Sistema" y ajusta los siguientes valores según tu hardware:

   * **WiFi (Modo AP)**: * SSID del AP: Define el nombre de la red que generará el ESP32 si no tiene configuración (Default: ESP32_CONFIG).

   * **Batería y Energía**:

      * Capacidad de la Batería (Ah): Ajusta este valor a la capacidad real de tu batería (ej. 2.6 para una celda 18650 típica).

       * Resistencia Shunt: Generalmente 0.1 Ohm para módulos INA219 estándar.

       * Corriente Máxima: Ajusta los rangos máximos esperados para el panel y la batería.

   * **Configuración MQTT**:

       * URL del Broker: Ej. mqtt://demo.thingsboard.io (o la IP de tu servidor).

       * Token de Acceso: Pega aquí el token de tu dispositivo de ThingsBoard.

      * Tópico de Telemetría: Por defecto v1/devices/me/telemetry.
