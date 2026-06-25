<p align="center">
  <a href="images/white_banner.png"><img src="images/white_banner.png" width="600" alt="Banner de SpecTalkZX"></a>
</p>

# SpecTalkZX

**Cliente IRC para ZX Spectrum con WiFi ESP8266**

:gb: [Read in English](README.md)

![Plataforma](https://img.shields.io/badge/Plataforma-ZX%20Spectrum-blue)
![Licencia](https://img.shields.io/badge/Licencia-GPLv2-green)
![Version](https://img.shields.io/badge/Version-1.3.8%20Hermes-orange)

SpecTalkZX 1.3.8: Hermes es el salto mas grande en calidad y madurez desde la primera version publica. Mantiene el objetivo de funcionar en maquinas de clase 48K, pero suma gestor de bookmarks IRC, restauracion de sesiones, modo RTC, parser IRC mas seguro, mejor `/names` y `/list`, renderizado mas suave, carga de overlays mas rapida y un conjunto mucho mayor de assets en `SPECTALK.OVL` y `SPECTALK.DAT`.

---

## Novedades principales de 1.3.8

- **Gestor de bookmarks IRC**: `!bm` abre cinco slots guardados en SD. Permite almacenar, conectar, borrar y marcar un slot para autoconexion/autojoin.
- **Restauracion de sesion**: `!save` guarda servidor, puerto, canales activos y politica de arranque. `!autoconnect` controla la conexion al servidor; `!autojoin` controla la entrada automatica a canales tras el registro IRC.
- **Canales de bookmark aislados**: cada slot guarda su snapshot de canales sin contaminar otros bookmarks.
- **Reloj RTC**: `!tz rtc` puede sembrar el reloj desde RTC local mediante un overlay frio; SNTP sigue disponible para configuraciones ESP8266 normales.
- **Nuevos comandos IRC**: `/mode`, `/reply` y `/notice`. `/0` a `/9` cambian directamente a slots de ventana.
- **Mejor `/names`**: la lista manual usa la zona principal, se pinta como grid de cuatro columnas, pagina/cancela limpiamente y evita que backlog antiguo se mezcle bajo una lista incompleta.
- **Resincronizacion opcional de contadores**: `!countsync` activa refresco ligero de usuarios cuando una sesion larga queda en reposo.
- **Separadores de contexto de canal**: `!divider` activa o desactiva las lineas visuales al cambiar de contexto.
- **Busqueda y `/list` mas legibles**: la salida de busqueda ya no pisa el prompt y limita repeticiones visuales rapidas.
- **NickServ mas robusto**: auto-identify reconoce servicios no estandar como `NiCK`, ademas de `NickServ`.
- **Keepalive durante About**: el `!about` animado procesa PING/PONG y evita desconexiones falsas al estar mucho tiempo en overlay.
- **Renderizado mas rapido**: trozos ASCII seguros en columnas pares usan el renderer rapido de linea 64-col; BPE, wraps, controles y casos delicados conservan la ruta antigua.
- **Carga de overlays mas rapida**: atlas, validacion de cabecera, seek/read y datos generados se han reducido y endurecido.
- **Texto mas robusto**: conversion UTF-8/Latin-1, stripping de formato IRC, salida fallback, captura de prompts y salida de overlays fueron auditadas.
- **Gran endurecimiento interno**: codigo residente, generadores de datos, renderer, parser y recepcion fueron reorganizados y auditados manteniendo el objetivo 48K.

Consulta [CHANGELOG.md](CHANGELOG.md) para el historial detallado, bugs corregidos, prototipos rechazados y metricas finales de build.

---

## Capturas

<p align="center">
  <a href="images/snapshot-01.png"><img src="images/snapshot-01.png" width="250" alt="Captura SpecTalkZX 1.3.8 1"></a>
  <a href="images/snapshot-02.png"><img src="images/snapshot-02.png" width="250" alt="Captura SpecTalkZX 1.3.8 2"></a>
  <a href="images/snapshot-03.png"><img src="images/snapshot-03.png" width="250" alt="Captura SpecTalkZX 1.3.8 3"></a>
</p>
<p align="center">
  <a href="images/snapshot-04.png"><img src="images/snapshot-04.png" width="250" alt="Captura SpecTalkZX 1.3.8 4"></a>
  <a href="images/snapshot-05.png"><img src="images/snapshot-05.png" width="250" alt="Captura SpecTalkZX 1.3.8 5"></a>
  <a href="images/snapshot-06.png"><img src="images/snapshot-06.png" width="250" alt="Captura SpecTalkZX 1.3.8 6"></a>
</p>
<p align="center">
  <a href="images/snapshot-07.png"><img src="images/snapshot-07.png" width="250" alt="Captura SpecTalkZX 1.3.8 7"></a>
  <a href="images/snapshot-08.png"><img src="images/snapshot-08.png" width="250" alt="Captura SpecTalkZX 1.3.8 8"></a>
  <a href="images/snapshot-09.png"><img src="images/snapshot-09.png" width="250" alt="Captura SpecTalkZX 1.3.8 9"></a>
</p>
<p align="center">
  <a href="images/snapshot-10.png"><img src="images/snapshot-10.png" width="250" alt="Captura SpecTalkZX 1.3.8 10"></a>
  <a href="images/snapshot-11.png"><img src="images/snapshot-11.png" width="250" alt="Captura SpecTalkZX 1.3.8 11"></a>
  <a href="images/snapshot-12.png"><img src="images/snapshot-12.png" width="250" alt="Captura SpecTalkZX 1.3.8 12"></a>
</p>

---

## Requisitos

| Componente | Requisito |
|------------|-----------|
| Ordenador | ZX Spectrum 48K, 128K, +2, +2A, +3 o compatible |
| Almacenamiento | divMMC/esxDOS con tarjeta SD |
| Red | ESP8266 o puente WiFi compatible con ESP-AT |
| UART | Ruta UART hardware soportada a 115200 baud |
| Ficheros | `SpecTalkZX.tap`, `SPECTALK.OVL` y `SPECTALK.DAT` juntos |

Configura el ESP8266 a **115200 baud** antes de usarlo. Las credenciales WiFi se suelen preparar con [NetManZX](https://github.com/IgnacioMonge/NetManZX) o una herramienta ESP-AT equivalente.

---

## Instalacion

1. Descarga el archivo de la pagina de [Releases](https://github.com/IgnacioMonge/SpecTalkZX/releases).
2. Copia estos tres ficheros al mismo directorio de la tarjeta SD:
   - `SpecTalkZX.tap` - cargador/programa.
   - `SPECTALK.OVL` - overlays de ayuda, About, bookmarks, RTC, config, status, comandos locales y pantallas relacionadas.
   - `SPECTALK.DAT` - fuente, strings comprimidos, ayuda, temas, What's New y assets de About/Earth.
3. Carga `SpecTalkZX.tap` en el Spectrum.
4. Espera `WiFi:OK` en la barra de estado y conecta a IRC.

`SPECTALK.DAT` y `SPECTALK.OVL` son obligatorios. Si faltan o no coinciden con la version, pueden fallar ayuda, About, config, bookmarks u otros overlays.

---

## Inicio rapido

```text
/nick TuNick
/server irc.libera.chat 6667
/join #spectrum
```

Configuracion inicial util:

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
- **Cambio directo** con `/0` a `/9`.
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
| `!init` | `!i` | Reinicia WiFi/ESP |
| `!config` | `!cfg` | Muestra todos los ajustes |
| `!theme N` | | Cambia tema `1`, `2` o `3` |
| `!about` | | Pantalla About animada |
| `!changelog` | | Pantalla What's New |
| `!bm` / `!bookmarks` | | Gestor de bookmarks IRC |
| `!save` | `!sv` | Guarda configuracion y sesion |
| `!autoconnect` | `!ac` | Activa conexion automatica al servidor |
| `!autojoin` | | Activa entrada automatica a canales guardados |
| `!tz` | | Muestra/fija zona horaria; acepta offset numerico y `rtc` |
| `!timestamps` | `!ts` | Cicla timestamps off/on/smart |
| `!notif` | `!nf` | Activa notificaciones |
| `!beep` | | Activa sonido de mencion |
| `!click` | | Activa click de tecla |
| `!traffic` | | Activa ruido JOIN/PART/QUIT |
| `!divider` | | Activa separadores de contexto |
| `!countsync` | `!cs` | Activa resincronizacion de contadores |
| `!autoaway` | `!aa` | Auto-away tras N minutos, `0` desactiva |
| `!friend` | | Lista o alterna amigos monitorizados |
| `!nickcolor` | `!nc` | Activa colores por nick |
| `!clear` | `!cls` | Limpia el area de chat |

### Comandos IRC

| Comando | Alias | Descripcion |
|---------|-------|-------------|
| `/server host [puerto]` | `/connect` | Conecta a servidor |
| `/nick [nombre]` | | Muestra o cambia nick |
| `/pass [password]` | | Muestra, fija o limpia password de servidor |
| `/id [password]` | | Identifica con NickServ o servicio detectado |
| `/join #canal` | `/j` | Entra a un canal |
| `/part [mensaje]` | `/p` | Sale del canal actual |
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
| `/who patron` | | Busca usuarios |
| `/list patron` | `/ls` | Lista canales |
| `/names` | | Grid paginado de usuarios del canal |
| `/topic [texto]` | | Muestra o fija topic |
| `/mode [args]` | | Muestra o fija modos |
| `/search patron` | | Busca en resultados de list/who |
| `/ignore [nick]` | | Lista, anade o quita ignorados (`-nick`) |
| `/kick nick [razon]` | `/k` | Expulsa del canal actual |
| `/channels` | `/w` | Lista ventanas abiertas |
| `/0`..`/9` | | Cambia a slot fisico de ventana |

---

## Configuracion

SpecTalkZX carga `SPECTALK.CFG` desde `/SYS/CONFIG/` o `/SYS/` y puede escribirlo con `!save`. El repositorio incluye [`SPECTALK.CFG.example`](SPECTALK.CFG.example) con todas las claves soportadas.

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
| `tz` | `-12`..`+14` o `rtc` | Zona SNTP o modo RTC local |
| `tzlast` | `-12`..`+14` | Ultima zona numerica al salir de RTC |
| `friends` | Nicks separados por coma | Hasta cinco amigos monitorizados |
| `ignores` | Nicks separados por coma | Hasta cinco nicks ignorados |

Ajustes destacables:

- `autoconnect=1` conecta al servidor guardado al arrancar.
- `autojoin=1` reenvia `channels=` tras el registro IRC y tras la espera de NickServ si hace falta.
- `tz=rtc` usa RTC local; los valores numericos usan hora ESP/SNTP.
- `divider=0` oculta futuros separadores de contexto.
- `countsync=0` desactiva refresco idle de contadores.
- `friends=` e `ignores=` admiten hasta cinco nicks cada uno.

Los bookmarks se guardan aparte como `/SYS/CONFIG/SPTBM1.CFG` hasta `SPTBM5.CFG`.

---

## Compilacion

Requisitos: z88dk con soporte SDCC, GNU Make y Python 3.8 o superior.

```sh
make NO_COLOR=1
make release NO_COLOR=1
make clean
```

Salidas:

- `build/SpecTalkZX.tap`
- `build/SPECTALK.OVL`
- `build/SPECTALK.DAT`

El proyecto usa unity build en C y modulos Z80 escritos a mano. Los datos generados incluyen strings comprimidos, ayuda, metadatos de overlays, What's New, fuente compacta y assets de About/Earth.

---

## Solucion de problemas

| Problema | Comprobar |
|----------|-----------|
| LED rojo permanente | Cableado, alimentacion y 115200 baud del ESP8266 |
| LED amarillo pero no conecta | Credenciales WiFi y servidor/puerto |
| Halt de esxDOS/DAT al arrancar | divMMC presente, SD montada, `SPECTALK.DAT` junto al TAP |
| Fallan ayuda/About/bookmarks | `SPECTALK.OVL` falta o es de otra version |
| Reloj en `00:00` | Zona horaria, SNTP o configuracion `!tz rtc` |
| Falla NickServ | Usa `/id password`, `nickpass=` o `nickserv=` |
| Demasiados JOIN/PART | `!traffic` |
| Contadores de canal se desajustan | Mantener `!countsync` o ejecutar `/names` |
| Acentos raros | UTF-8 se convierte al set visible del ZX |
| `/reply` no envia | Necesita haber recibido antes un PM |

---

## Licencia

SpecTalkZX es software libre publicado bajo **GNU General Public License v2.0**.

Incluye codigo derivado de:

- Cliente IRC **BitchZX**.
- Driver UART de **Nihirash**.
- Mini fuente **Ikkle-4** de Jack Oatley.

---

## Autor

**M. Ignacio Monge Garcia** - 2025-2026

*Conectando el ZX Spectrum a IRC desde 2025.*
