<p align="center">
  <a href="images/white_banner.png"><img src="images/white_banner.png" width="600" alt="SpecTalk Banner"></a>
</p>

# SpecTalk ZX

**Cliente IRC para ZX Spectrum con WiFi ESP8266**

:gb: [Read in English](README.md)

![Plataforma](https://img.shields.io/badge/Plataforma-ZX%20Spectrum-blue)
![Licencia](https://img.shields.io/badge/Licencia-GPLv2-green)
![Versión](https://img.shields.io/badge/Versión-1.3.7-orange)

---

## Descripción

SpecTalk ZX es un cliente IRC completo para ZX Spectrum que trae la funcionalidad de chat moderno al hardware clásico de 8 bits. Utilizando un módulo WiFi ESP8266 para la conectividad, proporciona una experiencia IRC completa con una pantalla personalizada de 64 columnas y soporte para hasta 10 ventanas simultáneas de canales y mensajes privados.

---

## Características

### Pantalla e Interfaz
- **Pantalla de 64 columnas** con fuente personalizada de 4 píxeles para máxima densidad de texto
- **Interfaz multi-ventana** soportando hasta 10 canales/consultas simultáneas
- **3 temas de color** con badges exclusivos: Default (rainbow dither), Terminal (`<<<` parpadeante), Commander (`_ [] X`)
- **Banner double-height** con efecto BRIGHT split (estilo NetManZX)
- **Indicadores de actividad**: Marcadores visuales para ventanas con mensajes sin leer, badge parpadeante en Theme 2
- **Resaltado de menciones**: Mensajes con tu nick mostrados en BRIGHT
- **Barra de cambio de canales**: Pulsa EDIT para una barra visual con indicadores de no leído/mención en tiempo real; DELETE cierra el canal seleccionado
- **Indicador de conexión**: LED de tres estados (🔴 Sin WiFi → 🟡 WiFi OK → 🟢 Conectado)
- **Reloj en tiempo real** sincronizado vía SNTP
- **Timestamps opcionales** en todos los mensajes

### Nuevas funcionalidades en v1.3.7

- **Sistema de overlays**: Ayuda, About, Config, Status, What's New cargados desde SPECTALK.OVL bajo demanda
- **Mini fuente Ikkle-4**: Fuente de 4px para notificaciones en fila 20
- **Notificaciones inteligentes**: Alertas de PM, menciones, amigos online. Configurable con `!notif on|off`
- **Coloreado de nicks**: Color por nick basado en hash, detecta temas monocromo. `!nickcolor on|off`
- **Navegación por palabras**: SS+IZQUIERDA/DERECHA, SS+BORRAR para borrar palabra
- **Auto-repetición de teclas**: Todas las teclas repiten al mantenerlas pulsadas
- **Respuesta rápida**: ENTER en notificación PM abre ventana del remitente; BREAK descarta
- **Globo rotatorio** en la pantalla About
- **Pantalla What's New** (pulsar N en About)
- **Pantalla Status mejorada**: Red, Latencia, Uptime, lista de canales en dos columnas
- **esxDOS obligatorio**: Parada fatal si no hay divMMC o falta el DAT
- **Beep diferenciado**: Mención suena más grave que el click de tecla

### Protocolo IRC
- **Compatibilidad IRC completa**: JOIN, PART, QUIT, NICK, PRIVMSG, NOTICE, TOPIC, MODE, KICK, WHO, WHOIS, LIST, NAMES
- **Soporte CTCP**: VERSION, PING, TIME, ACTION
- **Integración con NickServ**: Identificación rápida con `/id` o automática con `nickpass=`
- **Sistema Away**: `/away` manual y `/autoaway` automático con temporizador de inactividad
- **Ignorar usuarios**: Bloquea mensajes de usuarios específicos con `/ignore`
- **Búsqueda de canales**: Encuentra canales o usuarios por patrón
- **Soporte UTF-8**: Caracteres internacionales convertidos a ASCII legible

### Conectividad
- **Auto-conexión**: Conecta automáticamente al servidor configurado al iniciar
- **Auto-identificación**: Identificación automática con NickServ tras conectar
- **Sistema de amigos**: Monitoriza hasta 5 amigos con notificaciones de estado online
- **Manejo de colisión de nick**: Nick alternativo automático si el primario está en uso
- **Sistema Keep-alive**: PING automático para detectar desconexiones silenciosas
- **Latencia de ping**: Medición del tiempo de respuesta del servidor

### Rendimiento y Bajo Nivel
- **Hijacking de RAM del sistema**: Printer Buffer, workspace CHANS y zona UDG reutilizados para variables (+602 bytes BSS liberados)
- **Compresión BPE de strings**: 155 strings de pantalla comprimidas con 81 tokens (~947 bytes ahorrados)
- **35 reglas peephole personalizadas**: Factorización en subrutinas en múltiples call sites
- **Sistema de overlays**: 13 funciones ASM optimizadas cargadas bajo demanda desde SPECTALK.OVL
- **Arquitectura Unity Build**: Cliente completo compilado como unidad única para máxima optimización
- **Ring Buffer**: Buffer de 2KB para recepción de datos fiable a alta velocidad
- **Optimizado en ensamblador**: Rutas críticas de renderizado en Z80 assembly, renderizado inline de espacios
- **UART hardware**: 115200 baud vía divMMC/divTIESUS
- **esxDOS obligatorio**: Parada fatal con mensajes ROM si no se detecta divMMC o falta el fichero DAT

Sesión de chat en vivo en un canal IRC:

<p align="center">
  <a href="images/chat_default.png"><img src="images/chat_default.png" width="600" alt="SpecTalkZX - Chat IRC en vivo"></a>
</p>

Notificación de mención con resaltado BRIGHT:

<p align="center">
  <a href="images/chat_mention.png"><img src="images/chat_mention.png" width="600" alt="Mención de nick resaltada"></a>
</p>

---

## Requisitos de Hardware

| Componente | Especificación |
|------------|----------------|
| **Ordenador** | ZX Spectrum 48K, 128K, +2, +2A, +3, o compatible |
| **Módulo WiFi** | ESP8266 (ESP-01 o similar) con firmware AT |
| **Interfaz** | divMMC o divTIESUS (UART hardware) |
| **Velocidad** | **115200** bps |
| **Tarjeta SD** | Necesaria (esxDOS) para fuentes, overlays y configuración |

> **Importante**: Configura tu ESP8266 a 115200 baud antes de usar.

---

## Instalación

1. Descarga `spectalk_divmmc.zip` desde la página de [Releases](https://github.com/IgnacioMonge/SpecTalkZX/releases)
2. Extrae y copia todos los ficheros a tu tarjeta SD:
   - `SpecTalkZX.tap` — el programa
   - `SPECTALK.DAT` — fuente, temas y datos comprimidos (debe estar en el mismo directorio que el TAP)
   - `SPECTALK.OVL` — overlay con pantallas de ayuda, about, config y status
3. Cárgalo en tu Spectrum mediante tarjeta SD, cinta u otro método
4. Configura las credenciales WiFi usando [NetManZX](https://github.com/IgnacioMonge/NetManZX) o herramienta similar

> **Importante**: `SPECTALK.DAT` y `SPECTALK.OVL` son necesarios. Sin el DAT no se cargará la fuente de 64 columnas. Sin el OVL no funcionarán las pantallas de ayuda, about, config ni status.

---

## Inicio Rápido

1. **Inicializar**: Al arrancar, espera a ver `WiFi:OK` en la barra de estado (el indicador pasa a amarillo, luego verde al conectar)

2. **Establecer nickname**:
   ```
   /nick TuNickname
   ```

3. **Conectar al servidor**:
   ```
   /server irc.libera.chat 6667
   ```

4. **Unirse a un canal**:
   ```
   /join #spectrum
   ```

5. **A chatear!** Escribe tu mensaje y pulsa ENTER

Los tres temas de color disponibles:

<p align="center">
  <a href="images/chat_default.png"><img src="images/chat_default.png" width="250" alt="Tema 1 - Default"></a>
  &nbsp;&nbsp;
  <a href="images/theme_terminal.png"><img src="images/theme_terminal.png" width="250" alt="Tema 2 - Terminal"></a>
  &nbsp;&nbsp;
  <a href="images/theme_commander.png"><img src="images/theme_commander.png" width="250" alt="Tema 3 - Commander"></a>
  <br>
  <em>Default &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp; Terminal &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp; Commander</em>
</p>

Notificación en tema Terminal:

<p align="center">
  <a href="images/theme_terminal_notif.png"><img src="images/theme_terminal_notif.png" width="600" alt="Notificación en tema Terminal"></a>
</p>

---

## Referencia de Comandos

### Controles de Teclado

| Tecla | Acción |
|-------|--------|
| **ENTER** | Enviar mensaje / ejecutar comando |
| **EDIT** (Caps+1) | Abrir/cerrar barra de canales |
| **DELETE** (Caps+0) | Borrar carácter |
| **← / →** | Mover cursor |
| **↑ / ↓** | Historial de comandos |
| **SS+← / SS+→** | Navegación por palabras |
| **SS+DELETE** | Borrar palabra |
| **BREAK** (Caps+Space) | Salir de overlays / cerrar notificación |
| **ENTER** (en notificación PM) | Abrir ventana del remitente |

### Comandos de Sistema (!)

Comandos locales que no requieren conexión al servidor.

| Comando | Alias | Descripción |
|---------|-------|-------------|
| `!help` | `!h` | Mostrar pantallas de ayuda |
| `!status` | `!s` | Información de conexión |
| `!init` | `!i` | Reinicializar módulo ESP8266 |
| `!config` | `!cfg` | Mostrar configuración |
| `!theme N` | — | Cambiar tema de color (1-3) |
| `!about` | — | Versión y créditos |
| `!beep` | — | Activar/desactivar sonido de mención |
| `!notif` | `!nf` | Activar/desactivar notificaciones |
| `!changelog` | — | Mostrar novedades (What's New) |
| `!autoaway` | `!aa` | Auto-away tras N minutos |
| `!traffic` | — | Mostrar/ocultar mensajes JOIN/QUIT |
| `!timestamps` | `!ts` | Ciclar modo de timestamps |
| `!clear` | `!cls` | Limpiar zona de chat |
| `!save` | `!sv` | Guardar configuración en SD |
| `!autoconnect` | `!ac` | Activar/desactivar auto-conexión |
| `!tz [±N]` | — | Ver/configurar zona horaria |
| `!friend [nick]` | — | Gestionar lista de amigos |
| `!nickcolor` | `!nc` | Activar/desactivar coloreado de nicks |
| `!click` | — | Activar/desactivar sonido de tecla |

### Comandos IRC (/)

#### Conexión

| Comando | Alias | Descripción |
|---------|-------|-------------|
| `/server host [puerto]` | `/connect` | Conectar al servidor |
| `/nick [nombre]` | — | Establecer nick |
| `/pass [contraseña]` | — | Establecer contraseña del servidor |
| `/id [contraseña]` | — | Identificarse con NickServ |
| `/quit [mensaje]` | — | Desconectar |

#### Canales

| Comando | Alias | Descripción |
|---------|-------|-------------|
| `/join #canal` | `/j` | Unirse a un canal |
| `/part [mensaje]` | `/p` | Salir del canal |
| `/topic [texto]` | — | Ver/establecer topic |
| `/names` | — | Listar usuarios del canal |
| `/kick nick [razón]` | `/k` | Expulsar usuario |

#### Mensajes

| Comando | Alias | Descripción |
|---------|-------|-------------|
| `/msg nick texto` | `/m` | Enviar mensaje privado |
| `/query nick` | `/q` | Abrir ventana de PM |
| `/me acción` | — | Enviar acción |
| `/close` | — | Cerrar ventana actual |

#### Ventanas

| Comando | Alias | Descripción |
|---------|-------|-------------|
| `/0` ... `/9` | — | Cambiar ventana por número |
| `/channels` | `/w` | Listar ventanas abiertas |

#### Herramientas

| Comando | Alias | Descripción |
|---------|-------|-------------|
| `/search patrón` | — | Buscar canales/usuarios |
| `/list patrón` | `/ls` | Listar canales |
| `/who patrón` | — | Buscar usuarios |
| `/whois nick` | `/wi` | Información de usuario |
| `/ignore [nick]` | — | Activar/desactivar ignore |
| `/raw comando` | — | Enviar comando IRC crudo |

#### Estado Away

| Comando | Alias | Descripción |
|---------|-------|-------------|
| `/away [mensaje]` | — | Establecer/quitar away |
| `/autoaway N` | `/aa` | Auto-away tras N minutos inactivo (1-60, 0=off) |

> **Comportamiento de auto-away**: Cuando está activado, te pone automáticamente away tras N minutos de inactividad. Enviar cualquier mensaje quita el auto-away automáticamente. El `/away` manual debe quitarse manualmente con `/away`.

---

## Gestión de Ventanas

SpecTalk soporta hasta 10 ventanas simultáneas:

- **Ventana 0**: Mensajes del servidor (siempre presente)
- **Ventanas 1-9**: Canales y consultas privadas

### Navegación
- Usa `/0` a `/9` para cambiar de ventana
- Usa `/w` o `/channels` para ver todas las ventanas abiertas
- Pulsa **EDIT** para abrir la **barra de cambio de canales** — una barra visual mostrando todos los canales activos. Usa IZQUIERDA/DERECHA para navegar, ENTER para seleccionar, o teclas numéricas para acceso directo. Se oculta automáticamente tras 20 segundos.
- El indicador de actividad muestra ventanas con mensajes sin leer
- El indicador de mención muestra ventanas donde te mencionaron (resaltado en color)

### Mensajes Privados
- Los PMs entrantes crean automáticamente una ventana de consulta
- Usa `/query nick` para abrir manualmente un chat privado
- Usa `/close` para cerrar la ventana de consulta actual
- En una notificación de PM, pulsa **ENTER** para abrir directamente la ventana del remitente, o **BREAK** para descartarla

---

## Archivo de Configuración

SpecTalk puede cargar ajustes desde un archivo de configuración en tu tarjeta SD. El archivo debe llamarse `SPECTALK.CFG` y estar en el directorio `SYS/CONFIG/`. Usa `!save` para escribir tu configuración actual en este archivo.

### Formato del Archivo

Archivo de texto plano con un ajuste por línea en formato `clave=valor`:

```
nick=MiNickname
server=irc.libera.chat
port=6667
nickpass=micontraseñanickserv
nickserv=NickServ
autoconnect=1
theme=1
timestamps=1
autoaway=15
beep=1
traffic=1
notif=1
nickcolor=0
click=1
tz=1
friends=Amigo1,Amigo2,Amigo3
ignores=Troll1,Troll2
```

### Ajustes Disponibles

| Ajuste | Descripción | Valores | Por defecto |
|--------|-------------|---------|-------------|
| `nick` | Nickname por defecto | Cualquier nick IRC válido | (ninguno) |
| `server` | Servidor IRC | Hostname o IP | (ninguno) |
| `port` | Puerto del servidor | 1-65535 | 6667 |
| `pass` | Contraseña del servidor | Cualquier string | (ninguno) |
| `nickpass` | Contraseña NickServ | Cualquier string | (ninguno) |
| `nickserv` | Nick del servicio NickServ | nick | (auto) |
| `autoconnect` | Conectar al iniciar | 0 o 1 | 0 |
| `theme` | Tema de color | 1, 2, o 3 | 1 |
| `timestamps` | Mostrar timestamps | 0, 1, o 2 (smart) | 1 |
| `autoaway` | Minutos auto-away | 0-60 (0=off) | 0 |
| `beep` | Sonido en mención | 0 o 1 | 1 |
| `traffic` | Mostrar mensajes QUIT/JOIN | 0 o 1 | 1 |
| `notif` | Mostrar notificaciones | 0 o 1 | 1 |
| `nickcolor` | Coloreado de nicks | 0 o 1 | 0 |
| `click` | Sonido al pulsar tecla | 0 o 1 | 1 |
| `tz` | Desplazamiento horario | -12 a +12 | 1 |
| `friends` | Nicks de amigos a monitorizar (separados por coma, máx 5) | nick1,nick2,... | (ninguno) |
| `ignores` | Nicks ignorados (separados por coma, máx 5) | nick1,nick2,... | (ninguno) |

### Ver Configuración Actual

Usa `!config` o `!cfg` para mostrar todos los valores de configuración actuales. Usa `!save` para guardar los cambios en la tarjeta SD.

Pantalla mostrando la configuración actual del cliente:

<p align="center">
  <a href="images/config.png"><img src="images/config.png" width="600" alt="Pantalla de configuración"></a>
</p>

---

## Pantallas de Overlay

Las pantallas informativas se cargan bajo demanda desde el fichero `SPECTALK.OVL`:

**About** — Versión, créditos y globo rotatorio animado:

<p align="center">
  <a href="images/about_globe.png"><img src="images/about_globe.png" width="300" alt="About - Globo"></a>
  &nbsp;&nbsp;
  <a href="images/about_commander.png"><img src="images/about_commander.png" width="300" alt="About - Commander"></a>
</p>

**What's New** — Novedades de la versión actual (pulsar N en About):

<p align="center">
  <a href="images/whatsnew.png"><img src="images/whatsnew.png" width="600" alt="Pantalla What's New"></a>
</p>

**Help** — Referencia de comandos en pantalla:

<p align="center">
  <a href="images/help.png"><img src="images/help.png" width="600" alt="Pantalla de ayuda"></a>
</p>

---

## Barra de Estado

La barra de estado muestra la información de conexión en tiempo real:

```
[nick(+modos)] [idx/total:canal(@red)(+modos_canal)] [usuarios]    [HH:MM] [LED]
```

| Elemento | Descripción |
|----------|-------------|
| **nick(+modos)** | Tu nickname actual y tus modos de usuario (si los hay) |
| **idx/total:canal** | Índice de ventana, total de ventanas abiertas y nombre del canal |
| **(@red)(+modos_canal)** | Red IRC y modos del canal (si los hay) |
| **usuarios** | Número de usuarios en el canal actual |
| **HH:MM** | Hora actual (sincronizada por SNTP) |
| **LED** | Indicador de conexión: 🔴 Sin WiFi, 🟡 WiFi OK, 🟢 Conectado |

Cuando estás away, el indicador cambia de un círculo sólido a un semicírculo.

---

## Compilación desde Código Fuente

### Requisitos
- **z88dk** con soporte SDCC
- **GNU Make**

### Comandos de Compilación

```bash
# Compilación estándar
make

# Release build con optimización máxima
make release

# Limpiar artefactos de compilación
make clean
```

El proyecto usa estrategia **Unity Build**: todos los fuentes C se compilan como una única unidad (`main_build.c`) permitiendo optimización agresiva entre funciones.

---

## Solución de Problemas

| Problema | Solución |
|----------|----------|
| El indicador permanece rojo | Verifica el cableado del ESP8266 y la configuración de velocidad |
| Indicador amarillo pero no conecta | Verifica las credenciales WiFi con NetManZX |
| "Connection timeout" tras inactividad | Comportamiento normal - keep-alive detectó conexión muerta |
| Los mensajes de un usuario no paran | Usa `/ignore nick` para bloquearlo |
| No puedo identificarme con NickServ | Usa `/id contraseña` o configura `nickpass=` en el archivo de configuración |
| Olvidé la configuración actual | Usa `!config` para ver todos los valores de configuración |
| Demasiados mensajes de quit | Usa `!traffic` para desactivarlos |
| No hay sonido en menciones | Usa `!beep` para activar el sonido |
| Nick en uso al conectar | SpecTalk añade `_` automáticamente - usa `/nick` para cambiar después |
| Los caracteres acentuados se ven mal | UTF-8 se convierte automáticamente a equivalentes ASCII |
| Falta SPECTALK.OVL | Copia el fichero OVL junto al TAP y DAT en la tarjeta SD |
| Error fatal al arrancar | Necesitas divMMC con SPECTALK.DAT en la tarjeta SD |

---

## Licencia

SpecTalk ZX es software libre publicado bajo la **GNU General Public License v2.0**.

Incluye código derivado de:
- **BitchZX** — Cliente IRC (GPLv2)
- **Driver UART** por Nihirash

---

## Autor

**M. Ignacio Monge García** — 2025-2026

---

## Agradecimientos

- Proyecto BitchZX por la base del protocolo IRC
- Nihirash por el código del driver UART
- Jack Oatley por la mini fuente Ikkle-4 (dominio público)
- Equipo z88dk por el toolchain del compilador cruzado
- Comunidad de retrocomputación del ZX Spectrum

---

*Conectando el ZX Spectrum a IRC desde 2025*