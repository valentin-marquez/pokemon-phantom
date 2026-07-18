# Fear & Hunger como referencia narrativa para Pokémon Phantom

> Informe de estudio (jul 2026). Producido por investigación multi-fuente (entrevistas a Miro Haverinen, wikis, análisis críticos, recepción) + verificación técnica contra este repo. Sirve de base para el documento de diseño narrativo del hack.

---

## 1. Por qué funciona Fear & Hunger

Fear & Hunger no funciona por su gore ni por su dificultad: funciona porque **cada sistema del juego enuncia la misma tesis**. Guardar afirma "el descanso no es seguro"; abrir un cofre afirma "la curiosidad se cobra"; querer a alguien afirma "amar algo aquí es darle al mundo un rehén". Miro Haverinen (un solo developer, en RPG Maker, un motor tan limitado como pokeemerald) buscaba deliberadamente "relentless darkness" y "different ways to evoke hopelessness", y su brújula editorial era visceral: *"cada vez que me sentía mal haciendo algo, supe que iba por buen camino"* — pero recortaba todo lo que caía en "territorio de meme". Su filtro no era la intensidad, era la **seriedad**.

Los pilares, destilados:

1. **La violencia se sufre, nunca se ejerce como poder del jugador.** ("Endured, never inflicted" — Hypercritic.) El jugador es testigo o víctima; jamás verdugo recompensado.
2. **Frialdad total de presentación.** Sin música épica, sin cámara lenta, sin subrayado. El narrador describe una atrocidad con la misma voz plana con que describe una hierba. La indiferencia tonal ES el terror.
3. **El horror social precede al sobrenatural.** Prehevil ya era un pueblo represivo y cruel antes del festival ("The gallows become familiar to every nonconformist"); el horror cósmico solo literaliza lo que ya existía. Exactamente la tesis de Sombraluna.
4. **El lore es fragmentario, falible y opcional**; la historia emocional es autosuficiente. Todo lo escrito lo escribieron humanos que pueden mentir. Explicar todo es "un mago revelando sus trucos".
5. **Las revelaciones recontextualizan, no informan.** El final de la Niña de F&H1 no añade datos: reordena lo que ya hiciste. Y contra ese fondo de crueldad, la bondad mínima (una mano tomada un segundo) brilla desproporcionadamente. El mensaje final, según sus mejores análisis, es que "el espíritu humano es fuerte".

### 1.1 El método del autor

Dos prácticas de Haverinen directamente robables:

- **Anclaje documental real:** Prehevil se construyó sobre fotos reales de Praga en la 2ª Guerra Mundial ("something about Prague just appealed to me"), y luego se despegó de la historia — verosímil, no histórico. Para Sombraluna: construir sobre fotografías de un pueblo costero real (caletas chilenas, conserveras abandonadas del sur) para tiles, oficios, apellidos y cartelería.
- **La imagen onírica autónoma:** sus mejores set-pieces "came to me in different dreams" — la cena del alcalde, la iglesia, el apartamento de moho nacieron como imágenes irracionales que luego cosió al mundo, no de checklists del género. Reservar 1-2 escenas de Phantom para una imagen personal del autor: la que incomode sin saber por qué.

Todo esto es disciplina de diseño, no tecnología. Es replicable al 100% en GBA.

---

## 2. Catálogo de técnicas narrativas (con traducción a pokeemerald)

### 2.1 El narrador clínico en segunda persona
**Qué es:** todo texto de sistema habla de "tú", presente, con precisión sensorial y cero afecto. *"You feel a terrible presence entering the dungeons."*
**Traducción:** todos los textos de examinar (`msgbox` sin hablante): "Sientes el olor antes que la imagen.", "La sal se ha comido la madera. Y algo más." Es gratis y sustituye los tilesets gore que no tenemos. Los códigos de control del `charmap.txt` dan ritmo al frame: `PAUSE` (pausa a mitad de frase), `PAUSE_UNTIL_PRESS`, `PLAY_SE` (un sonido disparado en la palabra exacta) y `PAUSE_MUSIC` (silenciar la BGM a mitad de diálogo). "Lo colgamos{PAUSE 60} porque tenía hambre."

### 2.2 Textos anónimos incrustados en el escenario
**Qué es:** los "Unnamed Books" de Termina — graffiti, diarios, actas sin dueño claro que reconstruyen el pueblo ANTES del horror. El diario de la Filthy Shack establece que Prehevil ya era cruel antes del festival.
**Traducción:** 4-6 textos repartidos en signposts/objetos: el diario de alguien que huyó de la isla, un acta del consejo sobre "racionamiento", un graffiti en el muelle, un sermón. Deben **contradecirse deliberadamente** entre sí (versiones distintas de la hambruna fundacional) y jamás debe existir la versión objetiva. Cobrar un precio por leerlos: un NPC que se acerca mientras lees, o que leer consuma la franja horaria (ver sección 3).

