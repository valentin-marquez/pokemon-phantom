PhantomSandbox_MapScripts::
	.byte 0

PhantomSandbox_EventScript_Villager::
	lock
	faceplayer
	msgbox PhantomSandbox_Text_Villager, MSGBOX_NPC
	release
	end

PhantomSandbox_EventScript_MiniScene::
	lockall
	fadeoutbgm 2
	delay 30
	message PhantomSandbox_Text_Acta
	waitmessage
	playse SE_BANG
	fadescreen FADE_TO_BLACK
	special PhantomMarkExecutionSeen
	setflag FLAG_PHANTOM_MEOWTH_EXECUTED
	special PhantomReloadOverworldPalettes
	delay 60
	fadescreen FADE_FROM_BLACK
	releaseall
	end

PhantomSandbox_EventScript_Bed::
	lockall
	fadescreen FADE_TO_BLACK
	special HealPlayerParty
	special PhantomAdvanceDay
	call Common_EventScript_SaveGame
	fadescreen FADE_FROM_BLACK
	releaseall
	end

PhantomSandbox_Text_Villager:
	.string "Bienvenido a Sombraluna,{PAUSE 8} forastero.$"

PhantomSandbox_Text_Acta:
	.string "Se resolvió.{PAUSE 15} Como debe ser.$"
