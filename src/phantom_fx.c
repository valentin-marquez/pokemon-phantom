#include "global.h"
#include "phantom.h"
#include "scanline_effect.h"
#include "gpu_regs.h"
#include "task.h"
#include "trig.h"
#include "event_data.h"
#include "overworld.h"
#include "sprite.h"
#include "event_object_movement.h"
#include "palette.h"
#include "constants/flags.h"

// Efecto de "mareo"/distorsión de lente ("botella de vidrio") sobre el
// overworld tras la ejecución de Meowth. Desplaza el HOFS de las 3 capas del
// mapa (BG1/BG2/BG3) por una onda de seno por scanline, siguiendo el scroll de
// la cámara. El texto/menús viven en BG0 (prioridad 0) → NO se distorsionan.
//
// Se apropia del HBlank-DMA de scanline (libre en mapas sin flash; en cuevas el
// flash lo usa, así que ahí no arrancamos). El VBlank del overworld ya llama a
// ScanlineEffect_InitHBlankDmaTransfer, que arma el DMA desde gScanlineEffect.

#define MAREO_FREQUENCY 2            // ciclos de seno por pantalla (fijo)
#define MAREO_DEFAULT_AMPLITUDE 3    // px máx de desplazamiento (leve)
#define MAREO_DEFAULT_SPEED 2        // avance de fase por frame
// Amplitud y velocidad son ajustables por script (special PhantomMareoOn, que
// lee VAR_0x8004/VAR_0x8005). Ver sMareoAmplitude/sMareoSpeed más abajo.

// 6 halfwords por scanline: BG1HOFS,BG1VOFS,BG2HOFS,BG2VOFS,BG3HOFS,BG3VOFS.
// (Los HOFS llevan la onda; los VOFS, el scroll base sin tocar.)
#define MAREO_REGS_PER_LINE 6
#define MAREO_DMACNT (((DMA_ENABLE | DMA_START_HBLANK | DMA_REPEAT | DMA_SRC_INC | DMA_DEST_INC | DMA_16BIT | DMA_DEST_RELOAD) << 16) | MAREO_REGS_PER_LINE)

// Sin estáticos inicializados a no-cero (irían a .data, que el ld modern
// descarta): usamos un bool de "activo" en .bss en vez de comparar con TASK_NONE.
static bool8 sMareoActive;
static u16 sMareoPhase;
static u8 sMareoAmplitude;   // ajustable por script (0 = usar default)
static u8 sMareoSpeed;       // ajustable por script (0 = usar default)

static void Task_PhantomMareo(u8 taskId);

// Re-tiñe SOLO las paletas OBJ que usan los sprites del overworld (jugador,
// NPCs, objetos), no las de la UI/menú. Se llama UNA vez tras la ejecución
// (los residentes no pasan por PatchObjectPalette). Dedup por slot para no
// teñir dos veces la misma paleta (se oscurecería).
void PhantomFx_RetintObjectSprites(void)
{
    u32 i;
    u16 doneSlots = 0;

    if (!FlagGet(FLAG_PHANTOM_MEOWTH_EXECUTED))
        return;

    for (i = 0; i < OBJECT_EVENTS_COUNT; i++)
    {
        if (gObjectEvents[i].active)
        {
            u8 pal = gSprites[gObjectEvents[i].spriteId].oam.paletteNum;

            if (!(doneSlots & (1 << pal)))
            {
                doneSlots |= 1 << pal;
                Phantom_TintPaletteRange(OBJ_PLTT_ID(pal), 16);
            }
        }
    }
}

// Copia manual de la primera scanline (la DMA empieza tras la línea 0).
static void CopyFirstMareoScanline(void)
{
    vu16 *dst = (vu16 *)REG_ADDR_BG1HOFS;
    u16 *src = gScanlineEffectRegBuffers[gScanlineEffect.srcBuffer];
    u32 i;

    for (i = 0; i < MAREO_REGS_PER_LINE; i++)
        dst[i] = src[i];
}