### 2.3 El horror como mobiliario: narrativa ambiental sin texto
**Qué es:** en F&H "los instrumentos de tortura están dispersos **como decoración**, no como set-pieces señalizadas". Cadáveres y objetos son micro-relatos mudos.
**Traducción:** presupuesto de composiciones de metatiles que cuentan solas: una mesa puesta para tres con una silla quemada; jaulas pequeñas y vacías tras la conservera, todas con la puerta doblada hacia afuera; arañazos a la altura de un Meowth en la puerta del almacén de comida; una barca varada con un solo remo. Regla: la mitad de estas composiciones **sin texto de examinar** — el metatile es el relato; la otra mitad, una sola línea clínica (2.1). Es la técnica de menor coste por impacto de todo el catálogo.

### 2.4 Reticencia: el NPC que calla
**Qué es:** Levi responde "..." e "It's nothing", y cuenta que a un niño le clavaron la mano por robar caramelos **como anécdota administrativa**. Nótese: F&H ya usa el tropo exacto del pitch (castigo brutal desproporcionado por robar comida, narrado sin afecto).
**Traducción:** los habitantes de Sombraluna hablan de la ejecución del Meowth como trámite: "Robó. Se resolvió.", "...", "No es asunto tuyo, forastero." Cero justificación, cero disculpa, cero escándalo. El registro "explicativo" está prohibido en todo el guion.

### 2.5 La cortesía como amenaza
**Qué es:** el Gentleman te invita a cenar a la luz de velas en plena masacre; responder mal a sus preguntas sobre bellas artes = muerte. Pocketcat compra niños y jamás dice para qué; lo comunica un gesto físico repugnante.
**Traducción:** una escena de hospitalidad ritual en la casona del jefe del pueblo: multichoice de conversación donde las respuestas "incorrectas" enfrían visiblemente el diálogo (y suben una variable oculta). Lo predatorio se comunica con un gesto (un `applymovement`: el anfitrión se detiene, mira demasiado tiempo al Pokémon del jugador), nunca con texto.

### 2.6 El heraldo que nadie cree (modelo Per'kele)
**Qué es:** Per'kele aparece en sueños desde el minuto uno, **enuncia las reglas del festival con burla condescendiente y se niega a explicar el porqué** ("The time for words has come and gone. Show me violence."); nadie le cree, y el reloj hace el resto.
**Traducción:** un NPC descartable en apariencia (el borracho del muelle, la vieja que habla sola) que en el Día 1 enuncia las reglas reales del juego como delirio: "Los forasteros no se van. Nunca se han ido." / "Cuando la luna esté mala, no duermas en la pensión." Todo lo que dice es literalmente cierto y el guion jamás lo confirma; sus diálogos cambian por acto (`compare VAR_PHANTOM_TIME`). Coste: un object event y cuatro cajas de texto. Regla anexa: **el juego acumula dread al principio y descarga explicación al final — nunca al revés**; el heraldo es dread, no exposición.

### 2.7 La anormalidad aceptada con naturalidad
**Qué es:** en Prehevil los aldeanos comentan que "ha llegado un alcalde nuevo" pese a que no hubo elecciones, y nadie lo cuestiona: **la anormalidad asumida como trámite ES el horror**, más barato que cualquier monstruo.
**Traducción:** en los Días 2-3, los habitantes comentan lo imposible sin extrañeza: "La campana sonó sola anoche. Ya era hora." / "La casa de la del muelle siempre estuvo vacía." (contradiciendo lo que el jugador vio en el Día 1). Cero código nuevo: variantes de diálogo por flag. Combina con la reticencia de 2.4: el pueblo no calla solo sus crímenes; también normaliza sus milagros.

### 2.8 El stinger de presencia: texto + sonido, sin imagen
**Qué es:** "A terrifying presence has entered the room" + SFX cuando el Crow Mauler entra a tu mapa. No lo ves; sabes que está.
**Traducción:** en `on_transition` o al pisar un trigger: `playse SE_CUSTOM` + `msgbox` ("Algo ha entrado al pueblo contigo."). Y el arma fina: `PlaySE12WithPanning(songNum, pan)` (`include/sound.h:41`) con pan -64/+63 — el golpe suena a la izquierda, el jugador mira a la izquierda. El Breedmare debe *sonar* desde el lado por el que viene antes de verse.

### 2.9 La voz del monstruo: el cry corrupto
**Qué es:** los enemigos de F&H tienen vocalizaciones fuera de combate que se oyen antes de verlos.
**Traducción:** el motor casi lo trae: `PlayCry_ByMode(species, pan, mode)` con `CRY_MODE_GROWL_1` o `CRY_MODE_ECHO_START` reproduce el cry **al revés**; `CRY_MODE_WEAK`/`CRY_MODE_FAINT` lo degradan. Ojo (verificado): `PlayCryInternal(species, pan, volume, priority, mode)` NO expone pitch como parámetro — el pitch lo fija cada `CRY_MODE_*` internamente (`src/sound.c`, default 15360). Para la voz del Breedmare (reversa + pitch muy grave) hay que **añadir un CRY_MODE propio** en el switch de `PlayCryInternal` (~10 líneas: `reverse = TRUE; pitch = <valor bajo>;`). Sigue siendo barato. En scripts: `playmoncry` (`asm/macros/event.inc`). El cry de Ditto invertido y grave es la voz del Breedmare, y es el truco Lavender Town canónico.

