# Pokémon Phantom — Documento de diseño

> Spec de diseño (17 jul 2026). Reinterpreta el [pitch original de Notion](https://app.notion.com/p/Pok-mon-Phantom-Pitch-y-Resumen-7a3eb0e02b3648babce66ed528c7e3d2) con estilo propio, referencia narrativa **Fear & Hunger** y un alcance realista para un dev en solitario. Documentos hermanos: [`docs/design/informe-fear-and-hunger.md`](../../design/informe-fear-and-hunger.md) (principios narrativos) y [`docs/design/factibilidad-y-harness.md`](../../design/factibilidad-y-harness.md) (verificación técnica `file:line` + harness de testing).

---

## 0. Qué es y qué no es

**Es:** un mini ROM hack de terror psicológico sobre pokeemerald vanilla. Una experiencia **narrativa, lineal, de una tarde (4-5 h)**, jugable de principio a fin sin grindeo. Terror explícito construido con **atmósfera, texto y efectos de motor**, no con tilesets gore.

**No es:** un juego Pokémon de capturar/subir niveles/ganar medallas. No sale nunca de la isla. No tiene dex propia, ni finales ramificados completos, ni especies custom jugables.

**Los tres pilares intocables:** la ejecución del Meowth · el Breedmare (persecución + santuario) · el giro de que el monstruo te protegía.

**El filtro de diseño (de F&H):** cada sistema debe poder enunciarse como una frase sobre Sombraluna. Si no enuncia nada, es relleno. Y toda referencia se toma **por función, nunca por objeto literal** — nada importado tal cual de F&H.

---

## 1. Historia

### Premisa
**El Forastero** (19, sin nombre — proyección del jugador) viaja a la isla **Sombraluna** buscando a su hermano **Carlos**, que respondió al mismo anuncio de trabajo hace un año y dejó de escribir. Lleva la consola portátil que compartían, con el récord de Carlos en el shooter espacial.

Sombraluna es un pueblo pesquero-conservero decadente (anclaje visual: caletas del sur de Chile — conserveras abandonadas, muelles podridos, cartelería oxidada). Su **herida fundacional**: un invierno de hambruna en que el consejo "decidió quién comía" — reconstruible solo por textos anónimos que se contradicen entre sí (nunca hay versión objetiva). Desde entonces rige el utilitarismo total: cada boca que no contribuye, no come. El pueblo no es malvado con el Forastero; es coherentemente amoral *con todos*, ellos incluidos.

El **anuncio** se publica cada vez que el contratado anterior "deja de servir". El pueblo niega conocer a Carlos. La posadera te da su antigua habitación sin decírtelo.

El **Breedmare** (Ditto deformado por los experimentos del consejo) te acecha desde el Día 2. La revelación del santuario: cada aparición era protección. ¿Cuánto de Carlos queda en él? **El juego jamás lo confirma.** Dos cosas quedan sin resolver a propósito: qué era exactamente el experimento, y quién publica el anuncio ahora.

### Tema
La culpa no se tiene, **se hace**: la del jugador se construye con sus actos e inacciones en la isla (mirar la ejecución, aceptar la comida, callar). Buscar a Carlos es el espejo — llegaste tarde por un año, igual que apartaste la vista por un segundo. Registro tonal firma de la saga, reservado al Breedmare: **ternura y horror en la misma frase** ("los huesos estaban ordenados, como se ordena una cuna").

---

## 2. Estructura y escaleta

El tiempo avanza **solo al dormir** (`VAR_PHANTOM_TIME`, sin RTC). Dormir = guardar + curar + el mundo empeora un paso. Los "días" son actos; dormir es la transición de acto.

| Acto | Contenido | Cierre |
|---|---|---|
| **Prólogo** *(ya implementado)* | Tren + consola + shmup v1. El lanchero al desembarcar: *"Los últimos tres... supongo que decidieron quedarse."* | Desembarco |
| **Día 1 — La isla amable** | Presentación de 6-8 del reparto con nombre (§3) con detalle humano; preguntas por Carlos negadas (demasiado uniforme); máquina recreativa del bar (shmup, récord "CAR" visible); 1-2 textos anónimos; la tienda que no vende a forasteros. Superficie Pokémon **intacta** (jingles normales) | **La ejecución del Meowth** |
| **Día 2 — El colapso del objetivo** | El "trabajo" en la conservera (jaulas vacías con la puerta doblada hacia afuera); la habitación del anterior contratado = la de Carlos (su consola, sus iniciales, su diario a medias); la cena ritual en la casona; primer acecho del Breedmare; distorsión leve de los NPCs | Objetivo: de *trabajar* → *huir* |
| **Día 3 — La huida** | El pueblo suena muerto (pista nocturna única); el Plano es registro de bajas; el refugio-trampa traiciona; persecución en el bosque; el compañero se interpone; el shmup corrupto | Acorralado → santuario |
| **Amanecer — El santuario** | Paleta cálida, los huesos, el flashback mudo, el gesto (variable por culpa), el tour del epílogo, el bote. Post-créditos: el anuncio, otra vez clavado | Fin |

### Beats clave (detalle)
- **La ejecución** (cierre Día 1, dentro de los primeros ~30 min): sin música de aviso (`FadeOutBGM` antes), sprites normales, la campana, caja de mensajes en tono de acta ("Se le encontró con comida ajena. El consejo resolvió. Se resolvió."), fundido a negro, UN solo SE. **Sin estampa** (guardarla). Que quien te regaló la Baya sea quien sostiene la cuerda. Después, todos vuelven a ser amables → recalibra al jugador.
- **El colapso** (Día 2): encontrar la consola de Carlos con su récord recontextualiza todo el prólogo.
- **Intervenciones ambiguas del Breedmare** (3-4, antes del santuario): el puente destrozado la noche que ibas a cruzarlo; el objeto junto a tu cama que el pueblo niega; el aldeano que te seguía y aparece "enfermo"; el paso cerrado hacia la ruta del matadero. Todas leídas como amenaza en su momento.
- **El santuario** (amanecer): truco de paleta cálida (mismo tileset), la única música cálida no-de-guardado, el flashback mudo que repite las intervenciones desde el ángulo del Breedmare, y el gesto físico mínimo (te empuja al bote y se recuesta junto a los huesos).

---

## 3. Reparto

10 con nombre + agenda por franja (tabla de posición leída por el Plano); el resto es decorado que va desapareciendo.

| Personaje | Rol / arco |
|---|---|
| **La posadera** | Te guarda el plato; te da la antigua habitación de Carlos. Su licor cura con precio moral. Día 3: su pensión es la trampa |
| **El niño** | Te reta a la máquina, te regala el Plano. No delatarlo = flag muda de bondad. Día 3: desaparece del plano |
| **El verdugo** (carnicero) | Te regala una Baya el Día 1; sostiene la cuerda esa noche. Final: termina como lo que castigaba |
| **El jefe del consejo** | La cena ritual (cortesía como amenaza); nunca se ensucia las manos; mira demasiado tiempo a tu Pokémon |
| **El heraldo** (borracho del muelle) | Enuncia las reglas reales como delirio ("Los forasteros no se van"). Todo cierto; el guion nunca lo confirma. Diálogo por franja |
| **La mujer del muelle** | Quiso irse. NPC-agenda: si no hablas con ella antes de dormir la 2ª vez → Día 3 su casa vacía, barca rota. Sin escena |
| **El pescador** | Tu compañero de "trabajo"; te sigue de noche por orden del consejo → aparece "enfermo" y no vuelve a mirarte |
| **El cura/sacristán** | Toca la campana; su sermón es un texto anónimo; conoce la iglesia vieja (el santuario); lo niega con "..." |
| **El informante** (buhonero) | Vende respuestas por objetos o favores morales, nunca gratis ni completo |
| **Carlos** | Presente solo en rastros (habitación, récord "CAR", diario, documentos contradictorios). **Jamás aparece en pantalla** |

**El compañero:** llegas con UN Pokémon (el anuncio pedía "experiencia con Pokémon"). Default **Poochyena** — un perro que gruñe a lo que tú no ves es horror gratis (se eriza, se niega a avanzar, teme a los aldeanos antes que tú). Se interpone una vez (Día 3). *Especie negociable.*

**El Breedmare:** silueta 16×32 en overworld, detalle solo en estampas; firma sonora (cry de Ditto invertido + grave, paneado del lado por el que viene). Es un **estado del mundo** desde el Día 2, no un evento único.

---

## 4. Sistemas (cada uno enuncia una frase)

> Factibilidad y hooks `file:line` en [`factibilidad-y-harness.md`](../../design/factibilidad-y-harness.md). Complejidad: S=horas, M=días.

| Sistema | Frase | Implementación (default) |
|---|---|---|
| **El reloj** | "El tiempo solo pasa cuando bajas la guardia" | `VAR_PHANTOM_TIME` (5 estados). Avanza solo al dormir. NPCs/música/paleta leen la var en `ON_TRANSITION`. Sin RTC. **[S]** |
| **La culpa** | "Aquí todo se anota" | `VAR_PHANTOM_GUILT` oculta, la suben ~8-10 decisiones; 3 umbrales → intensidad de deriva de paleta, líneas de NPCs, y **el gesto del epílogo**. Aparte, 3 *flags mudas de bondad* leídas solo en el santuario. **[S]** |
| **Ritual de la cama** | "El descanso nunca es neutro" | Elegir de qué lado dormir (multichoice + posición del sprite). Resultado predeterminado por guion. El otro lado = símbolo de Carlos y vehículo de la visita del Breedmare ("El otro lado del colchón está hundido. Tibio."). **Reemplaza al coin flip de F&H** — referencia por función, no por objeto. **[S]** |
| **Guardado diegético** | "Aquí no se guarda gratis" | SAVE fuera del menú (borrar 1 línea). Guardar = camas (avanzan el día) + 2-3 puntos "de respiro" por día (altar, banca) que guardan sin avanzar. Checkpoints invisibles en persecuciones (perder nunca repite >~3 min). **[S]** |
| **Sin encuentros aleatorios** | "Todo lo que te ataca te eligió" | Tasas a 0 en toda la isla. Hierba silenciosa como microterror. Toda amenaza es object event visible o trigger, precedida por firma sonora. **[S]** |
| **Combates (6-8)** | "El juego no te debe una pelea justa" | 4-5 ganables tipo **puzzle** (cada uno con solución del entorno aprendida fuera del combate; ningún tipo se repite) + 2-3 **imposibles**. **Default para los imposibles: vía barata [M]** (enemigo invencible + excepción de derrota estilo Battle Pyramid + `gBattleOutcome` forzado), NO battle controller cinemático (eso es semanas). Perder → interceptar `DoWhiteOut` para mandar a escena, no al centro. **[M]** |
| **Breedmare-estado** | "Aquí lo que te protege se gasta" | Flags por acto + mapas calientes con decisión en `ON_TRANSITION`/`ON_FRAME`. Firma: `CRY_MODE` propio (~10 líneas) + `PlaySE12WithPanning` + mensaje de presencia. **Persecución: cadena de segmentos de un solo mapa unidos por warps** (NPC duplicado por mapa, IA de Mew invertida) — NO follower multi-mapa real. Se retrasa (puertas `setmetatile`, comida), no se vence. **[M]** |
| **Cicatriz del compañero** | (parte del Breedmare) | Se interpone una vez; cicatriz permanente vía **EVs/IVs o flag en SaveBlock + nickname alterado** — NUNCA stats/HP directos (se recalculan). Sin centro que lo repare. **[M]** |
| **Gramática de glitches** | (el lenguaje, no un sistema) | Módulo `src/phantom_fx.c` + specials. **Léxico fijo, un efecto = un significado:** mosaico=aparición del Breedmare · onda de scanline=recuerdos/visiones · deriva de paleta a verde=estado del pueblo+culpa · corte de BGM=algo viene. Tinte en `gPlttBufferUnfaded` (o el clima lo pisa). Scanline: relanzar por mapa, prohibido con flash. 2-3 picos fuertes en todo el juego; el resto, anticipación. **[M]** |
| **El Plano** | "Aquí lo que ignoras muere" | Key item (molde Wailmer Pail) que lista los 10 con nombre y su paradero según `VAR_PHANTOM_TIME`+flags. Día 3: registro de bajas. Requiere icono en la tabla (o crashea la bolsa). **[S–M]** |

---

## 5. Presupuesto de assets externos (lo único que no hace el dev)

El juego se **diseña para funcionar con placeholders**; los encargos se hacen con el guion congelado.

| Pieza | Cantidad | Nota |
|---|---|---|
| Estampas fullscreen 240×160 | 4-6 | ejecución en siluetas, santuario, derrota ante Breedmare, cena, bote/epílogo, +reserva. Una imagen 8bpp a la vez, ≤240 colores, paleta 15 al textbox |
| Sprite overworld Breedmare | 1 | silueta 16×32, pocos frames |
| Sprite de batalla Breedmare | 1 | solo para los combates imposibles; silueta/contraluz |
| Tiles puntuales | ~1 tanda | fachada de conservera, poste de la plaza, detalles. **Todo lo demás: tilesets de Hoenn recolorizados por paleta** (la dirección de arte ES la paleta — código, no arte) |
| Música original | 2-3 piezas | el tema cálido, la pista muerta nocturna, un ambient. Resto: silencio, SFX, drones m4a, pistas de Emerald degradadas. **Reutilizar los 182 voicegroups.** Costo dominante: autoría MIDI |
| SEs custom | 3-4 | firma del Breedmare, campana, stingers |

---

## 6. Harness de testing IA

Sistema para que Claude arranque el juego, lea/escriba RAM, inyecte inputs, capture pantalla y depure en vivo, sin que Valentin pase bugs a mano. Arquitectura y comandos exactos en [`factibilidad-y-harness.md`](../../design/factibilidad-y-harness.md). En 5 fases:

0. **Bootstrap** — instalar toolchain (`arm-none-eabi-gcc` + mGBA) + `mgba-rom-test`; primer `make DINFO=1 modern`. **Bloqueante: hoy la ROM no compila.**
1. **Smoke test headless** — `DebugPrintf` de checkpoints + `mgba-rom-test` → pass/fail de build+boot+escena. *(Primero: la red de seguridad más barata.)*
2. **Control con screenshots** — `libmgba-py`: cargar savestate, inyectar inputs, avanzar frames, sacar PNG, leer memoria por símbolo del `.map`.
3. **Debug simbólico** — stub GDB de mGBA (ya en `.vscode`) + `gdb -batch` para leer `vars`/`flags`/`SaveBlock`. Solo breakpoints + lecturas.
4. **Menú debug in-ROM** — warp + set flags/vars (menú mínimo del pret wiki).

---

## 7. Fuera de alcance (recortes decididos)

- **Especies custom jugables** (Culpeon, especímenes alterados). El Breedmare no entra al dex: es escenas + 2 sprites.
- **Finales múltiples** → un final, epílogo variable por culpa/bondad.
- **Valle Minero y Pico de la Revelación** → fusionados: la mina son los sótanos de la conservera; el santuario es la iglesia del bosque.
- **Sistema de culpa "en tiempo real"** del pitch → simplificado a estados por día + umbrales.
- **Persecución follower multi-mapa "de verdad"** → segmentos de un mapa por warp.
- **Combate imposible cinemático tipo Wally** → vía barata (enemigo invencible + excepción de derrota).
- **Todo Hoenn** → nunca sales de la isla; menús no pertinentes (dex, Pokénav) se ocultan.
- **Intro de Birch / selección de género** → recortada; el Forastero es fijo.
- **Violencia sexual del pitch original** → fuera sin matices (lección de recepción de F&H); el horror del Breedmare es implícito y clínico.

---

## 8. Riesgos y orden de trabajo

**Riesgos honestos:**
1. **Toolchain roto hoy** — nada avanza hasta la Fase 0 del harness (build + `arm-none-eabi-gcc`).
2. **Música** — 2-3 piezas originales es el mayor hueco tras el arte; mitigado por la estética del silencio, no a cero.
3. **Mapas** — ~12-15 mapas pequeños son el grueso del trabajo bruto; disciplina de "pequeño y denso".
4. **Dependencia de encargos** — solo al final, con guion congelado; placeholders hasta entonces.

**Orden sugerido (a detallar en el plan de implementación):**
1. Fase 0-1 del harness (toolchain + smoke test) — sin esto no se verifica nada.
2. Andamiaje de flujo: recorte de intro/menús, `VAR_PHANTOM_TIME`, guardado diegético, sin encuentros.
3. Un vertical slice jugable del Día 1 con placeholders (pueblo + la ejecución) para validar tono y loop.
4. Módulo de estampas + módulo `phantom_fx` (gramática de glitches).
5. Días 2-3, el Breedmare-estado, el shmup v2, el santuario.
6. Audio y encargos de arte con el guion congelado.

---

## Referencias
- Pitch original: [Notion](https://app.notion.com/p/Pok-mon-Phantom-Pitch-y-Resumen-7a3eb0e02b3648babce66ed528c7e3d2)
- Principios narrativos: [`docs/design/informe-fear-and-hunger.md`](../../design/informe-fear-and-hunger.md)
- Verificación técnica + harness: [`docs/design/factibilidad-y-harness.md`](../../design/factibilidad-y-harness.md)
