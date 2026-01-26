![SpecTalk Banner](images/white_banner.png)

# SpecTalk ZX

**Cliente IRC para ZX Spectrum con WiFi ESP8266**

🇬🇧 [Read in English](README.md)

![Plataforma](https://img.shields.io/badge/Plataforma-ZX%20Spectrum-blue)
![Licencia](https://img.shields.io/badge/Licencia-GPLv2-green)
![Versión](https://img.shields.io/badge/Versión-1.1-orange)

## Descripción

SpecTalk ZX es un cliente IRC completo para ZX Spectrum. Usando un módulo WiFi ESP8266 para la conectividad, proporciona una experiencia IRC completa en hardware de 8 bits con pantalla de 64 columnas y soporte para hasta 10 ventanas simultáneas de canales/consultas.

[![SpecTalkZX](images/snap1.png)](images/snap1.png)

## Características

- **Pantalla de 64 columnas** con fuente personalizada de 4 píxeles
- **Interfaz multi-ventana**: Hasta 10 canales/queries simultáneos
- **3 temas de colores**: Default, Terminal, Colorful
- **Integración con NickServ**: Identificación automática
- **Soporte CTCP**: VERSION, PING, TIME, ACTION
- **Contador de usuarios** con actualizaciones en tiempo real
- **Función de búsqueda**: Encuentra canales o usuarios por patrón
- **Sistema Keep-Alive**: PING automático para evitar timeout
- **Indicadores de actividad**: Notificación visual de mensajes no leídos

[![Tema 1](images/theme1.png)](images/theme1.png) [![Tema 2](images/theme2.png)](images/theme2.png) [![Tema 3](images/theme3.png)](images/theme3.png)

## Requisitos de Hardware

### Opción 1: divTIESUS / divMMC (Recomendado)
- ZX Spectrum 48K/128K/+2/+3
- divTIESUS o divMMC con UART hardware
- Módulo ESP8266/ESP-12 con firmware AT
- UART hardware a 115200 baudios

### Opción 2: AY Bit-Bang
- ZX Spectrum 48K/128K/+2/+3
- ESP8266/ESP-12 conectado al puerto AY-3-8912
- UART software a 9600 baudios

## Instalación

1. Descarga el archivo TAP para tu hardware
2. Carga en tu Spectrum (tarjeta SD, cinta, etc.)
3. Configura el WiFi con [NetManZX](https://github.com/IgnacioMonge/NetManZX) o similar

## Inicio Rápido

```
/nick TuNick            Establece tu nickname
/server irc.libera.chat Conecta al servidor
/join #canal            Únete a un canal
```

Escribe `!help` para ver la ayuda integrada.

## Referencia de Comandos

### Comandos de Sistema (!)

| Comando | Descripción |
|---------|-------------|
| `!help` o `!h` | Muestra páginas de ayuda (cualquier tecla cambia página, EDIT para salir) |
| `!status` o `!s` | Muestra estado de conexión, nick, servidor y canales abiertos |
| `!init` o `!i` | Reinicializa el módulo ESP8266 |
| `!theme N` | Cambia el tema de colores (1-3) |
| `!about` | Muestra versión y créditos |

### Comandos IRC (/)

#### Conexión
| Comando | Descripción |
|---------|-------------|
| `/nick nombre` | Establece o cambia el nickname |
| `/pass contraseña` | Establece contraseña de NickServ (se envía al conectar) |
| `/server host[:puerto]` | Conecta al servidor IRC (puerto por defecto: 6667) |
| `/quit [mensaje]` | Desconecta del servidor |

#### Canales
| Comando | Alias | Descripción |
|---------|-------|-------------|
| `/join #canal` | `/j` | Unirse a un canal |
| `/part [mensaje]` | `/p` | Salir del canal actual |
| `/topic [texto]` | | Ver o establecer el topic del canal |
| `/names` | | Listar usuarios en el canal actual |
| `/kick nick [razón]` | `/k` | Expulsar usuario del canal (requiere op) |

#### Mensajes
| Comando | Alias | Descripción |
|---------|-------|-------------|
| `/msg nick texto` | `/m` | Enviar mensaje privado |
| `/query nick` | `/q` | Abrir ventana de query para chat privado |
| `/me acción` | | Enviar mensaje de acción (*TuNick hace algo*) |
| `nick: texto` | | Sintaxis rápida de PM (desde ventana de canal) |

#### Ventanas
| Comando | Descripción |
|---------|-------------|
| `/0` | Cambiar a ventana Server |
| `/1` a `/9` | Cambiar a ventana de canal/query |
| `/w` o `/channels` | Listar todas las ventanas abiertas |
| `/close` | Cerrar ventana de query actual (o `/part` si es canal) |

#### Búsqueda e Info
| Comando | Alias | Descripción |
|---------|-------|-------------|
| `/search patrón` | | Buscar canales (`#pat`) o usuarios (`nick`) |
| `/list [patrón]` | `/ls` | Listar canales que coincidan con el patrón |
| `/who #canal` | | Listar usuarios en un canal |
| `/whois nick` | `/wi` | Obtener información de un usuario |

#### Otros
| Comando | Descripción |
|---------|-------------|
| `/away [mensaje]` | Establecer o quitar estado away |
| `/ignore nick` | Activar/desactivar ignorar a un usuario |
| `/raw comando` | Enviar comando IRC raw |

## Teclado

| Tecla | Función |
|-------|---------|
| **ENTER** | Enviar mensaje o ejecutar comando |
| **EDIT** (CAPS+1) | Cancelar operación actual |
| **↑ / ↓** | Navegar historial de comandos |
| **← / →** | Mover cursor en línea de entrada |
| **DELETE** (CAPS+0) | Borrar carácter |

## Compilar desde el Código Fuente

### Requisitos
- z88dk con SDCC
- Make

### Compilación

```bash
make              # Compilación divTIESUS/divMMC
make ay           # Compilación AY bit-bang  
make clean        # Limpiar artefactos
```

## Estructura del Proyecto

```
SpecTalkZX/
├── src/
│   ├── spectalk.c       # Módulo principal, UI, conexión
│   ├── irc_handlers.c   # Parseo del protocolo IRC
│   └── user_cmds.c      # Procesamiento de comandos
├── asm/
│   ├── spectalk_asm.asm # Rutinas optimizadas en ensamblador
│   ├── ay_uart.asm      # Driver UART AY bit-bang
│   └── divmmc_uart.asm  # Driver UART hardware
├── include/
│   ├── spectalk.h       # Header común
│   ├── themes.h         # Temas de colores
│   └── font64_data.h    # Datos de fuente de 4 píxeles
├── Makefile
├── CHANGELOG.md
└── LICENSE
```

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

---

*¡Conecta tu Spectrum al mundo!*
