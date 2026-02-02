![SpecTalk Banner](images/white_banner.png)

# SpecTalk ZX

**Cliente IRC para ZX Spectrum con WiFi ESP8266**

🇬🇧 [Read in English](README.md)

![Plataforma](https://img.shields.io/badge/Plataforma-ZX%20Spectrum-blue)
![Licencia](https://img.shields.io/badge/Licencia-GPLv2-green)
![Versión](https://img.shields.io/badge/Versión-1.2-orange)

## Descripción General

SpecTalk ZX es un cliente IRC con todas las funciones para el ZX Spectrum. Utilizando un módulo WiFi ESP8266 para la conectividad, proporciona una experiencia IRC completa en hardware de 8 bits con una pantalla de 64 columnas y soporte para hasta 10 ventanas simultáneas de canales/consultas.

## Características

- **Pantalla de 64 columnas** utilizando una fuente personalizada de 4 píxeles de ancho
- **Interfaz multi-ventana**: Hasta 10 canales/consultas simultáneas
- **3 temas de color**: Default, Terminal, Colorful
- **Integración con NickServ**: Identificación automática
- **Soporte CTCP**: VERSION, PING, TIME, ACTION
- **Conteo de usuarios del canal** con actualizaciones en tiempo real
- **Funcionalidad de búsqueda**: Encuentra canales o usuarios por patrón
- **Sistema Keep-alive**: PING automático para prevenir tiempos de espera
- **Indicadores de actividad**: Notificación visual para mensajes no leídos
- **Manejo Inteligente del Protocolo** *(Nuevo)*:
  - El analizador numérico genérico permite ver la salida del comando `/raw` (ej., `/raw info`, `/raw stats`) y errores personalizados del servidor.
  - Filtra el ruido de conexión (MOTD, modos del servidor) para una barra de estado más limpia.
- **Arquitectura Unity Build** *(Nuevo)*:
  - Todo el cliente compilado como una sola unidad para maximizar la velocidad y ajustarse a la RAM de 48K.
- **UART de Alto Rendimiento**:
  - Ring Buffer (2KB) para una recepción de datos fiable.
  - Drivers optimizados para divMMC (115200 baudios) y AY (9600 baudios).

## Requisitos de Hardware

- **ZX Spectrum** 48K, 128K, +2, o +3.
- **Módulo WiFi ESP8266** configurado a la velocidad de baudios correcta.

| Interfaz | Driver Usado | Velocidad Requerida |
|-----------|-------------|--------------------|
| **divMMC / divTIESUS** | UART Hardware / Bit-bang Rápido | **115200** bps |
| **ZX-Uno / Interfaz AY** | Bit-banging AY-3-8912 | **9600** bps |

## Instalación

1.  **Configuración WiFi**: Asegúrate de que tu ESP8266 esté conectado a tu red WiFi local (usando comandos AT estándar) antes de cargar el programa.
2.  **Carga**: Carga el archivo `SpecTalkZX.tap` en tu Spectrum o emulador.
    - Si usas divMMC (ESXDOS), puedes ejecutarlo directamente desde el navegador.
    - Si usas cargador de cinta: `LOAD ""`

## Inicio Rápido

1.  **Conectar**: Al inicio, el cliente intentará inicializar el ESP8266.
    - Espera a ver `WiFi:OK` en la barra de estado.
2.  **Servidor**: Conéctate a un servidor IRC (el predeterminado está configurado en el código fuente, o usa el comando):
    - `/server irc.libera.chat 6667`
3.  **Identificarse**: Establece tu apodo:
    - `/nick MiNickRetro`
4.  **Entrar**: Entra a un canal:
    - `/join #spectrum`

## Referencia de Comandos

### 1. Controles y Atajos de Teclado

| Combinación de Teclas | Acción | Descripción |
|-----------|--------|-------------|
| **EXTEND** (CS+SS) | Alternar Modo | Cambia entre **Modo Comando** (Escribir) y **Modo Vista** (Desplazarse). |
| **TRUE VIDEO** (CS+3)| Siguiente Ventana | Cambia al siguiente canal o consulta activa (Pestaña). |
| **INV VIDEO** (CS+4) | Ventana Anterior | Cambia al anterior canal o consulta activa. |
| **ENTER** | Enviar / Actuar | Envía el mensaje o ejecuta el comando. |
| **EDIT** (CAPS+1) | Cancelar / Limpiar | Limpia la línea de entrada o cancela búsquedas activas. |
| **DELETE** (CAPS+0) | Retroceso | Borra el carácter a la izquierda. |
| **↑ / ↓** | Historial | Navega por el historial de comandos. |
| **← / →** | Cursor | Mueve el cursor dentro de la línea. |

### 2. Comandos de Barra

Todos los comandos comienzan con `/`.

| Categoría | Comando | Descripción |
|----------|---------|-------------|
| **Sesión** | `/nick [nombre]` | Cambiar apodo. |
| | `/quit [msg]` | Desconectar y salir. |
| | `/raw [cmd]` | Enviar comando IRC crudo (ej., `/raw VERSION`). |
| | `/quote [cmd]` | Alias para `/raw`. |
| **Canal** | `/join [chan]` | Unirse a un canal. |
| | `/part [chan]` | Salir de un canal. |
| | `/topic [texto]` | Ver o establecer el tema del canal. |
| | `/names` | Listar usuarios en el canal actual. |
| | `/kick [usuario]` | Expulsar a un usuario (Solo Ops). |
| | `/mode [args]` | Establecer modos de canal/usuario. |
| **Mensajes** | `/msg [usuario] [txt]` | Enviar mensaje privado. |
| | `/query [usuario]` | Abrir una ventana de chat privado. |
| | `/me [acción]` | Enviar acción (ej., `* Usuario saluda`). |
| | `/notice [tgt] [txt]`| Enviar aviso (notice). |
| **Herramientas** | `/windows` | Listar todas las ventanas abiertas e IDs. |
| | `/clear` | Limpiar texto en la ventana actual. |
| | `/search [str]` | Buscar canales/usuarios activos. |
| | `/list` | Descargar lista de canales (usar con precaución). |
| | `/1` ... `/9` | Saltar a ID de ventana. (/0 Servidor)|

## Compilación desde el Código Fuente

Este proyecto utiliza una estrategia de **Unity Build** (Compilación Unificada) para optimizar para el objetivo Z80.

### Requisitos
- **z88dk** (con soporte SDCC).
- **Make**.

### Comandos de Compilación

El `Makefile` soporta diferentes objetivos para diferentes backends de hardware:

bash
# 1. Compilación Estándar (divMMC / divTIESUS - 115200 baudios)
make

# 2. Compilación Legacy (Interfaz AY - 9600 baudios)
make ay

# 3. Limpiar artefactos
make clean


## Licencia

SpecTalk ZX es software libre bajo **GNU General Public License v2.0**.

Incluye código derivado de:
- **BitchZX** - Cliente IRC (GPLv2)
- **Driver UART AY/ZXuno** por Nihirash

## Autor

**M. Ignacio Monge Garcia** - 2026

## Agradecimientos

- Proyecto BitchZX por la inspiración en el protocolo IRC
- Nihirash por el código del driver UART AY
- Equipo de z88dk por el compilador cruzado
- Comunidad de retrocomputación del ZX Spectrum