### 2.10 Paleta como narrativa: un mapa, dos mundos
**Qué es:** Ma'habre presente (ruina mugrienta) vs. Ma'habre pasado (misma ciudad, dorada, viva): mismo mapa, otra paleta, otra música.
**Traducción:** `TintPalette_SepiaTone` / `TintPalette_GrayScale` ya existen (`src/palette.c:852-891`); `BlendPalettes(sel, coeff, color)` (`src/palette.c:832`) para teñir hacia un verde enfermizo `RGB(4,10,4)` (la "luna mala") o hacia calidez. Hookear la carga de paletas de overworld (patrón día/noche de los DNS de ROM hacking) y la isla entera cambia de estado sin tocar un tileset. Es la forma más barata para un dev solo de duplicar contenido: el pueblo del acto 1 y el del acto 3 son el mismo mapa.

### 2.11 Oscuridad, grano y distorsión
**Traducción directa, todo verificado:** el sistema de Flash (`SetFlashLevel` — declarado en `include/overworld.h:99`, definido en `src/overworld.c:981`; `animateflash` en scripts) es literalmente el círculo de visión de F&H — forzarlo en interiores nocturnos. `setweather` con `WEATHER_FOG_HORIZONTAL` (niebla Silent Hill), `WEATHER_VOLCANIC_ASH` (ceniza = nieve sucia de isla), `WEATHER_SHADE`. Para irrealidad: `ScanlineEffect_InitWave` (ondulación tipo espejismo en visiones) y `REG_MOSAIC` vía `SetGpuReg` + `BGCNT_MOSAIC` (precedente en `src/battle_intro.c`) para pixelar/despixelar reveals. `SetCameraPanning` para temblores.

### 2.12 Revelación en dos tiempos: silueta → estampa
**Qué es:** en overworld el monstruo de F&H es un sprite pequeño y ambiguo; el horror detallado vive en el battle sprite grande dibujado a mano. La imaginación del jugador rellena el hueco.
**Traducción:** el Breedmare en overworld es una silueta rara de 16x32; el detalle grotesco vive en las 4-6 estampas fullscreen (240x160, BG de 256 colores en un CB2 propio, mismo patrón que la intro del juego y que nuestro `minigame_spaceship.c`, que ya monta 4 BGs con INCBIN). Entrada/salida de estampas con mosaico. Las estampas son nuestros battle sprites de F&H: ahí va lo que el tileset no puede decir.

### 2.13 Guardado escaso con teatro de agencia
**Qué es:** en F&H solo se guarda en camas, muchas con coin flip cuya elección **no afecta nada** (el resultado está predeterminado; el input solo elige la animación). Termina lo refina: dormir guarda pero avanza el reloj del mundo.
**Traducción:** quitar SAVE del menú (`src/start_menu.c`, sacar la entrada del array — minutos de trabajo). 6-8 puntos de guardado diegéticos (camas, el altar), 2-3 "seguros de verdad" marcados con la ÚNICA música cálida del juego (patrón Save Room Theme). Coin flip cara/cruz (`multichoice` + `random 2`, ignorando la elección) solo para consecuencias parciales: pesadilla, visita nocturna, +culpa. **Nunca muerte por 50/50** (ver sección 4).

### 2.14 La UI como voz del horror
**Traducción:** editar strings del sistema (`src/strings.c`, `src/battle_message.c`): "¡Un POKÉMON salvaje apareció!" → "Algo se acerca."; el texto del punto de guardado que en el acto 3 cambia a "DESCANSAR... ¿para qué?" (el equivalente barato del skill "Suicide" que aparece solo en el menú de F&H). Corromper también jingles de victoria y fanfarrias en los momentos clave: cada sonido alegre de Emerald que sobreviva intacto en el acto 3 trabaja en contra (lección Snakewood) — o a favor si se rompe a propósito.

### 2.15 La derrota como contenido
**Qué es:** los game overs de F&H tienen arte y texto específicos de CÓMO moriste; los post-game-over states (jugar ciego, arrastrarte desollado) son su truco más cruel.
**Traducción:** interceptar el whiteout (hook en `Overworld_ResetStateAfterWhiteOut` / chequeo de `B_OUTCOME`) para que perder ante el Breedmare no sea "ojos en blanco → Centro Pokémon" sino escena: estampa + texto de qué te pasó + despertar en otro sitio. Reservar 1-2 estampas del presupuesto para derrotas. Y marcar 2-3 de los 5-10 combates como imposibles: huir es la respuesta correcta, y el juego nunca te debe una pelea justa.

### 2.16 Sin encuentros aleatorios: toda amenaza se ve (o se oye) venir
**Qué es:** regla estructural de F&H: los enemigos son visibles en el mapa y te persiguen — no hay encuentros aleatorios; ves venir la amenaza y decides.
**Traducción:** **tasas de encuentro a cero en toda la isla** (o mapas sin datos de wild encounters); todo combate es un object event visible o un trigger scripteado, siempre precedido por su firma sensorial (2.8-2.9). La hierba alta que no produce nada es además un microterror propio de un hack Pokémon: el jugador espera el encuentro y el silencio del sistema lo inquieta. Frase-tema: "aquí nada te ataca por azar; todo lo que te ataca te eligió".

