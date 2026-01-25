<p align="center">
  <img src="images/white_banner.png" alt="SpecTalkZX Logo" width="640" />
</p>

# SpecTalk ZX

**Cliente IRC para ZX Spectrum con WiFi ESP8266**

🇬🇧 [Read in English](README.md)

![SpecTalk ZX](https://img.shields.io/badge/Plataforma-ZX%20Spectrum-blue)
![Licencia](https://img.shields.io/badge/Licencia-GPLv2-green)
![Versión](https://img.shields.io/badge/Versión-1.0-orange)

## Descripción

SpecTalk ZX es un cliente IRC completo que trae el chat moderno de internet al clásico ZX Spectrum. Usando un módulo WiFi ESP8266 para la conectividad, proporciona una experiencia IRC completa en hardware de 8 bits con pantalla de 64 columnas, soporte para múltiples canales y temas visuales.

## Características

- **Soporte completo del protocolo IRC**: JOIN, PART, QUIT, NICK, PRIVMSG, NOTICE, TOPIC, MODE, KICK, BAN, WHO, WHOIS, LIST y más
- **Pantalla de 64 columnas**: Fuente personalizada de 4 píxeles de ancho para maximizar el texto en pantalla
- **Interfaz multi-ventana**: Hasta 10 ventanas simultáneas de canales/consultas
- **Temas visuales**: 3 temas de colores incluidos (Default, Terminal, Colorful)
- **Integración con NickServ**: Identificación automática con contraseña almacenada
- **Soporte CTCP**: Respuestas a VERSION, PING, TIME, ACTION
- **Contador de usuarios**: Conteo en tiempo real de usuarios con manejo de timeout
- **Búsqueda**: Búsqueda en el historial de mensajes
- **Sistema Keep-Alive**: PING automático para prevenir desconexiones
- **Indicadores de actividad**: Notificación visual para mensajes en canales inactivos

## Requisitos de Hardware

### Opción 1: divTIESUS / divMMC (Recomendado)
- ZX Spectrum 48K/128K/+2/+3
- divTIESUS Maple Edition o divMMC compatible con UART
- Módulo ESP8266/ESP-12 con firmware AT
- UART hardware a 115200 baudios

### Opción 2: AY Bit-Bang
- ZX Spectrum 48K/128K/+2/+3
- Módulo ESP8266/ESP-12 conectado al puerto AY-3-8912
- TX: Puerto A bit 3, RX: Puerto A bit 7
- UART software a 9600 baudios

## Instalación

1. Descarga el archivo TAP apropiado para tu hardware:
   - `SpecTalkZX.tap` - Para UART hardware divTIESUS/divMMC
   - `SpecTalkZX_AY.tap` - Para UART bit-bang AY

2. Carga en tu Spectrum usando tu método preferido (tarjeta SD, cinta, etc.)

3. Configura tu WiFi y ajustes IRC usando los comandos integrados

## Inicio Rápido

```
!wifi SSID,password     Conectar a red WiFi
!server irc.server.net  Establecer servidor IRC
!port 6667              Establecer puerto IRC (por defecto: 6667)
!nick TuNick            Establecer tu nickname
!connect                Conectar al servidor IRC
/join #canal            Unirse a un canal
```

## Referencia de Comandos

### Comandos de Sistema (!)

| Comando | Descripción |
|---------|-------------|
| `!wifi SSID,pass` | Conectar a red WiFi |
| `!server host` | Establecer servidor IRC |
| `!port num` | Establecer puerto IRC |
| `!nick nombre` | Establecer nickname |
| `!pass password` | Establecer contraseña NickServ |
| `!connect` | Conectar al servidor IRC |
| `!disconnect` | Desconectar del servidor |
| `!init` | Reinicializar ESP8266 |
| `!theme [1-3]` | Cambiar tema de colores |
| `!status` | Mostrar estado de conexión |
| `!help` | Mostrar ayuda |
| `!about` | Acerca de SpecTalk |
| `!quit` | Salir a BASIC |

### Comandos IRC (/)

| Comando | Descripción |
|---------|-------------|
| `/join #canal` | Unirse a un canal |
| `/part [mensaje]` | Salir del canal actual |
| `/msg nick texto` | Enviar mensaje privado |
| `/me acción` | Enviar mensaje de acción |
| `/nick nuevonick` | Cambiar nickname |
| `/topic [texto]` | Ver/establecer topic del canal |
| `/kick nick [razón]` | Expulsar usuario del canal |
| `/ban nick` | Banear usuario |
| `/mode +/-flags` | Establecer modos de canal/usuario |
| `/who #canal` | Listar usuarios del canal |
| `/whois nick` | Obtener info de usuario |
| `/list [patrón]` | Listar canales |
| `/names` | Listar usuarios en el canal |
| `/away [mensaje]` | Establecer/quitar estado away |
| `/quote raw` | Enviar comando IRC raw |

### Navegación de Ventanas

| Tecla | Acción |
|-------|--------|
| `Tab` | Ventana siguiente |
| `Shift+Tab` | Ventana anterior |
| `Ctrl+1-9` | Saltar a ventana 1-9 |
| `Ctrl+0` | Saltar a ventana 10 |

## Compilar desde el Código Fuente

### Requisitos
- z88dk (con SDCC)
- Make

### Comandos de Compilación

```bash
# Compilación por defecto (divTIESUS/divMMC)
make

# Compilación AY bit-bang
make ay

# Limpiar artefactos de compilación
make clean
```

## Estructura del Proyecto

```
SpecTalkZX/
├── src/
│   ├── spectalk.c      # Módulo principal, UI, manejo de conexión
│   ├── irc_handlers.c  # Parseo de mensajes del protocolo IRC
│   └── user_cmds.c     # Procesamiento de comandos de usuario
├── asm/
│   ├── spectalk_asm.asm   # Rutinas optimizadas en ensamblador
│   ├── ay_uart.asm        # Driver UART bit-bang AY
│   └── divmmc_uart.asm    # Driver UART hardware divTIESUS
├── include/
│   ├── spectalk.h      # Header común
│   ├── themes.h        # Definiciones de temas de colores
│   └── font64_data.h   # Datos de fuente de 4 píxeles
├── Makefile
├── LICENSE
├── README.md
├── READMEsp.md
└── CHANGELOG.md
```

## Detalles Técnicos

- **Memoria**: Cabe en 48K, usa buffer circular optimizado (2KB) para UART RX
- **Pantalla**: Renderizador personalizado de 64 columnas con caché de atributos
- **Protocolo**: Tokenizador IRC completo con dispatch de comandos por tabla
- **Rendimiento**: Rutas críticas optimizadas en ensamblador Z80

## Licencia

SpecTalk ZX es software libre bajo la **GNU General Public License v2.0**.

Este proyecto incluye código derivado de:
- **BitchZX** - Cliente IRC (GPLv2)
- **Driver UART AY/ZXuno** por Nihirash

Consulta el archivo [LICENSE](LICENSE) para el texto completo de la licencia.

## Autor

**M. Ignacio Monge Garcia** - 2026

## Agradecimientos

- El proyecto BitchZX por la inspiración en la implementación del protocolo IRC
- Nihirash por el código del driver UART AY/ZXuno
- El equipo de z88dk por el excelente compilador cruzado
- La comunidad de retrocomputación del ZX Spectrum

---

*¡Conecta tu Spectrum al mundo!*