// (Re)arma el DMA de scanline para el efecto. Idempotente: la carga de mapa
// resetea gScanlineEffect, así que hay que volver a montarlo en cada mapa.
static void SetupMareoScanline(void)
{
    ScanlineEffect_Clear();
    // +MAREO_REGS_PER_LINE = arrancar el DMA en la línea 1 (la 0 va a mano).
    gScanlineEffect.dmaSrcBuffers[0] = (u16 *)gScanlineEffectRegBuffers[0] + MAREO_REGS_PER_LINE;
    gScanlineEffect.dmaSrcBuffers[1] = (u16 *)gScanlineEffectRegBuffers[1] + MAREO_REGS_PER_LINE;
    gScanlineEffect.setFirstScanlineReg = CopyFirstMareoScanline;
    gScanlineEffect.dmaControl = MAREO_DMACNT;
    gScanlineEffect.dmaDest = (void *)REG_ADDR_BG1HOFS;
    gScanlineEffect.state = 1;
}

// Rellena un buffer de scanline con la onda: HOFS = scroll de cámara + seno,
// VOFS = scroll sin tocar. Lee el scroll vivo cada frame.
// Nota (over-read benigno): la DMA lee 6 halfwords por la HBlank posterior a la
// línea 159 (la línea 160, fuera de pantalla) → lee 6 halfwords más allá del
// buffer usado. En GBA es inofensivo (EWRAM no falla, línea invisible, se
// corrige al frame siguiente); por eso NO rellenamos una línea extra (el buffer
// gScanlineEffectRegBuffers[2][0x3C0] cabe justo 6×160).
static void FillMareoScanlineBuffer(u16 *buf)
{
    s16 bg1h = GetGpuReg(REG_OFFSET_BG1HOFS);
    s16 bg1v = GetGpuReg(REG_OFFSET_BG1VOFS);
    s16 bg2h = GetGpuReg(REG_OFFSET_BG2HOFS);
    s16 bg2v = GetGpuReg(REG_OFFSET_BG2VOFS);
    s16 bg3h = GetGpuReg(REG_OFFSET_BG3HOFS);
    s16 bg3v = GetGpuReg(REG_OFFSET_BG3VOFS);
    u32 line;

    for (line = 0; line < DISPLAY_HEIGHT; line++)
    {
        u8 theta = (line * MAREO_FREQUENCY + sMareoPhase) & 0xFF;
        s16 wave = (gSineTable[theta] * sMareoAmplitude) >> 8;
        u32 o = line * MAREO_REGS_PER_LINE;

        buf[o + 0] = bg1h + wave;
        buf[o + 1] = bg1v;
        buf[o + 2] = bg2h + wave;
        buf[o + 3] = bg2v;
        buf[o + 4] = bg3h + wave;
        buf[o + 5] = bg3v;
    }
}

// Apaga SOLO el task del mareo (mata el task + deshace el x2 de los sprites),
// sin tocar el sistema de scanline. Se usa al entrar a un mapa con flash: ahí
// el flash ya montó el scanline y NO debemos pisarlo (romperíamos la cueva).
static void TeardownMareoTask(void)
{
    u8 taskId = FindTaskIdByFunc(Task_PhantomMareo);
    u32 i;

    if (taskId != TASK_NONE)
        DestroyTask(taskId);
    sMareoActive = FALSE;
    for (i = 0; i < OBJECT_EVENTS_COUNT; i++)
        if (gObjectEvents[i].active)
            gSprites[gObjectEvents[i].spriteId].x2 = 0;
}