### 2.17 Audio: 90% ambient, silencio activo, un solo tema cálido
**Traducción:** drones graves como "instrumento" m4a (sample DirectSound largo en loop en el voicegroup, MIDI de 2 notas) o strings detuned. Cada mapa elige conscientemente entre ambient / solo SFX en loop / silencio total. El stinger más barato: `FadeOutBGM` a silencio + `playse` de un pitido agudo. Tres pistas de batalla (normal/única/jefe) para que el jugador aprenda el código de amenaza por el oído.
**La pista nocturna única (Termina):** de día cada distrito tiene música propia ("Ashes of War", "Remaining Routine"...); de noche **una sola pista sustituye a todas: la identidad sonora del distrito desaparece**. Traducción: cada zona con su ambient diurno; a partir de cierta franja, `on_transition` ignora la música del map header y fuerza en TODOS los mapas la misma pista muerta (o silencio + un único SFX en loop) según `VAR_PHANTOM_TIME` (`playbgm`/`savebgm`). El jugador percibe que el pueblo "se apagó" sin que nadie se lo diga. Coste: una pista y dos líneas por mapa.

---

## 3. El molde Termina → Sombraluna

Termina es el molde correcto porque es exactamente nuestro formato: **un solo pueblo aislado + bosque + subsuelo, ciclo cerrado, y el terror no viene del tamaño del mapa sino de que el tiempo corre y el mundo actúa sin ti.**

### El reloj
Termina divide el juego en 9 franjas (3 días × mañana/tarde/noche) y el tiempo **solo avanza cuando duermes**. Traducción exacta: una variable `VAR_PHANTOM_TIME` (0-8) que se incrementa al dormir en camas, y scripts `ON_TRANSITION` que colocan/quitan NPCs por flags según su valor. No se necesita RTC. Dormir = guardar + curar + **el mundo empeora un paso**: cada cama es una decisión con culpa. Para un juego lineal de 4-5 h, la versión sana es un híbrido: los "días" son actos narrativos y dormir es la transición de acto — se conserva la estructura dramática de Termina sin su presión de triage (que en un juego lineal solo generaría FOMO).

### NPCs con agenda
En Termina, 13 NPCs móviles cargan toda la ilusión de mundo vivo sobre un pueblo estático: Caligura mata a Levi y a Henryk **sin ti**; Tanaka busca una silla de ruedas **para Olivia**; August se suicida la noche del día 3. La culpa más fuerte del juego no viene de elecciones binarias sino de **volver a una zona y encontrar el cadáver de alguien que estaba vivo la franja anterior**. Traducción: de los ~200 habitantes del pitch, elegir **8-12 con nombre y tabla de posición por franja** (el jefe del pueblo, el verdugo, la posadera, el niño, el pescador, el cura, la mujer que quiso irse...). El resto es decorado que va desapareciendo. Hacer que 2-3 eventos ocurran entre ellos sin el jugador: si no hablaste con la mujer del muelle antes de dormir la segunda vez, en el acto 3 su casa está vacía y hay una barca rota en la playa. Sin pantalla de decisión: solo la ausencia.

### El plano de Sombraluna: deja que el jugador VEA el sistema
El ítem **Prehevil map** muestra la posición de los contestants en tiempo real, y "vende la ilusión de mundo vivo mejor que cualquier diálogo". Traducción: un key item "Plano de Sombraluna" (regalo del niño del Día 1, con dibujos infantiles) que al usarse lista a los 8-12 habitantes con nombre y su paradero en la franja actual — pura lectura de `VAR_PHANTOM_TIME` + flags con `msgbox` condicionales: "El pescador: en el muelle." → Día 2: "El pescador: no se le ha visto desde anoche." En el acto 3 el plano se convierte en **registro de bajas** y en el epílogo, en la guía del tour de lo que no pudiste salvar.

### El estado final como spoiler psicológico — y su progresión por etapas
El moonscorch convierte a cada personaje en su miedo literalizado (Tanaka, el oficinista culposo → Judgement). Traducción: en el acto final, cada habitante con nombre reaparece transformado/roto/muerto **en un lugar significativo para él y de una forma que revela su secreto**. El verdugo del Meowth terminando como lo que castigaba. El epílogo como tour de lo que el jugador no pudo (ni podía) salvar. Robar también la imagen más barata y potente de Termina: el museo de NPCs que repiten en bucle los gestos de conversaciones que ya no existen — abren la boca, asienten, sin texto. 100% replicable con movement scripts y cero diálogo.

