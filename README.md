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
│   │   ├── web_managment.c/.h  # Servidor Web y API para configuración
│   │   ├── servo_control.c/.h  # Driver para configurar el servo
|   |   └── solar_tracker.c/.h  # Driver para unir los datos leidos del ADC con el servo
│   │
│   └── CMakeLists.txt
├── CMakeLists.txt
└── README.md
```

---

## ⚙️ Configuración e Instalación

#### 1. Requisitos Previos
* ESP-IDF (v5.0 o superior recomendado).
* Toolchain para ESP32 configurado.

#### 2. Clonar y Configurar
```bash
git clone <url-del-repo>
cd <nombre-del-repo>
```

#### 3. Configuración del Firmware (Menuconfig)

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
#### 4. Compilar y Flashear

Conecta el ESP32 al puerto USB y ejecuta:
```bash
idf.py build flash monitor
```

---

## 🚀 Guía de Uso

**Primer Arranque (Modo Aprovisionamiento)**

1. Al encender el dispositivo por primera vez (o tras borrar la flash), no encontrará credenciales WiFi guardadas.

2. El sistema levantará automáticamente un Punto de Acceso (AP).

3. Busca en tu ordenador o móvil la red WiFi llamada ESP32_CONFIG (o el nombre que configuraste) y conéctate.

4. Debería abrirse automáticamente una ventana de inicio de sesión (Portal Cautivo). Si no ocurre, abre un navegador y ve a http://192.168.4.1.

5. Verás una lista de las redes WiFi detectadas. Selecciona tu red doméstica.

6. Introduce la contraseña y pulsa el botón Connect & Save.

7. El ESP32 guardará las credenciales en la memoria no volátil (NVS), se reiniciará automáticamente y se conectará a internet.

**Operación Normal**

Una vez configurado y conectado a la red WiFi, el dispositivo entra en su ciclo de trabajo normal:

1. Monitorización: Lee los sensores de corriente cada 1 segundo y los niveles de luz cada 3 segundos.

2. Cálculo: Actualiza el algoritmo de SoC de la batería.

3. Transmisión: Cada 5 segundos, envía un paquete JSON al broker MQTT configurado.

Estructura de Datos MQTT

Los datos se envían al tópico configurado con la siguiente estructura JSON, lista para ser visualizada en dashboards como ThingsBoard:
``` JSON
{
  "solarVoltage": 18.50,   // Voltaje del Panel (V)
  "solarCurrent": 0.45,    // Corriente del Panel (A)
  "solarPower": 8.32,    // Potencia del Panel (W)
  "batteryVoltage": 4.10,      // Voltaje de la Batería (V)
  "batteryCurrent": 0.20,      // Corriente de Batería (A, +Descarga / -Carga)
  "batteryPower": 0.82,      // Potencia de Batería (W)
  "batteryChargeLvl": 95.4,    // Estado de Carga estimado (%)
  "ldr_1": 15.2,      // Resistencia LDR 1 (kOhms)
  "ldr_2": 18.1,      // Resistencia LDR 2 (kOhms)
  "ldr_3": 50.5,      // Resistencia LDR 3 (kOhms)
  "ldr_4": 48.2       // Resistencia LDR 4 (kOhms)
  "servo_h": 134     // Grados del servo en el eje X
  "servo_v": 90      // Grados del servo en el eje Y
}
```

## 🧩 Estado del Proyecto

* [x] Drivers I2C para doble sensor INA219.

* [x] Lectura y conversión de ADC para matriz de LDRs.

* [x] Algoritmo de estimación de SoC (Voltaje + Coulomb Counting).

* [x] Servidor Web embebido para configuración WiFi (Captive Portal).

* [x] Cliente MQTT con reconexión automática.

* [x] Almacenamiento persistente (NVS).

* [x] Soporte para actualizaciones OTA (Over-The-Air).

* [ ] Optimización de energía (Deep Sleep).
