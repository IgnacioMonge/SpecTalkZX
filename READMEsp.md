# SpecTalkZX

<p align="center">
  <img src="images/spectalkzx-banner.png" alt="SpecTalkZX" width="80%">
</p>

**Cliente IRC para ZX Spectrum y el cartucho SpectraNext**

:gb: [Read in English](README.md)

**Instalación:** [ZX Classic / divMMC](#zx-classic--divmmc) ·
[Cartucho SpectraNext](#cartucho-spectranext)

![Plataforma](https://img.shields.io/badge/Plataforma-ZX%20Spectrum%20%7C%20SpectraNext-blue)
![Licencia](https://img.shields.io/badge/Licencia-GPLv2-green)
![Versión](https://img.shields.io/badge/Versi%C3%B3n-1.3.9-orange)

SpecTalkZX 1.3.9 es la primera versión con soporte nativo para SpectraNext.
Lleva el cliente IRC completo al cartucho mediante sockets ROM, almacenamiento
XFS y un instalador HTTPS guiado, sin abandonar la versión Classic para
ZX/divMMC. Esta versión también reúne todos los cambios de fiabilidad e
interfaz realizados desde la publicación de 1.3.8.

---

## Novedades principales de 1.3.9

- **Objetivo SpectraNext nativo** mediante sockets ROM y DNS del cartucho, sin
  una capa UART/ESP-AT.
- **Instalador guiado de SpectraNext** que verifica el paquete, instala una
  copia local en XFS y conserva configuración y bookmarks al actualizar.
- **Sesiones persistentes en SpectraNext** bajo <code>/CFG</code>, incluidos los
  cinco bookmarks, canales, autoconnect y autojoin.
- **Reloj UDP/SNTP para SpectraNext**, con zona horaria numérica antes de abrir
  el único socket IRC.
- **Classic sigue plenamente soportado** con sus tres ficheros para divMMC y la
  misma interfaz y comandos IRC.
- **Esperas UART, ESP-AT y UDP crudo acotadas**, evitando bloqueos indefinidos o
  continuar tras transmisiones parciales.
- **Configuración y listas más seguras** mediante claves exactas, controles de
  capacidad y renderizado acotado de amigos e ignorados.
- **Estado de interfaz protegido** durante operaciones de configuración,
  bookmarks y overlays en ambos backends.
- **Animación About/Earth más robusta**, con validación de paquetes, lecturas
  seguras frente a interrupciones y bombeo de red SpectraNext fluido al estar
  conectado.
- **Scroll de chat seguro frente a interrupciones**, sin utilizar RAM temporal
  de pantalla como pila.
- **Errores de registro IRC más claros** mediante reconocimiento exacto del
  token <code>ERROR</code>.
- **Nueva presentación** con imagen completa, doce cambios, espaciado
  correcto de resultados y banner específico para cada objetivo.

Consulta [CHANGELOG.md](CHANGELOG.md) para el historial completo, límites
técnicos y medidas finales de compilación.

---

## Capturas


### Primeros pasos y navegación

<table>
  <tr>
    <td align="center" valign="top" width="50%">
      <strong>Instalador guiado</strong><br>
      <a href="images/snapshot-spectranext-installer.png"><img src="images/snapshot-spectranext-installer.png" width="420" alt="Instalador guiado de SpecTalkZX en SpectraNext"></a><br>
      <sub>El recurso HTTPS inicia el instalador guiado. Verifica y escribe la aplicación en el XFS local sin tocar los datos del usuario bajo <code>/CFG</code>.</sub>
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
      <sub>Ventana del servidor tras conectar IRC mediante el socket nativo de SpectraNext.</sub>
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
      <a href="images/snapshot-chat.png"><img src="images/snapshot-chat.png" width="280" alt="Conversación IRC normal"></a><br>
      <sub>Chat de 64 columnas con timestamps, colores de nick, modos y estado no leído.</sub>
    </td>
    <td align="center" valign="top" width="33%">
      <strong>Respuesta privada rápida</strong><br>
      <a href="images/snapshot-fast-reply.png"><img src="images/snapshot-fast-reply.png" width="280" alt="Respuesta rápida a un privado"></a><br>
      <sub>Las notificaciones PM permiten abrir la respuesta inmediatamente con ENTER.</sub>
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
      <strong>Bookmarks</strong><br>
      <a href="images/snapshot-bookmarks.png"><img src="images/snapshot-bookmarks.png" width="280" alt="Gestor de bookmarks IRC"></a><br>
      <sub>Cinco slots independientes guardan servidor, puerto, canales y política de arranque.</sub>
    </td>
    <td align="center" valign="top" width="33%">
      <strong>Configuración</strong><br>
      <a href="images/snapshot-config.png"><img src="images/snapshot-config.png" width="280" alt="Resumen de configuración"></a><br>
      <sub>Toda la configuración activa puede revisarse sin salir del cliente.</sub>
    </td>
    <td align="center" valign="top" width="33%">
      <strong>Estado de conexión</strong><br>
      <a href="images/snapshot-status.png"><img src="images/snapshot-status.png" width="280" alt="Estado de conexión"></a><br>
      <sub>Red, latencia, uptime y ventanas abiertas aparecen en un único resumen.</sub>
    </td>
  </tr>
  <tr>
    <td align="center" valign="top" width="33%">
      <strong>Ayuda de comandos</strong><br>
      <a href="images/snapshot-help.png"><img src="images/snapshot-help.png" width="280" alt="Ayuda integrada"></a><br>
      <sub>La ayuda generada de cinco páginas cubre los comandos locales e IRC dentro del ZX.</sub>
    </td>
    <td align="center" valign="top" width="33%">
      <strong>Acerca de</strong><br>
      <a href="images/snapshot-about.png"><img src="images/snapshot-about.png" width="280" alt="Pantalla About animada"></a><br>
      <sub>La animación de la Tierra mantiene activos PING/PONG y el control de conexión.</sub>
    </td>
    <td align="center" valign="top" width="33%">
      <strong>Novedades</strong><br>
      <a href="images/snapshot-changes.png"><img src="images/snapshot-changes.png" width="280" alt="Pantalla What's New de SpecTalkZX"></a><br>
      <sub>La pantalla 1.3.9 combina la imagen de SpecTalkZX con doce cambios concisos.</sub>
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
      <sub>Paleta azul/roja de alto contraste que demuestra un renderizado seguro por tema.</sub>
    </td>
  </tr>
</table>

---

## Requisitos

| Objetivo | Ordenador | Almacenamiento | Red |
|---|---|---|---|
| Classic | ZX Spectrum 48K, 128K, +2, +2A, +3 o compatible | SD con divMMC/esxDOS | UART soportada a 115200 baudios con ESP8266 o puente ESP-AT compatible |
| SpectraNext | Modelo de ZX Spectrum compatible con el firmware del cartucho SpectraNext | XFS local del cartucho | Wi-Fi nativo y sockets ROM del cartucho |

Ambas builds utilizan un conjunto coincidente: <code>SpecTalkZX.tap</code>,
<code>SPECTALK.OVL</code> y <code>SPECTALK.DAT</code>. No mezcles ficheros de
compilaciones distintas. Un atlas o DAT antiguo puede romper ayuda, About,
configuración, bookmarks o What's New.

---

## Instalación

### ZX Classic / divMMC

1. Descarga el archivo **Classic** de la release de GitHub.
2. Copia <code>SpecTalkZX.tap</code>, <code>SPECTALK.OVL</code> y
   <code>SPECTALK.DAT</code> al mismo directorio de la tarjeta SD.
3. Configura el puente ESP-AT a **115200 baudios**. Las credenciales Wi-Fi se
   pueden preparar con [NetManZX](https://github.com/IgnacioMonge/NetManZX) o
   una herramienta ESP-AT equivalente.
4. Carga <code>SpecTalkZX.tap</code>, espera al indicador de red y conecta a
   IRC.

### Cartucho SpectraNext

SpectraNext **no** utiliza el procedimiento de copia para divMMC de Classic. No
pongas <code>SpecTalkZX.tap</code>, <code>SPECTALK.OVL</code> ni
<code>SPECTALK.DAT</code> en una SD. La instalación pública es un recurso
HTTPS. La raíz canónica, en GitHub Pages de este repositorio, es:

<code>https://ignaciomonge.github.io/SpecTalkZX/</code>

1. Configura el cartucho y el Wi-Fi con las
   [instrucciones oficiales de SpectraNext](https://docs.spectranext.net/tutorials/setting-up-mounts).
2. En el prompt de BASIC:

```text
%umount 2
%mount 2, "https://ignaciomonge.github.io/SpecTalkZX/"
%fs 2
%cat
%load "boot.zx"
```

   <code>boot.zx</code> es BASIC tokenizado. No uses <code>%tapein</code>.
3. El instalador guiado valida el paquete, escribe
   <code>SPECTALK.tap</code>, <code>SPECTALK.OVL</code>,
   <code>SPECTALK.DAT</code>, <code>SPECTALK.ZX</code> y el marcador de
   versión en el slot 0 del XFS local, y arranca el cliente.
4. En adelante inicia <code>SPECTALK.ZX</code> desde el XFS local. Para
   actualizar, monta la misma raíz HTTPS y carga <code>boot.zx</code> otra
   vez; se conservan <code>/CFG/SPECTALK.CFG</code> y los cinco bookmarks.

Un zip de GitHub Release no es un recurso montable. Pasos de publicación:
[Publicación del recurso SpectraNext](#publicación-del-recurso-spectranext).

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

Para guardar una sesion completa, abre `!bm`. Dentro de bookmarks: **ARRIBA/ABAJO** selecciona slot, **S** guarda el snapshot actual, **A** lo marca para arranque, **ENTER** conecta, **D** borra y **BREAK** guarda/sale.

---

## Interfaz

- **Chat de 64 columnas** con fuente personalizada de 4 pixeles.
- **Hasta 10 ventanas**: servidor `0` y canales/queries `1` a `9`.
- **Cambio directo** con `!0` a `!9` o `/0` a `/9`.
- **Switcher con EDIT** e indicadores de no leido/mencion.
- **Tres temas** con badges y comportamiento visual propio.
- **Coloreado de nicks** por hash estable, con `!nickcolor`.
- **Notificaciones inteligentes** con mini-fuente Ikkle-4 en la fila inferior.
- **Respuesta rapida a PM**: ENTER sobre la notificacion abre la query.
- **Timestamps** off/on/smart.
- **Separadores de contexto** con `!divider`.
- **Barra de estado** con nick, ventana, red/modos, usuarios, reloj, away y LED.


## Controles de teclado

| Tecla | Accion |
|-------|--------|
| **ENTER** | Enviar mensaje, ejecutar comando o aceptar accion de overlay |
| **EDIT** | Abrir/cerrar switcher de canales |
| **DELETE** | Borrar caracter anterior |
| **IZQUIERDA/DERECHA** | Mover cursor o seleccion de overlay |
| **ARRIBA/ABAJO** | Historial de comandos o seleccion de fila |
| **Symbol Shift + IZQUIERDA/DERECHA** | Movimiento por palabras |
| **Symbol Shift + DELETE** | Borrar palabra anterior |
| **BREAK** | Cerrar notificacion, cancelar paginacion o salir de overlay |
| **ENTER en notificacion PM** | Abrir query del remitente |

---


## Comportamiento IRC

- Soporta el flujo IRC habitual: `JOIN`, `PART`, `QUIT`, `NICK`, `PRIVMSG`, `NOTICE`, `TOPIC`, `MODE`, `KICK`, `WHO`, `WHOIS`, `LIST` y `NAMES`.
- Soporta CTCP `VERSION`, `PING`, `TIME` y `ACTION`.
- NickServ se puede usar manualmente con `/id` o automaticamente con `nickpass=`.
- `nickserv=` permite fijar el nick del servicio si la red no usa el nombre estandar `NickServ`.
- Los amigos se monitorizan con `!friend`; los batches JOIN/NAMES generan notificaciones compactas.
- Los ignores se gestionan con `/ignore`, incluido borrado con `-nick`.
- Away soporta `/away` manual y `!autoaway` por inactividad.
- El keepalive detecta desconexiones silenciosas y tambien sigue activo durante About.
- Las sesiones largas mantienen mejor los contadores mediante NAMES y `!countsync` opcional.

---
## Comandos

### Comandos locales

| Comando | Alias | Descripcion |
|---------|-------|-------------|
| `!help` | `!h` | Muestra ayuda |
| `!status` | `!s` | Conexion, latencia, uptime y ventanas |
| `!init` | `!i` | Reinicia el backend de red activo |
| `!config` | `!cfg` | Muestra todos los ajustes |
| `!theme N` | | Cambia tema `1`, `2` o `3` |
| `!about` | | Pantalla About animada |
| `!changelog` | | Pantalla What's New |
| `!bookmarks` | `!bm` | Gestor de bookmarks IRC |
| `!save` | `!sv` | Guarda configuracion y sesion |
| `!autoconnect` | `!ac` | Alterna conexion automatica al servidor |
| `!autojoin` | | Alterna entrada automatica a canales guardados |
| `!tz` | | Muestra/fija zona `-12`..`+12`; `rtc` usa el RTC de Classic |
| `!timestamps` | `!ts` | Cicla timestamps off/on/smart |
| `!notif` | `!nf` | Alterna notificaciones |
| `!beep` | | Alterna sonido de mencion |
| `!click` | | Alterna click de tecla |
| `!traffic` | | Alterna ruido JOIN/PART/QUIT |
| `!divider` | | Alterna separadores de contexto |
| `!countsync` | `!cs` | Alterna resincronizacion de contadores |
| `!autoaway` | `!aa` | Auto-away tras N minutos, `0` desactiva |
| `!friend` | | Lista o alterna amigos monitorizados |
| `!nickcolor` | `!nc` | Alterna colores por nick |
| `!clear` | `!cls` | Limpia el area de chat |

Los toggles sin argumento alternan; aceptan `on`/`off`/`1`/`0`.
`!timestamps` añade `smart`.

### Comandos IRC

| Comando | Alias | Descripcion |
|---------|-------|-------------|
| `/server [host[:port]]` | `/connect` | Sin args: muestra estado o reconecta servidor guardado; si no, conecta a `host[:port]` |
| `/nick [nombre]` | | Muestra o cambia nick |
| `/pass [password\|clear\|none]` | | Muestra/fija password guardado; `clear`/`none` lo borran para la proxima conexion |
| `/id [password]` | | Identifica con NickServ o servicio detectado |
| `/join canal\|#canal\|&canal` | `/j` | Entra a canal sin prefijo, con prefijo `#` o `&` |
| `/part [#canal\|&canal] [mensaje]` | `/p` | Sale del canal actual o del canal `#`/`&` indicado |
| `/msg nick texto` | `/m` | Envia privado |
| `/reply texto` | | Responde al ultimo PM recibido |
| `/notice destino texto` | | Envia IRC NOTICE |
| `/query nick` | `/q` | Abre query privada |
| `/close` | | Cierra query o hace part del canal actual |
| `/quit [mensaje]` | | Desconecta con confirmacion |
| `/me accion` | | Envia CTCP ACTION |
| `/away [mensaje]` | | Marca o limpia away |
| `/raw comando` | | Envia comando IRC crudo |
| `/whois nick` | `/wi` | Muestra WHOIS |
| `/who [canal\|nick]` | | Busca usuarios; sin args usa el canal actual |
| `/list patron` | `/ls` | Lista canales |
| `/names [#canal\|&canal]` | | Grid paginado del canal actual o del indicado |
| `/topic [#canal] [texto]` | | Muestra/fija topic; canal y texto son opcionales |
| `/mode [args]` | | Muestra o fija modos |
| `/search #patron\|nick` | | Busca canales LIST con `#patron` o usuarios WHO con `nick` |
| `/ignore [nick]` | | Lista, anade o quita ignorados (`-nick`) |
| `/kick nick [razon]` | `/k` | Expulsa del canal actual |
| `/channels` | `/w` | Lista ventanas abiertas |
| `/0`..`/9` | | Cambia a slot fisico de ventana |

`/pass` solo actualiza el password guardado para la proxima conexion; no envia
un comando IRC `PASS` inmediato. Las formas numericas `!0`..`!9` y `/0`..`/9`
son atajos del dispatcher para slots fisicos, no aliases.

---

## Configuración

SpecTalkZX guarda la configuración actual con `!save`. Classic carga `SPECTALK.CFG` desde `/SYS/CONFIG/`, con `/SYS/` como alternativa. SpectraNext carga `/CFG/SPECTALK.CFG` desde el XFS local del cartucho.

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
| `server` | Hostname/IP | Servidor IRC |
| `port` | `1`-`65535` | Puerto IRC, normalmente `6667` |
| `pass` | Texto o vacio | Password de servidor |
| `nickpass` | Texto o vacio | Password NickServ para `/id` / auto-identify |
| `nickserv` | Nick o vacio | Override del servicio en redes no estandar |
| `autoconnect` | `0`/`1` | Conectar al servidor guardado al arrancar |
| `autojoin` | `0`/`1` | Entrar en `channels` tras el registro IRC |
| `channels` | Canales separados por coma | Lista de canales para restaurar sesion |
| `theme` | `1`, `2`, `3` | Tema de color |
| `timestamps` | `0`, `1`, `2` | Off, on, smart |
| `beep` | `0`/`1` | Sonido de mencion |
| `click` | `0`/`1` | Click de tecla |
| `traffic` | `0`/`1` | Mostrar JOIN/PART/QUIT |
| `divider` | `0`/`1` | Separadores de contexto de canal |
| `countsync` | `0`/`1` | Resincronizacion idle de contadores |
| `notif` | `0`/`1` | Notificaciones en fila inferior |
| `nickcolor` | `0`/`1` | Colores por nick |
| `autoaway` | `0`-`60` | Minutos de inactividad, `0` desactiva |
| `tz` | `-12`..`+12` o `rtc` | Zona SNTP; `rtc` es el modo RTC de Classic |
| `tzlast` | `-12`..`+12` | Ultima zona numerica al salir de RTC |
| `friends` | Nicks separados por coma | Hasta cinco amigos monitorizados |
| `ignores` | Nicks separados por coma | Hasta cinco nicks ignorados |

Ajustes destacables:

- `autoconnect=1` conecta al servidor guardado al arrancar.
- `autojoin=1` reenvia `channels=` tras el registro IRC y tras la espera de NickServ si hace falta.
- En Classic, `tz=rtc` usa el RTC local y los valores numéricos usan ESP/SNTP. SpectraNext no expone RTC al Z80: usa UDP/SNTP y, si se selecciona `rtc`, vuelve a `tzlast`.
- `divider=0` oculta futuros separadores de contexto.
- `countsync=0` desactiva refresco idle de contadores.
- `friends=` e `ignores=` admiten hasta cinco nicks cada uno.

Los bookmarks se guardan como `/SYS/CONFIG/SPTBM1.CFG` a `SPTBM5.CFG` en Classic y `/CFG/SPTBM1.CFG` a `SPTBM5.CFG` en SpectraNext.

---

## Límites de SpectraNext

- IRC usa texto plano en el puerto configurado. El cartucho expone TLS solo en
  su servicio fijo del puerto 443, por lo que no hay IRC TLS en el puerto 6697.
- El cartucho no expone RTC al Z80. SpectraNext utiliza UDP/SNTP; `tz=rtc`
  vuelve a la última zona numérica guardada en `tzlast`.
- SpecTalkZX posee un solo socket del cartucho. La sincronización del reloj se
  realiza antes de abrir el socket IRC.
- Los guardados correctos de configuración y bookmarks persisten en XFS. Un
  fallo de E/S o de alimentación durante una sobrescritura no ofrece rollback
  atómico en esta versión.

---

## Compilación

Requisitos: z88dk con soporte SDCC, GNU Make y Python 3.8 o posterior.

```sh
make NO_COLOR=1
make release NO_COLOR=1
make release NO_COLOR=1 PLATFORM=spectranext SPXN_DIR=/ruta/a/SpectraNext/driver
make clean
```

Salidas:

- `build/SpecTalkZX.tap`
- `build/SPECTALK.OVL`
- `build/SPECTALK.DAT`

El instalador de SpectraNext es una compilación aparte de esos mismos
artefactos. Véase
[Publicación del recurso SpectraNext](#publicación-del-recurso-spectranext).

El proyecto utiliza una unity build en C y módulos Z80 escritos a mano. Los
datos generados incluyen strings comprimidos, ayuda, metadatos de overlays,
What's New, la fuente compacta y la animación About/Earth. La versión del
compilador queda registrada en `build/toolchain.version` para reproducibilidad,
pero el proyecto no fija ni rechaza una release concreta de z88dk.

---

## Publicación del recurso SpectraNext

Este apartado es para quien publica la versión. Los usuarios instalan montando
el directorio HTTPS de GitHub Pages; no copian TAP al cartucho y no esperan
al catálogo del autor del cartucho. El Resource Index oficial es publicidad
opcional posterior. El procedimiento de alojamiento es
[Publishing a Resource](https://docs.spectranext.net/publishing/publish-a-resource).

### 1. Compilar y probar los artefactos de release

Compila ambos objetivos desde el mismo fuente y después el instalador de
**release**, **sin** `--force-install`:

```sh
make release NO_COLOR=1 PLATFORM=classic
make release NO_COLOR=1 PLATFORM=spectranext SPXN_DIR=/ruta/a/SpectraNext/driver
/ruta/a/SpectraNext/tools/dev installer packaging/spectranext/installer.json \
  build/spectranext-resource
```

En Windows PowerShell usa `tools\dev.cmd`. El directorio de salida debe estar
vacío o no existir. `--force-install` solo sirve para reinstalar en local la
misma versión; no alojes ese instalador.

La carpeta publicada es un directorio HTTPS plano:

| Fichero | Función |
|---|---|
| `boot.zx` | Entrada remota. BASIC tokenizado. Nunca `%tapein`. |
| `SPECTALK.INS` | Runtime genérico del instalador. |
| `SPECTALK.PKG` | Paquete SPXI con TAP, OVL y DAT. |
| `SPECTALK.SCR` | Pantalla de carga. |
| `package.json` | Nombre, versión, tamaños y SHA-256. |
| `index.txt` | Listado del filesystem HTTP. |
| `index.html` | Solo raíz de GitHub Pages; no va en `index.txt`. |
| `.nojekyll` | Extra de GitHub Pages. |

`SPECTALK.ZX` lo crea el cartucho al instalar. No se aloja. El `boot.zx`
remoto se queda en el servidor; no debe copiarse al slot 0 de XFS, donde
`boot.zx` es el arranque global del dueño del cartucho.

Usa el mismo conjunto TAP/OVL/DAT registrado en [CHANGELOG.md](CHANGELOG.md).
No sustituyas una recompilación posterior de desarrollo.

### 2. Alojar el directorio por HTTPS

Los recursos públicos se sirven desde una raíz `https://` estable. HTTP plano
solo vale para pruebas locales. Un zip de GitHub Release **no** es un recurso
montable.

El alojamiento público de SpecTalkZX es GitHub Pages del repositorio
**estable** (`https://ignaciomonge.github.io/SpecTalkZX/`), no el checkout de
desarrollo. Sube el directorio generado **byte a byte**. El PNG de
previsualización del instalador es local y no forma parte del recurso.

GitHub Pages responde 404 en la URL de un directorio si no hay `index.html`.
El cartucho monta esa URL, así que un `index.html` ausente se convierte en
`No such file or directory` aunque existan `boot.zx` e `index.txt`. Añade un
`index.html` mínimo solo para Pages; no lo pongas en `index.txt`. Comprueba
en el navegador que la raíz HTTPS devuelve HTTP 200 antes de montar.

### 3. Verificar los ficheros alojados en un navegador de escritorio

Abre:

- `https://ignaciomonge.github.io/SpecTalkZX/` (debe ser HTTP 200, no la página 404 de GitHub)
- `https://ignaciomonge.github.io/SpecTalkZX/index.txt`
- `https://ignaciomonge.github.io/SpecTalkZX/boot.zx`
- `https://ignaciomonge.github.io/SpecTalkZX/package.json`

Comprueba el formato del listado, los bytes de `boot.zx` y que cada SHA-256 de
`package.json` coincida con el fichero subido. El contrato de `index.txt` está
en el
[filesystem HTTP(s)](https://docs.spectranext.net/filesystem/https-fs).

### 4. Compartir la raíz HTTPS

Esa raíz de GitHub Pages es lo que monta el cartucho. Los usuarios siguen la
[instalación](#cartucho-spectranext). No les pidas `%tapein` de `boot.zx`. Un
alias tipo TinyURL puede ayudar a teclear; no es la URL canónica.

### 5. Alta opcional posterior en el Resource Index oficial

GitHub Pages basta para la instalación pública. Enviar
`https://ignaciomonge.github.io/SpecTalkZX/` a
[spectranext.net/submit-resource.html](https://spectranext.net/submit-resource.html)
es publicidad opcional del catálogo del autor del cartucho. No retrases una
release por esa revisión. Cambia las notas de usuario solo cuando aparezca.

---

## Solución de problemas

| Problema | Comprobar |
|---|---|
| El indicador Classic permanece rojo | Alimentación, cableado y 115200 baudios del puente ESP-AT |
| No se detecta SpectraNext | Cartucho presente y `/CFG` disponible en XFS local |
| La red está lista pero IRC no conecta | Credenciales Wi-Fi, hostname y puerto IRC en texto plano |
| El arranque se detiene en esxDOS/DAT | divMMC montado y los tres ficheros juntos de la misma build |
| Fallan ayuda/About/bookmarks | Falta `SPECTALK.OVL` o `SPECTALK.DAT`, o pertenece a otra build |
| No empieza la instalación SpectraNext | Monta la raíz HTTPS de GitHub Pages; no uses `%tapein` con `boot.zx` |
| El reloj sigue en `00:00` | Acceso SNTP y zona numérica; Classic también admite `!tz rtc` |
| Falla NickServ | Usa `/id`, `nickpass=` o el override `nickserv=` |
| Demasiados JOIN/PART | Alterna `!traffic` |
| Se desajustan los contadores | Mantén `!countsync` o ejecuta `/names` |
| Los acentos se ven mal | UTF-8 se convierte al juego de caracteres visible del ZX |
| `/reply` no tiene destino | Primero debe recibirse un PM para recordar su remitente |

---

## Licencia

SpecTalkZX es software libre publicado bajo **GNU General Public License
v2.0**.

Incluye código derivado de:

- Driver UART de **Nihirash**.
- Mini fuente **Ikkle-4** de Jack Oatley.

---

## Autor

**M. Ignacio Monge Garcia** — 2025–2026

*Conectando el ZX Spectrum a IRC desde 2025.*