**Por etapas, no de golpe:** Haverinen describe la transformación como reloj visible — "primero distorsiona levemente los rasgos, luego la piel se pela, luego esa carne se endurece". Día 2: cada habitante con nombre muestra su "distorsión leve" — un tic nuevo en el `applymovement` (se rasca, se detiene a mitad de camino, mira fijo un segundo de más), una línea alterada, y en 2-3 casos un sprite levemente mal (paleta un tono más gris). Mapeo gratis en vanilla: **`OBJ_EVENT_GFX_VAR_0..F`** (`include/constants/event_objects.h:262`) — el graphics id del object event se lee de una variable, así que el mismo NPC cambia de sprite según `VAR_PHANTOM_TIME` sin duplicar events ni mapas.

### Distritos y candados
Estructurar la isla en 5-6 zonas con función social legible y un secreto cada una: **puerto** (llegada, el anuncio), **plaza/mercado** (la ejecución), **casona del jefe** (la cena ritual), **viviendas/conservera** (los textos anónimos, la herida histórica), **bosque** (capas progresivas, estilo Deep→Deepest Woods), **santuario** (final), más un **doble subterráneo** (sótanos/cuevas conectados) que es la ruta sucia. Dosificar con candados redundantes al estilo Old Town Gate — cada zona cerrada con **tres** entradas posibles: la llave narrativa, la ruta por el subsuelo, y la **fuerza bruta con coste** (la Old Town Gate se abre a escopetazos gastando munición): forzar la puerta de la conservera hace ruido (esa noche el Breedmare visita tu mapa: flag caliente), romper la ventana deja un rastro que un NPC comenta al día siguiente (+`VAR_PHANTOM_GUILT`), o cuesta el único objeto-herramienta del juego. Convierte los candados en decisiones morales y no en fetch quests. Hace que un juego lineal se *sienta* explorable.

### La herida previa
Prehevil funciona porque ya estaba roto antes del festival (guerra, ocupación, pobreza). Sombraluna necesita su herida fundacional — una hambruna, un naufragio, el invierno que obligó a "decidir quién comía" — reconstruible solo por los textos anónimos contradictorios. El utilitarismo del pueblo debe tener lógica interna visible: verdugos que también son víctimas, vecinos que también tienen miedo. La brutalidad se siente "ganada" cuando el mundo es coherentemente amoral con todos, no solo contigo.

### El refugio-trampa
Termina da dos refugios (el tren, el club de jazz) y ambos traicionan: Henryk envenena la comida del PRHVL Bop la mañana del día 3. El Crow Mauler corrompe la cama que custodiaba. Es el golpe más barato de programar y más recordado: dar al jugador UNA casa segura con la única música cálida del juego... y quemarla en el acto 3 (la cama de la pensión pasa a exigir coin flip; perderlo = el Breedmare estuvo ahí).

### El Breedmare como estado del mundo (modelo Crow Mauler + stalkers de Termina)
No un chase scripteado único: un **estado**. Flags por acto; en mapas "calientes", `on_transition`/`on_frame` decide si entra por una salida, siempre precedido por su firma sonora (cry invertido panned + mensaje de presencia). Persecución con movement scripts (no se necesita pathfinding: pasillos + teleport entre mapas venden la continuidad). Se puede **retrasar, no matar**: cerrar puertas con `setmetatile`, tirar comida (mapea las trampas de oso). Tocarte = combate scripteado imposible cuyo desenlace es escena, no game over.

---

## 4. Qué NO imitar

1. **Violencia sexual: fuera, sin matices.** Es lo más criticado de F&H1 ("trivialización", "shock juvenil", game overs explícitos sin elipsis); el propio Haverinen la recortó drásticamente en Termina y la recepción mejoró exactamente en ese eje. En una IP infantil se leería como profanación edgy garantizada. El horror del Breedmare (experimentos, deformidad, cría forzada **implícita**) es devastador con elipsis y texto clínico; explícito sería Snakewood.
2. **El castigo que expulsa.** La queja n.º 1 de F&H1: "¿cómo aprendes del error si el castigo es empezar de cero?". Nada de coin flips letales, permadeath, ni pérdida de progreso. F&H puede matarte al 50/50 porque la muerte es su loop de aprendizaje; nuestro juego es lineal y narrativo — ahí sería rabia contra el diseñador. Checkpoints inmediatos e invisibles en toda persecución: la tensión debe venir de la ficción, no de la amenaza de repetir 40 minutos. (Contraejemplo exitoso: Look Outside — conservó el horror corporal, eliminó el RNG punitivo.)
3. **Opacidad mecánica ≠ opacidad narrativa.** La niebla tipo Souls funciona en el LORE (libros, ambientes, contradicciones); las mecánicas ocultas que exigen wiki, no. Ser generoso y claro con todo lo jugable; elíptico solo con la historia. (Excepción deliberada: 1-2 secretos mecánicos nunca explicados, estilo lucky coin, para que la comunidad los descubra y difunda.)
4. **Los errores de Snakewood, el hack de terror canónicamente fallido:** música alegre de RSE intacta bajo un guion "opresivo" (incoherencia audiovisual letal), humor de cuarta pared, referencias internas, escalada absurda, cero contención y cero timing. Su destino: jugarse como comedia involuntaria. La coherencia audiovisual es total o no es.
5. **Copiar la superficie, no el sistema.** Los imitadores de F&H copian gore, dioses crueles y coin flips; casi nunca copian la frialdad de presentación, la coherencia del mundo y la humanidad de las víctimas. F&H triunfa *a pesar* de su jank y su exceso, no gracias a ellos.
6. **Descartar sin culpa** (no caben en 4-5 h y su carga temática queda cubierta): sistema de extremidades en combate, medidores de hambre/cordura en tiempo real, loop roguelike, party humana reclutable. Y jamás dar al jugador la crueldad como poder recompensado: si el juego permite un acto cruel, debe costar, nunca lucir.
7. **No sobreexponer la mejor carta.** El giro Breedmare-protector es la estructura exacta que hace grande a F&H (bondad pequeña contra fondo cruel). Cada aparición "casi comprensible" del monstruo antes de tiempo se lo gasta. Ambigüedad hasta el santuario.