// Núcleo: monta el efecto (respeta el flash; NO chequea la flag). Aplica los
// defaults si amplitud/velocidad no se han fijado por script. Idempotente:
// re-arma el scanline y crea el task solo si no existe ya (robusto ante
// ResetTasks del cambio de mapa: no trackeamos el taskId, lo buscamos).
static void StartMareoCore(void)
{
    if (GetFlashLevel() != 0)   // en cuevas el flash ya usa el scanline
        return;
    if (sMareoAmplitude == 0)
        sMareoAmplitude = MAREO_DEFAULT_AMPLITUDE;
    if (sMareoSpeed == 0)
        sMareoSpeed = MAREO_DEFAULT_SPEED;

    SetupMareoScanline();
    // Prefill ambos buffers para que el primer frame no muestre offset (0,0)
    // (ScanlineEffect_Clear los dejó a cero; sin esto habría un tirón de 1 frame
    // al arrancar en el sitio, con la pantalla visible).
    FillMareoScanlineBuffer(gScanlineEffectRegBuffers[0]);
    FillMareoScanlineBuffer(gScanlineEffectRegBuffers[1]);
    // La fase NO se resetea: persiste en .bss → la onda sigue derivando de forma
    // continua entre mapas (nada de "pop" al re-armar tras un warp).
    if (FindTaskIdByFunc(Task_PhantomMareo) == TASK_NONE)
        CreateTask(Task_PhantomMareo, 0);
    sMareoActive = TRUE;
}

// Arranque automático tras la ejecución: gateado por la flag.
void PhantomFx_StartMareo(void)
{
    if (!FlagGet(FLAG_PHANTOM_MEOWTH_EXECUTED))
        return;
    StartMareoCore();
}

// Enganche en la carga de mapa (persistencia entre warps). En mapas sin flash
// con la ejecución ya vista, re-arma; entrando a cueva/flash, lo apaga.
void PhantomFx_OnMapLoad(void)
{
    if (FlagGet(FLAG_PHANTOM_MEOWTH_EXECUTED) && GetFlashLevel() == 0)
        StartMareoCore();
    else if (sMareoActive)
        TeardownMareoTask();   // entrando a cueva/flash: NO tocar el scanline (lo usa el flash)
}

// Special para scripts:
//   setvar VAR_0x8004, <amplitud>   @ px (0 = default 3)
//   setvar VAR_0x8005, <velocidad>  @ avance de fase/frame (0 = default 2)
//   special PhantomMareoOn
// Amplitud/velocidad se aplican EN VIVO (el task las lee cada frame); si estaba
// parado, arranca. No exige la flag (para tunear o usarlo en otros momentos).
void PhantomMareoOn(void)
{
    // Amplitud/velocidad son u8 (0-255); valores mayores en la var se truncan.
    // Rango útil real: amplitud ~1-10, velocidad ~1-6.
    sMareoAmplitude = gSpecialVar_0x8004;
    sMareoSpeed = gSpecialVar_0x8005;
    StartMareoCore();
}

// Special para scripts: special PhantomMareoOff
void PhantomMareoOff(void)
{
    PhantomFx_StopMareo();
}

// Apagado explícito (special PhantomMareoOff): además de matar el task, libera
// el scanline (aquí sí, porque lo apagamos a propósito en un mapa normal).
void PhantomFx_StopMareo(void)
{
    TeardownMareoTask();
    ScanlineEffect_Stop();
}

static void Task_PhantomMareo(u8 taskId)
{
    FillMareoScanlineBuffer(gScanlineEffectRegBuffers[gScanlineEffect.srcBuffer]);

    // Los sprites (jugador, NPCs, objetos) viven en la capa OBJ, que la onda de
    // scanline no toca. Para que también "ondulen", desplazamos cada sprite del
    // overworld en X según la onda a su altura de pantalla (x2 = offset extra).
    {
        u32 i;
        for (i = 0; i < OBJECT_EVENTS_COUNT; i++)
        {
            if (gObjectEvents[i].active)
            {
                struct Sprite *s = &gSprites[gObjectEvents[i].spriteId];
                u8 sy = (u8)(s->y + s->y2);
                u8 th = (sy * MAREO_FREQUENCY + sMareoPhase) & 0xFF;
                s->x2 = (gSineTable[th] * sMareoAmplitude) >> 8;
            }
        }
    }

    sMareoPhase += sMareoSpeed;
}
