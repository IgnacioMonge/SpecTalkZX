# SpecTalkZX

<p align="center">
  <img src="images/spectalkzx-banner.png" alt="SpecTalkZX" width="90%">
</p>

<p align="center"><strong>Cliente IRC para ZX Spectrum, Spectrum Next y el cartucho Spectranext</strong></p>

<p align="center">🇬🇧 <a href="README.md">Read in English</a></p>

<p align="center">
  <strong>Instalación:</strong>
  <a href="#zx-classic--divmmc">ZX Classic / divMMC</a> ·
  <a href="#spectrum-next-nativo">Spectrum Next nativo</a> ·
  <a href="#cartucho-spectranext">Cartucho Spectranext</a>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/Plataforma-ZX%20Spectrum%20%7C%20Next%20%7C%20Spectranext-blue" alt="Plataforma: ZX Spectrum, Next y Spectranext">
  <img src="https://img.shields.io/badge/Licencia-GPLv2-green" alt="Licencia: GPLv2">
  <img src="https://img.shields.io/badge/Versión-1.4.0-orange" alt="Versión: 1.4.0">
</p>

Versión actual:
[SpecTalkZX 1.4.0 Proteus](https://github.com/IgnacioMonge/SpecTalkZX/releases/tag/v1.4.0).

La versión 1.4.0 añade soporte nativo para Spectrum Next y un nuevo sistema de
paginación para el cartucho Spectranext. La edición Classic ZX/divMMC mantiene
la misma interfaz, los mismos comandos y el mismo formato de configuración.

---

## Índice

- [Novedades principales de 1.4.0](#novedades-principales-de-140)
- [Requisitos](#requisitos)
- [Instalación](#instalación)
- [Inicio rápido](#inicio-rápido)
- [Capturas](#capturas)
- [Interfaz](#interfaz)
- [Controles de teclado](#controles-de-teclado)
- [Comportamiento IRC](#comportamiento-irc)
- [Comandos](#comandos)
- [Configuración](#configuración)
- [Límites de Spectranext](#límites-de-spectranext)
- [Compilación](#compilación)
- [Solución de problemas](#solución-de-problemas)
- [Licencia](#licencia)
- [Autor](#autor)

---

## Novedades principales de 1.4.0

- **Soporte nativo para Spectrum Next**, con la misma interfaz y los mismos
  comandos IRC de SpecTalkZX.
- **Nuevo sistema de paginación Spectranext**, que separa los datos IRC
  entrantes de las pantallas y los comandos secundarios.
- **Arranque más resistente en Next nativo**, con recuperación de un ESP que no
  responde y cancelación mediante BREAK.

Consulta [CHANGELOG.md](CHANGELOG.md) para el historial completo de cambios
visibles y las notas de compatibilidad.

---

## Requisitos

| Objetivo | Ordenador | Almacenamiento | Red |
|---|---|---|---|
| Classic | ZX Spectrum 48K, 128K, +2, +2A, +3 o compatible | SD con divMMC/esxDOS | UART soportada a 115200 baudios con ESP8266 o puente ESP-AT compatible |
| Next nativo | ZX Spectrum Next con NextZXOS/esxDOS | Tarjeta SD para el NEX y `/SYS/CONFIG` o `/SYS` escribible | ESP interno configurado |
| Spectranext | Modelo de ZX Spectrum compatible con el firmware del cartucho Spectranext | XFS local del cartucho | Wi-Fi nativo y sockets ROM del cartucho |

Classic utiliza <code>SpecTalkZX.tap</code>, <code>SPECTALK.OVL</code> y
<code>SPECTALK.DAT</code>. Conserva los tres ficheros de la misma versión.
Spectranext instala su propio conjunto TAP, OVL y DAT en el almacenamiento del
cartucho. Next nativo utiliza un único <code>SPECTALK.NEX</code> autónomo.

---

## Instalación

### ZX Classic / divMMC

1. Descarga `spectalk_divmmc_v1.4.0.zip` desde la
   [versión 1.4.0](https://github.com/IgnacioMonge/SpecTalkZX/releases/tag/v1.4.0).
2. Copia <code>SpecTalkZX.tap</code>, <code>SPECTALK.OVL</code> y
   <code>SPECTALK.DAT</code> al mismo directorio de la tarjeta SD.
3. Configura el puente ESP-AT a **115200 baudios**. Las credenciales Wi-Fi se
   pueden preparar con [NetManZX](https://github.com/IgnacioMonge/NetManZX) o
   una herramienta ESP-AT equivalente.
4. Carga <code>SpecTalkZX.tap</code>, espera al indicador de red y conecta a
   IRC.

### Spectrum Next nativo

1. Descarga `spectalk_next_v1.4.0.zip` desde la
   [versión 1.4.0](https://github.com/IgnacioMonge/SpecTalkZX/releases/tag/v1.4.0).
2. Configura el ESP interno del Spectrum Next para la red Wi-Fi deseada.
3. Copia <code>SPECTALK.NEX</code> a la tarjeta SD del Next.
4. Ejecútalo desde el navegador de NextZXOS. La configuración y los marcadores
   se guardan en <code>/SYS/CONFIG</code>, con <code>/SYS</code> como alternativa.

### Cartucho Spectranext

El instalador público ofrece SpecTalkZX 1.4.0 y requiere firmware Spectranext
`0.9-6fc153a3` o posterior.

1. Conecta el cartucho a la red Wi-Fi.
2. En el menú de Spectranext, selecciona **Load Resource URL** e introduce:
   <code>https://ignaciomonge.github.io/SpecTalkZX/</code>.
3. El instalador guiado valida el paquete, escribe
   <code>SPECTALK.tap</code>, <code>SPECTALK.OVL</code>,
   <code>SPECTALK.DAT</code> y <code>SPECTALK.ZX</code> en el XFS local, y
   arranca el cliente.
4. En adelante inicia <code>SPECTALK.ZX</code> desde el XFS local. Para
   actualizar, vuelve a usar **Load Resource URL**; se conservan
   <code>/CFG/SPECTALK.CFG</code> y los cinco marcadores.

---

## Inicio rápido

```text
/nick TuNick
/server irc.libera.chat 6667
/join #spectrum
```

Configuración inicial útil:

```text
!theme 1
!timestamps smart
!notif on
!nickcolor on
!save
```

Para guardar una sesión completa, abre `!bm`. En el gestor de marcadores,
**ARRIBA/ABAJO** selecciona una ranura, **S** guarda la sesión actual, **A** la
marca para el arranque, **ENTER** conecta, **D** borra y **BREAK** guarda y sale.

---

## Capturas

La galería combina capturas de la versión 1.4.0 para Spectrum Next nativo,
Spectranext y Classic.

### Primeros pasos y navegación

<table>
  <tr>
    <td align="center" valign="top" width="50%">
      <strong>Instalador guiado</strong><br>
      <a href="images/snapshot-spectranext-installer.png"><img src="images/snapshot-spectranext-installer.png" width="420" alt="Instalador guiado de SpecTalkZX 1.4.0 en Spectranext"></a><br>
      <sub>El instalador Spectranext escribe el cliente en el XFS local y conserva la configuración y los marcadores.</sub>
    </td>
    <td align="center" valign="top" width="50%">
      <strong>Elegir nick</strong><br>
      <a href="images/snapshot-nick.png"><img src="images/snapshot-nick.png" width="420" alt="Selección del nick IRC"></a><br>
      <sub>Selección inicial del nick antes de abrir la conexión con el servidor.</sub>
    </td>
  </tr>
  <tr>
    <td align="center" valign="top" width="50%">
      <strong>Servidor conectado</strong><br>
      <a href="images/snapshot-connected.png"><img src="images/snapshot-connected.png" width="420" alt="Conexión a Libera Chat"></a><br>
      <sub>Ventana del servidor tras conectar a IRC desde Spectranext.</sub>
    </td>
    <td align="center" valign="top" width="50%">
      <strong>Navegación de ventanas</strong><br>
      <a href="images/snapshot-options.png"><img src="images/snapshot-options.png" width="420" alt="Ventanas de servidor y canales"></a><br>
      <sub>La barra de pestañas deja servidor y canales activos a una sola tecla.</sub>
    </td>
  </tr>
</table>

### Conversación y descubrimiento

<table>
  <tr>
    <td align="center" valign="top" width="33%">
      <strong>Entrada a un canal</strong><br>
      <a href="images/snapshot-joining.png"><img src="images/snapshot-joining.png" width="280" alt="Entrada a un canal IRC"></a><br>
      <sub>El progreso y el contexto del canal permanecen visibles durante el registro.</sub>
    </td>
    <td align="center" valign="top" width="33%">
      <strong>Conversación en directo</strong><br>
      <a href="images/snapshot-chat.png"><img src="images/snapshot-chat.png" width="280" alt="Conversación IRC en Spectrum Next nativo"></a><br>
      <sub>Chat nativo en Spectrum Next con timestamps, colores de nick, modos y estado no leído.</sub>
    </td>
    <td align="center" valign="top" width="33%">
      <strong>Respuesta privada rápida</strong><br>
      <a href="images/snapshot-fast-reply.png"><img src="images/snapshot-fast-reply.png" width="280" alt="Respuesta rápida a un privado"></a><br>
      <sub>Pulsa ENTER sobre una notificación privada para abrir la conversación.</sub>
    </td>
  </tr>
  <tr>
    <td align="center" valign="top" width="33%">
      <strong>Amigos conectados</strong><br>
      <a href="images/snapshot-friends-online.png"><img src="images/snapshot-friends-online.png" width="280" alt="Detección de amigos"></a><br>
      <sub>Los amigos encontrados durante NAMES se agrupan en una sola notificación.</sub>
    </td>
    <td align="center" valign="top" width="33%">
      <strong>Usuarios del canal</strong><br>
      <a href="images/snapshot-users.png"><img src="images/snapshot-users.png" width="280" alt="Lista de usuarios del canal"></a><br>
      <sub>La vista /names paginada en cuatro columnas mantiene legibles las listas largas.</sub>
    </td>
    <td align="center" valign="top" width="33%">
      <strong>Búsqueda de canales</strong><br>
      <a href="images/snapshot-channel-search.png"><img src="images/snapshot-channel-search.png" width="280" alt="Resultados de búsqueda de canales"></a><br>
      <sub>Búsqueda y paginación muestran listas extensas sin interferir con la entrada.</sub>
    </td>
  </tr>
</table>

### Gestión e información

<table>
  <tr>
    <td align="center" valign="top" width="33%">
      <strong>Marcadores</strong><br>
      <a href="images/snapshot-bookmarks.png"><img src="images/snapshot-bookmarks.png" width="280" alt="Gestor de marcadores IRC"></a><br>
      <sub>Cinco ranuras independientes guardan servidor, puerto, canales y política de arranque.</sub>
    </td>
    <td align="center" valign="top" width="33%">
      <strong>Configuración</strong><br>
      <a href="images/snapshot-config.png"><img src="images/snapshot-config.png" width="280" alt="Resumen de configuración"></a><br>
      <sub>Toda la configuración activa puede revisarse sin salir del cliente.</sub>
    </td>
    <td align="center" valign="top" width="33%">
      <strong>Estado de conexión</strong><br>
      <a href="images/snapshot-status.png"><img src="images/snapshot-status.png" width="280" alt="Estado de conexión"></a><br>
      <sub>Red, latencia, tiempo activo y ventanas abiertas aparecen en un único resumen.</sub>
    </td>
  </tr>
  <tr>
    <td align="center" valign="top" width="33%">
      <strong>Ayuda de comandos</strong><br>
      <a href="images/snapshot-help.png"><img src="images/snapshot-help.png" width="280" alt="Ayuda integrada"></a><br>
      <sub>La ayuda integrada cubre los comandos locales e IRC sin salir del cliente.</sub>
    </td>
    <td align="center" valign="top" width="33%">
      <strong>Acerca de</strong><br>
      <a href="images/snapshot-about.png"><img src="images/snapshot-about.png" width="280" alt="Pantalla About animada en Spectrum Next nativo"></a><br>
      <sub>La Tierra animada y el banner de Spectrum Next nativo durante una conexión activa.</sub>
    </td>
    <td align="center" valign="top" width="33%">
      <strong>Novedades</strong><br>
      <a href="images/snapshot-changes.png"><img src="images/snapshot-changes.png" width="280" alt="Pantalla What's New de SpecTalkZX 1.4.0 Proteus en Spectrum Next"></a><br>
      <sub>La pantalla 1.4.0 Proteus presenta la versión nativa para Next y el nuevo sistema de paginación Spectranext.</sub>
    </td>
  </tr>
</table>

### Temas

<table>
  <tr>
    <td align="center" valign="top" width="33%">
      <strong>Tema 1</strong><br>
      <a href="images/snapshot-theme-1-away.png"><img src="images/snapshot-theme-1-away.png" width="280" alt="Tema 1 con estado away"></a><br>
      <sub>Paleta predeterminada con away, notificaciones y nicks coloreados.</sub>
    </td>
    <td align="center" valign="top" width="33%">
      <strong>Tema 2</strong><br>
      <a href="images/snapshot-theme-2.png"><img src="images/snapshot-theme-2.png" width="280" alt="Tema 2"></a><br>
      <sub>Paleta verde de estilo terminal con la interfaz IRC completa.</sub>
    </td>
    <td align="center" valign="top" width="33%">
      <strong>Tema 3</strong><br>
      <a href="images/snapshot-theme-3.png"><img src="images/snapshot-theme-3.png" width="280" alt="Tema 3"></a><br>
      <sub>Paleta azul y roja de alto contraste.</sub>
    </td>
  </tr>
</table>

---

## Interfaz

- **Chat de 64 columnas** con fuente personalizada de 4 píxeles.
- **Hasta 10 ventanas**: servidor `0` y canales o conversaciones privadas `1` a `9`.
- **Cambio directo** con `!0` a `!9` o `/0` a `/9`.
- **Selector con EDIT** e indicadores de mensajes no leídos y menciones.
- **Tres temas** con distintivos y comportamiento visual propio.
- **Colores por nick** con `!nickcolor`.
- **Notificaciones inteligentes** con mini-fuente Ikkle-4 en la fila inferior.
- **Respuesta rápida a privados**: ENTER sobre la notificación abre la conversación.
- **Marcas de tiempo** desactivadas, activadas o inteligentes.
- **Separadores de contexto** con `!divider`.
- **Barra de estado** con nick, ventana, red y modos, usuarios, reloj, ausencia e indicador de conexión.


## Controles de teclado

| Tecla | Acción |
|-------|--------|
| **ENTER** | Enviar mensaje, ejecutar comando o aceptar una acción |
| **EDIT** | Abrir o cerrar el selector de canales |
| **DELETE** | Borrar el carácter anterior |
| **IZQUIERDA/DERECHA** | Mover el cursor o la selección |
| **ARRIBA/ABAJO** | Recorrer el historial de comandos o seleccionar una fila |
| **Symbol Shift + IZQUIERDA/DERECHA** | Movimiento por palabras |
| **Symbol Shift + DELETE** | Borrar palabra anterior |
| **BREAK** | Cerrar una notificación, cancelar la paginación o salir de una pantalla |
| **ENTER en notificación privada** | Abrir la conversación con el remitente |

---


## Comportamiento IRC

- Soporta el flujo IRC habitual: `JOIN`, `PART`, `QUIT`, `NICK`, `PRIVMSG`, `NOTICE`, `TOPIC`, `MODE`, `KICK`, `WHO`, `WHOIS`, `LIST` y `NAMES`.
- Soporta CTCP `VERSION`, `PING`, `TIME` y `ACTION`.
- NickServ se puede usar manualmente con `/id` o automáticamente con `nickpass=`.
- `nickserv=` permite fijar el nick del servicio si la red no usa el nombre estándar `NickServ`.
- Los amigos se siguen con `!friend`; los resultados de JOIN/NAMES generan notificaciones compactas.
- Los nicks ignorados se gestionan con `/ignore`, incluido el borrado con `-nick`.
- El estado de ausencia admite `/away` manual y `!autoaway` por inactividad.
- La comprobación periódica detecta desconexiones silenciosas y continúa durante About.
- `!countsync` ayuda a mantener los contadores de usuarios durante sesiones largas.

---
## Comandos

### Comandos locales

| Comando | Alias | Descripción |
|---------|-------|-------------|
| `!help` | `!h` | Muestra ayuda |
| `!status` | `!s` | Conexión, latencia, tiempo activo y ventanas |
| `!init` | `!i` | Reinicia la conexión de red |
| `!config` | `!cfg` | Muestra todos los ajustes |
| `!theme N` | | Cambia tema `1`, `2` o `3` |
| `!about` | | Pantalla About animada |
| `!changelog` | | Pantalla What's New |
| `!bookmarks` | `!bm` | Gestor de marcadores IRC |
| `!save` | `!sv` | Guarda configuración y sesión |
| `!autoconnect` | `!ac` | Alterna la conexión automática al servidor |
| `!autojoin` | | Alterna la entrada automática a canales guardados |
| `!tz` | | Muestra o fija la zona `-12`..`+12`; `rtc` usa un RTC local compatible |
| `!timestamps` | `!ts` | Cambia las marcas de tiempo entre off/on/smart |
| `!notif` | `!nf` | Alterna notificaciones |
| `!beep` | | Alterna el sonido de mención |
| `!click` | | Alterna el sonido de las teclas |
| `!traffic` | | Muestra u oculta mensajes JOIN/PART/QUIT |
| `!divider` | | Alterna separadores de contexto |
| `!countsync` | `!cs` | Alterna la resincronización de contadores |
| `!autoaway` | `!aa` | Activa la ausencia tras N minutos; `0` la desactiva |
| `!friend` | | Lista, añade o quita amigos seguidos |
| `!nickcolor` | `!nc` | Alterna colores por nick |
| `!clear` | `!cls` | Limpia el área de chat |

Los comandos de activación sin argumento alternan su estado; también aceptan
`on`/`off`/`1`/`0`.
`!timestamps` añade `smart`.

### Comandos IRC

| Comando | Alias | Descripción |
|---------|-------|-------------|
| `/server [host [puerto]\|host:puerto]` | `/connect` | Sin argumentos muestra el estado o reconecta; con argumentos conecta al servidor indicado |
| `/nick [nombre]` | | Muestra o cambia nick |
| `/pass [password\|clear\|none]` | | Muestra o fija la contraseña guardada; `clear`/`none` la borran para la próxima conexión |
| `/id [password]` | | Identifica con NickServ o servicio detectado |
| `/join canal\|#canal\|&canal` | `/j` | Entra en un canal sin prefijo o con `#` o `&` |
| `/part [#canal\|&canal] [mensaje]` | `/p` | Sale del canal actual o del canal `#`/`&` indicado |
| `/msg nick texto` | `/m` | Envía un mensaje privado |
| `/reply texto` | | Responde al último mensaje privado recibido |
| `/notice destino texto` | | Envía un aviso IRC |
| `/query nick` | `/q` | Abre una conversación privada |
| `/close` | | Cierra la conversación privada o sale del canal actual |
| `/quit [mensaje]` | | Desconecta con confirmación |
| `/me accion` | | Envía una acción CTCP |
| `/away [mensaje]` | | Activa o desactiva el estado de ausencia |
| `/raw comando` | | Envía un comando IRC sin modificar |
| `/whois nick` | `/wi` | Muestra WHOIS |
| `/who [canal\|nick]` | | Busca usuarios; sin argumentos usa el canal actual |
| `/list patron` | `/ls` | Lista canales |
| `/names [#canal\|&canal]` | | Lista paginada del canal actual o del indicado |
| `/topic [#canal] [texto]` | | Muestra o cambia el tema; canal y texto son opcionales |
| `/mode [opciones]` | | Muestra o cambia modos IRC |
| `/search #patron\|nick` | | Busca canales LIST con `#patron` o usuarios WHO con `nick` |
| `/ignore [nick]` | | Lista, añade o quita nicks ignorados (`-nick`) |
| `/kick nick [razon]` | `/k` | Expulsa del canal actual |
| `/channels` | `/w` | Lista ventanas abiertas |
| `/0`..`/9` | | Cambia a una ventana numerada |

`/pass` solo actualiza la contraseña guardada para la próxima conexión; no
envía un comando IRC `PASS` inmediato. Las formas numéricas `!0`..`!9` y
`/0`..`/9` seleccionan directamente las ventanas numeradas.

---

## Configuración

SpecTalkZX guarda la configuración actual con `!save`. Classic y Next nativo
cargan `SPECTALK.CFG` desde `/SYS/CONFIG/`, con `/SYS/` como alternativa.
Spectranext carga `/CFG/SPECTALK.CFG` desde el XFS local del cartucho.

```ini
nick=MiNick
server=irc.libera.chat
port=6667
pass=
nickpass=miPasswordNickServ
nickserv=NickServ
autoconnect=1
autojoin=1
channels=#spectrum,#zx
theme=1
timestamps=2
autoaway=15
beep=1
click=1
traffic=1
divider=1
countsync=1
tz=1
notif=1
nickcolor=1
friends=Amigo1,Amigo2
ignores=NickPesado
```


Ajustes soportados:

| Ajuste | Valores | Notas |
|--------|---------|-------|
| `nick` | Nick IRC | Nick por defecto |
| `server` | Nombre de host o IP | Servidor IRC |
| `port` | Puerto decimal | El puerto IRC predeterminado es `6667` |
| `pass` | Texto o vacío | Contraseña del servidor |
| `nickpass` | Texto o vacío | Contraseña de NickServ para `/id` e identificación automática |
| `nickserv` | Nick o vacío | Nombre del servicio en redes no estándar |
| `autoconnect` | `0`/`1` | Conectar al servidor guardado al arrancar |
| `autojoin` | `0`/`1` | Entrar en `channels` tras el registro IRC |
| `channels` | Canales separados por coma | Lista de canales para restaurar la sesión |
| `theme` | `1`, `2`, `3` | Tema de color |
| `timestamps` | `0`, `1`, `2` | Off, on, smart |
| `beep` | `0`/`1` | Sonido de mención |
| `click` | `0`/`1` | Sonido de las teclas |
| `traffic` | `0`/`1` | Mostrar JOIN/PART/QUIT |
| `divider` | `0`/`1` | Separadores de contexto de canal |
| `countsync` | `0`/`1` | Resincronización de contadores durante periodos inactivos |
| `notif` | `0`/`1` | Notificaciones en fila inferior |
| `nickcolor` | `0`/`1` | Colores por nick |
| `autoaway` | `0`-`60` | Minutos de inactividad, `0` desactiva |
| `tz` | `-12`..`+12` o `rtc` | Desfase SNTP o RTC local cuando sea compatible |
| `tzlast` | `-12`..`+12` | Última zona numérica usada al salir de RTC |
| `friends` | Nicks separados por coma | Hasta cinco amigos seguidos |
| `ignores` | Nicks separados por coma | Hasta cinco nicks ignorados |

Ajustes destacables:

- `autoconnect=1` conecta al servidor guardado al arrancar.
- `autojoin=1` reenvía `channels=` tras el registro IRC y la espera de NickServ, si hace falta.
- En Classic y Next nativo, `tz=rtc` usa un RTC local detectado. Spectranext
  no expone RTC al Z80: usa UDP/SNTP y, si se selecciona `rtc`, vuelve a
  `tzlast`.
- `divider=0` oculta futuros separadores de contexto.
- `countsync=0` desactiva el refresco de contadores durante periodos inactivos.
- `friends=` e `ignores=` admiten hasta cinco nicks cada uno.

Los marcadores usan `/SYS/CONFIG/SPTBM1.CFG` a `SPTBM5.CFG` en Classic y Next
nativo, con `/SYS/SPTBM1.CFG` a `SPTBM5.CFG` como alternativa si falta
`/SYS/CONFIG`. Spectranext usa `/CFG/SPTBM1.CFG` a `SPTBM5.CFG`.

---

## Límites de Spectranext

- IRC usa texto plano en el puerto configurado. El cartucho expone TLS solo en
  su servicio fijo del puerto 443, por lo que no hay IRC TLS en el puerto 6697.
- El cartucho no expone RTC al Z80. Spectranext utiliza UDP/SNTP; `tz=rtc`
  vuelve a la última zona numérica guardada en `tzlast`.
- SpecTalkZX utiliza una sola conexión del cartucho cada vez. La sincronización
  del reloj se realiza antes de abrir la conexión IRC.
- La configuración y los marcadores se guardan en XFS. Si se corta la
  alimentación durante una escritura, el fichero puede quedar incompleto.

---

## Compilación

Requisitos: z88dk con soporte SDCC, GNU Make, Python 3.8 o posterior y un
conjunto de herramientas de shell compatible con POSIX. En Windows, w64devkit
incluye Make y las utilidades necesarias.

```sh
# ZX Classic
make NO_COLOR=1

# Spectrum Next nativo
make next NO_COLOR=1

# Compilaciones de publicación
make release NO_COLOR=1
make release NO_COLOR=1 PLATFORM=next
make release NO_COLOR=1 PLATFORM=spectranext SPXN_DIR=/ruta/a/Spectranext/driver
```

Salidas:

- `build/SpecTalkZX.tap`
- `build/SPECTALK.OVL`
- `build/SPECTALK.DAT`
- `build/SPECTALK.NEX` con `make next` (imagen autónoma para Next nativo)

El objetivo Spectranext también necesita el directorio `driver` del SDK de
Spectranext.

---

## Solución de problemas

| Problema | Comprobar |
|---|---|
| El indicador Classic permanece rojo | Alimentación, cableado y 115200 baudios del puente ESP-AT |
| No se detecta Spectranext | Cartucho presente y `/CFG` disponible en XFS local |
| La red está lista pero IRC no conecta | Credenciales Wi-Fi, nombre del servidor y puerto IRC en texto plano |
| El arranque se detiene en esxDOS/DAT | divMMC montado y los tres ficheros juntos de la misma compilación |
| Fallan la ayuda, About o los marcadores | Falta `SPECTALK.OVL` o `SPECTALK.DAT`, o pertenece a otra compilación |
| No empieza la instalación Spectranext | Selecciona **Load Resource URL** e introduce `https://ignaciomonge.github.io/SpecTalkZX/` |
| El reloj sigue en `00:00` | Acceso SNTP y zona numérica; Classic y Next nativo también admiten `!tz rtc` |
| Falla NickServ | Usa `/id`, `nickpass=` o un nombre de servicio alternativo en `nickserv=` |
| Demasiados JOIN/PART | Alterna `!traffic` |
| Se desajustan los contadores | Mantén `!countsync` o ejecuta `/names` |
| Los acentos se ven mal | UTF-8 se convierte al juego de caracteres visible del ZX |
| `/reply` no tiene destino | Primero debe recibirse un PM para recordar su remitente |

---

## Licencia

SpecTalkZX es software libre publicado bajo **GNU General Public License
v2.0**.

Incluye código derivado de:

- Cliente IRC **BitchZX**.
- Controlador UART de **Nihirash**.
- Mini fuente **Ikkle-4** de Jack Oatley.

---

## Autor

**M. Ignacio Monge Garcia** — 2025–2026

*Conectando el ZX Spectrum a IRC desde 2025.*