---

## 5. Consejos de trama concretos para Phantom

**1. Estructura: tres días numerados + un amanecer, avance por sueño.** Rotular los actos como días en pantalla ("Día primero en Sombraluna" sobre negro — idea propia inspirada en la estructura de franjas de Termina, no un rasgo documentado de su presentación). `VAR_PHANTOM_TIME` avanza solo al dormir en la pensión; cada día tiene paleta, música y tabla de NPCs propia vía `ON_TRANSITION`. Día 1: normalidad banal y la ejecución. Día 2: el trabajo colapsa. Día 3: huida y persecución. Amanecer: santuario. Dormir es la transición: "¿Te acuestas? Mañana empieza el trabajo." — y el jugador aprende pronto que dormir cambia el pueblo.

**2. Los primeros 20-30 minutos son inversión, no relleno.** Modelo tren de Termina: presentar a 6-8 habitantes amables con detalles humanos específicos (la posadera que te guarda el plato, el niño que te enseña su máquina de juegos, el verdugo —aún sin saberlo tú— que te regala una Baya). Mantener los jingles y la superficie Pokémon intactos: el terror Pokémon funciona por corrupción del material querido, no por sustitución (lección Lavender Town). **La ejecución del Meowth cierra el Día 1**, dentro de esos primeros 20-30 min de juego: sin música de aviso (cortar la BGM con `FadeOutBGM` un momento antes), sprites normales, la campana del pueblo, y la caja de mensajes en tono de acta: "Se le encontró con comida ajena. El consejo resolvió. Se resolvió." Fundido a negro, UN solo SE. Sin estampa: guardarla. Que el que te regaló la Baya sea quien sostiene la cuerda. Después, todos vuelven a ser amables. Eso recalibra al jugador para el resto del juego.

**3. Colapsar el objetivo a mitad, modelo Le'garde.** El anuncio de trabajo debe morir en el Día 2: el jugador encuentra la habitación (o la tumba, o el diario) del **anterior contratado** — el anuncio se publica cada cierto tiempo, y los forasteros que responden no se van de la isla. El "trabajo" era siempre el mismo: traer carne nueva (para el pueblo, para los experimentos de los que salió el Breedmare — no aclararlo del todo; dejar que dos textos se contradigan). El objetivo pasa de "trabajar" a "huir" justo cuando el jugador creía entender el juego.

**4. Sembrar 3-4 intervenciones ambiguas del Breedmare ANTES del santuario** (modelo la Niña de F&H1: la revelación debe recontextualizar, no informar). Concretas: (a) el puente al acantilado aparece destrozado la noche que ibas a cruzarlo — luego sabrás que cruzar era morir; (b) una Baya/objeto aparece junto a tu cama tras una escena dura, y el pueblo lo niega ("aquí nadie te dejó nada"); (c) un aldeano que te seguía de noche aparece al día siguiente "enfermo" y no vuelve a mirarte; (d) en una persecución, el Breedmare cierra el paso hacia una ruta — que resultará ser la del matadero. Todas leídas en su momento como amenaza. En el santuario, un flashback barato (mismos mapas, paleta cálida vía `BlendPalettes`, sin texto) las repite desde su ángulo. No explicar con diálogo: re-mostrar.

**5. El santuario: paleta, gesto, silencio — y los regalos vuelven.** Truco Ma'habre completo: el santuario (o el pueblo visto desde él) en versión luminosa — mismo tileset, otra paleta, la única música cálida no-de-guardado del juego. Los huesos del Ditto original. El cierre es un gesto físico mínimo, nunca un monólogo: el equivalente exacto de "The girl takes your hand for a second before letting it go" — el Breedmare te empuja suavemente hacia la salida/el bote y se recuesta junto a los huesos. Un frame de conducta no-monstruosa.
**La mecánica exacta de la Niña:** en F&H1, si la Niña lleva equipados los regalos que le hiciste, el jefe final "pierde turnos contemplando su antiguo yo" — la bondad mecánicamente trivial se vuelve retroactivamente el corazón del final. Traducción: 2-3 oportunidades triviales y no señalizadas de bondad (darle una Baya al Meowth antes de la ejecución; dejar comida en el bosque "para nada"; no delatar al niño) registradas en flags mudas. En el santuario, esas flags alteran la conducta del Breedmare sin una palabra: el gesto final dura más, devuelve un objeto que dejaste, o —si hay combate scripteado— "duda" turnos enteros. Mapeo: `setflag` + `goto_if_set` en el script del santuario. Dejar sin resolver al menos dos cosas (qué era exactamente el experimento; quién publica el anuncio ahora): explicar todo es el mago revelando el truco.

**6. El shmup como objeto narrativo diegético: la máquina del bar.** Ya existe `src/minigame_spaceship.c` (nave, 4 BGs con INCBIN, más `minigame_pre.c`). Usarlo dos veces, con la estructura corrupción-de-lo-querido:
   - **Día 1:** es una máquina recreativa en la pensión ("única diversión de la isla"), tradición Game Corner. El niño del pueblo te reta; jugarlo es opcional, inocente, con premio trivial. Es la inversión de normalidad.
   - **Día 3 (o pesadilla del Día 2):** la máquina "te llama" — versión corrupta obligatoria: mismas rutinas, paletas cambiadas por INCBIN alternativo (verde Rher/grises), el countdown contando mal o al revés, los enemigos sustituidos por siluetas de aldeanos, la "nave" con la silueta del jugador... y derrota scripteada: en el momento álgido, la cosa que te persigue en pantalla deja de disparar y **te escolta**, esquivando contigo — el minijuego prefigura mecánicamente la revelación del Breedmare sin una palabra de texto. Salir del minijuego con mosaico y un silencio largo. (Variante/tercer uso si el final es huir en bote: reskin del shmup como la travesía nocturna, con el Breedmare "persiguiéndote" por última vez.)

**7. Gramática estricta de glitches: cada efecto significa UNA cosa — y la culpa toca el final.** El error típico es usar los efectos como condimento; F&H enseña que el jugador debe aprender el código. Léxico fijo: **mosaico** (`REG_MOSAIC`) = solo presencia/aparición del Breedmare; **onda de scanline** (`ScanlineEffect_InitWave`) = solo recuerdos/visiones del pasado de la isla; **deriva de paleta hacia verde** = el estado del pueblo por día, sutilmente ligada a una `VAR_PHANTOM_GUILT` oculta que suben decisiones (mirar la ejecución, aceptar comida, delatar); **corte de BGM a silencio** = algo va a pasar en esta pantalla. Presupuesto tipo F&H: 2-3 picos fuertes en todo el juego (estampa + sonido), el resto es anticipación.
**La culpa no puede quedarse en cosmética:** 2-3 umbrales de `VAR_PHANTOM_GUILT` que alteren el **epílogo** — qué gesto tiene el Breedmare contigo en el santuario (te empuja al bote / solo se aparta / no se deja ver), qué encuentras dentro del bote, y una única línea final distinta. El que rejuegue descubrirá que fue observado y juzgado en silencio todo el tiempo — exactamente la tesis del juego. (Sin ramificar finales completos: es epílogo variable, no árbol.)

**8. El compañero que se interpone (una vez).** Si el jugador lleva un Pokémon, robar el rasgo pasivo de la Niña como UNA escena mecanizada, no como sistema: en un combate clave del Día 3, el golpe que iba a tumbarte lo recibe él por script (fin de combate scripteado, queda a 1 PS o muere según una decisión previa del jugador, `GetMonData`/`SetMonData`). Cicatriz permanente: stat reducido, nickname alterado, sin Centro Pokémon funcional en la isla que lo repare. Prefigura al Breedmare: llevas todo el juego protegido sin saberlo, dos veces.

**9. Economía: curarse cuesta cuerpo o alma.** 12-15 curaciones colocadas a mano en todo el juego; la tienda "no vende a forasteros". En F&H la cordura se recupera con alcohol, tabaco y opio — "**la cura de la mente es la autodestrucción del cuerpo**". Traducción: el único curalotodo *renovable* es del pueblo — el licor de la posadera o la comida del consejo: cura al equipo completo pero sube `VAR_PHANTOM_GUILT` y los NPCs lo registran ("Ya comes como nosotros." — es la comida por la que ejecutaron al Meowth). Complemento: **un informante que cobra** (modelo Pocketcat) — un NPC que vende respuestas por objetos o favores morales, nunca gratis, nunca completo.

**10. Los combates ganables son puzzles, no guerras de stats.** F&H (Super Eyepatch Wolf; parche 2.0 de Termina): casi todo problema tiene solución creativa (trampas de oso contra ogros, cortar el brazo que sostiene el arma), y "cada zona introduce un tipo de amenaza nuevo — nunca dominas la gramática del peligro". De los 5-10 combates: los 3-5 ganables deben tener cada uno una **solución del entorno o del conocimiento** aprendida fuera del combate (atraerlo a la trampa del propio pueblo, apagar la luz que lo enfurece, el objeto correcto leído en un diario), y **ningún tipo de amenaza se repite**. Mapeo: script pre-combate que setea condiciones (`setweather`, `setvar` de debilidad, `checkitem`) — cero cambios al motor de batalla. Los 2-3 restantes son los imposibles de 2.15.

**11. Regla de escritura: ternura y horror en la misma frase.** Firma tonal de la saga: el "matrimonio de carne" se narra *"Te has convertido en la materialización del amor que compartieron. Entrelazados por siempre."* — horror corporal y amor en la misma oración. Reservar este registro exclusivamente para lo relativo al Breedmare y la cría: describir lo monstruoso con vocabulario de cuidado ("los huesos estaban ordenados, como se ordena una cuna") y lo doméstico con vocabulario clínico. Es la gramática exacta del giro protector: el texto enseña al oído del jugador, mucho antes del santuario, que aquí el amor y lo monstruoso comparten sintaxis.

**12. Diseñar para ser narrado.** F&H explotó por videoensayos ("más gente lo ve que lo juega"); un hack de terror lineal de 4-5 h con estampas es material ideal para YouTube/streamers hispanos. Los tres momentos clip-eables ya existen en el pitch: la ejecución, la primera persecución con firma sonora, y la revelación del santuario. Pulirlos pensando en cómo se ven narrados, porque esa será la distribución real. Y el test final de Miro para cada escena: si te incomoda escribirla, vas por buen camino; si te hace reír, córtala.

**El filtro global** para cada sistema que se añada: **debe poder enunciarse como una frase sobre Sombraluna**. "Aquí no se guarda gratis" (guardado diegético), "aquí la comida tiene dueño" (economía), "aquí lo que ignoras muere" (NPCs con agenda), "aquí lo que te protege se gasta" (compañero/Breedmare), "aquí nada te ataca por azar" (sin encuentros aleatorios). Si una mecánica no enuncia nada, es relleno.

---

## 6. Fuentes

**Entrevistas y análisis de diseño**
- Dark RPGs — entrevistas con Miro Haverinen (2019 y 2023): darkrpgs.home.blog (2019/03/08 y 2023/01/16), más "A journey in a land without morality" (2019/02/06) y "The unique stalking enemies of Termina" (2022/12/29)
- Hypercritic — reseña crítica de Fear & Hunger: hypercritic.org/collection/fear-and-hunger-video-game-review
- Horrorgameanalysis — "You should play Fear & Hunger" (2025), "Termina: some thoughts" (2023), "Early impressions" (2023)
- Medium — JupiterGenderfuck, "The Bodies of Fear and Hunger"; Spencer Johnson, "Unlearning how to be a hero" y "Termina is a horror game about trauma"
- Super Eyepatch Wolf — *The Cruelest Video Game* / *The Second Cruelest Video Game* (YouTube; hilo en ResetEra)
- Calxylian — "Fear & Hunger: alchemy, morality and suffering"; IndieHellZone; CGMagazine ("Brutal Draw"); KeenGamer

**Wikis (mecánicas y contenido verificado)**
- fearandhunger.wiki.gg: Coin_flip, Saving, Game_Mechanics, Mind, Dismemberment, Crow_Mauler, The_Girl, Girl, Moonscorch, Le'garde (fandom), Endings_F&H1, Books_F&H1, Unnamed_Books_F&H2, Pocketcat, Per'kele, Levi/Dialogue, Gentleman, Mayor's_Manor, Ma'habre, The_Gods, Prehevil, Prehevil_(Inner_City), Termina, Locations_List_F&H2, Tanaka, Henryk, August, Caligura, Samarie, Pav, PRHVL_Bop, Rher, Prehevil_map, Soundtrack_F&H1/F&H2, Difficulty_Settings_F&H2, Characters_List_F&H2
- Wikipedia: Fear_&_Hunger, Lavender_Town

**Recepción y casos comparables**
- TV Tropes (F&H reviews; YMMV Pokémon Snakewood); Backloggd; hilos de Steam (coin flip saves, obras inspiradas en F&H, Look Outside); GameFAQs (guía "Absolute basics")
- Kotaku (Patricia Hernandez, Lavender Town Syndrome); Pinkie's Pokémon (reseña Snakewood); PokéCommunity (Pokémon Ghost Grey); GameRant (hacks basados en creepypastas); Bloody Disgusting (horror ROM hacks); Gameland (parche 2.0 de Termina); DualShockers, Screen Rant, Destructoid (recepción de Termina)

**Verificación técnica en el repo (pokemon-phantom / pokeemerald)**
- `src/palette.c` (TintPalette_SepiaTone/GrayScale, BlendPalettes), `include/sound.h` (PlaySE12WithPanning, FadeOutBGM, PlayCryInternal — firma sin pitch: se necesita un CRY_MODE propio), `include/constants/sound.h` (CRY_MODE_*), `asm/macros/event.inc` (playse, playmoncry, animateflash, fadescreen), `charmap.txt` (PLAY_SE, PAUSE_MUSIC, PAUSE), `include/overworld.h` (SetFlashLevel), `include/scanline_effect.h`, `include/gba/io_reg.h` (REG_MOSAIC), `include/constants/event_objects.h` (OBJ_EVENT_GFX_VAR_0..F), `src/start_menu.c`, `src/strings.c`, `src/battle_message.c`, y el minijuego ya implementado: `src/minigame_spaceship.c`, `src/minigame_pre.c`
