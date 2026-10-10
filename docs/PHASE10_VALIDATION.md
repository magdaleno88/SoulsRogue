# Fase 10 — Light Attack animado y Attack Window

10 de octubre de 2026, America/Mexico_City. Incremento expresamente autorizado sobre Fase9: **154 pruebas anteriores + 22 nuevas = 176/176 Success**, build Editor Development con UE5.8.3, cook Windows y loop completo en PIE con render. La Fase11 queda pendiente.

## Entrega solicitada

1. **Fuente:** `/Game/Characters/Mannequins/Anims/Unarmed/Attack/MM_Attack_01`, 1s. Es la preferida por la solicitud y comparte exactamente el skeleton del jugador; permite un swing único sin reutilizar el sistema de combos. También se inspeccionaron MM_Attack_02/03 y AM_ComboAttack de Variant_Combat como referencia. Evidencia previa a implementar: `Saved/Diagnostics/AnimatedAttackPhase-Inspection.json` y `Saved/Logs/AnimatedAttackPhase-Inspection.log`.
2. **Skeleton:** `/Game/Characters/Mannequins/Meshes/SK_Mannequin.SK_Mannequin`. BP_ThirdPersonCharacter usa SKM_Quinn_Simple y ABP_Unarmed_C, con AnimationBlueprint y DefaultSlot. La compatibilidad se comprueba en el editor y por test W; las poses renderizadas confirman que el slot real reproduce la animación.
3. **C++ creado (4):** `Source/SoulsNew/Combat/Animation/SoulsAnimNotifyState_MeleeWindow.{h,cpp}`, `Source/SoulsNew/Tests/SoulsAnimatedAttackTests.cpp` y `Source/SoulsNew/Tests/SoulsMeleeTestFixture.h`. El fixture es exclusivamente de tests.
4. **C++ modificado (6):** `Source/SoulsNew/Combat/SoulsMeleeCombatComponent.{h,cpp}`, `Source/SoulsNew/SoulsNewCharacter.{h,cpp}`, `Source/SoulsNew/Tests/SoulsMeleeCombatTests.cpp` y `Source/SoulsNew/Tests/SoulsCombatMapTests.cpp`. Health, Items, Rewards, Pickup, DropSpawner, boss, Controller y sus reglas no cambian.
5. **Asset creado:** `/Game/SoulsNew/Animations/AM_Souls_LightAttack`. Autoría mediante AnimMontageFactory/AssetTools y AnimationLibrary, guardado con EditorAssetLibrary. Contiene un segmento fuente, DefaultSlot y un único notify propio. Scripts diagnósticos en Saved: CreateLightAttackMontage.py y ConfigureMeleeWindow.py. Se usa la API instalada y las operaciones oficiales de [AnimMontageFactory](https://dev.epicgames.com/documentation/en-us/unreal-engine/python-api/class/AnimMontageFactory?application_version=5.2) y [AnimationLibrary](https://dev.epicgames.com/documentation/en-us/unreal-engine/python-api/class/AnimationLibrary?application_version=5.7) de Epic. No se fabrican bytes ni se añade Python al runtime.
6. **Assets preexistentes modificados: ninguno.** La autoría y configuración durante esta fase afectan únicamente al montage nuevo. Se conservan mapa oficial, ThirdPerson, Variant_Combat, source clips, Animation Blueprint, input y Vitalidad.
7. **Lifecycle:** enum reflejado Idle/Windup/Active/Recovery. ActorComponent sin Tick permanente ni timers. El componente conserva GUID causal, ID de montage, mesh/AnimInstance débiles, montage fuerte y HitActors débiles durante la ejecución. La autoridad de daño permanece en Health.
8. **TryLightAttack:** deja de ejecutar el sweep. Valida estado Idle, cooldown, configuración finita, owner/World/BeginPlay, Health único y vivo si existe, mesh única, AnimInstance y skeleton. Entra en Windup y reproduce el montage. True significa inicio admitido, aunque luego falle el sweep; false no deja ventana ni ejecución parcial. Los errores de reproducción limpian estado y no reservan cooldown.
9. **Reproducción:** Montage_Play a velocidad1 desde0, sin detener todos los grupos. Se captura la instancia que se acaba de crear. El Character proporciona el default fuerte/configurable y lo aplica en BeginPlay; el componente no codifica rutas de contenido. La fuente tiene root motion habilitado: se usa IgnoreRootMotion durante la ejecución y se restaura el modo previo al limpiar, si no lo cambió otro sistema. No se modifica el clip ni se añade locomoción de ataque.
10. **Apertura:** el notify nativo stateless solicita BeginAttackWindow con mesh, asset e ID de instancia. Solo una solicitud válida de Windup abre Active y prepara el conjunto de impactos. Los branching points nativos suministran la procedencia exacta; el camino estándar exige contexto de montage y rechaza INDEX_NONE. No se infiere el ataque a partir de GetPlayerCharacter.
11. **Hits:** cada NotifyTick solicita ProcessAttackWindow. Solo Active permite el sweep frontal existente: Damage100, Range200, TraceRadius40, Pawn/WorldDynamic/WorldStatic, owner ignorado. Targets con exactamente un Health válido, orden ordinal actor/componente y contexto Attack.Light con GUID común, Source/Instigator y HitLocation/HitNormal del hit seleccionado. Un target que entra después puede recibir daño en otro Tick.
12. **Un impacto por actor:** HitActors persiste durante todo el ataque. Se registra el actor antes de ApplyDamage para bloquear duplicados y reentrada durante observadores. Varios cuerpos o cinco sweeps no vuelven a aplicar100. Un nuevo ataque limpia el conjunto y usa otro GUID.
13. **Cierre:** NotifyEnd solicita EndAttackWindow, cierra Active y entra en Recovery. Duplicar End o intentar reabrir no aplica daño. Recovery sigue bloqueando input aunque el cooldown haya terminado.
14. **Interrupción:** FinishCurrentAttack verifica el GUID y concentra toda la limpieza idempotente: Idle, ventana cerrada, GUID/ID invalidados, HitActors vacío, delegates propios retirados, referencias liberadas y modo de root motion restaurado. CancelCurrentAttack detiene la instancia exacta por ID; no usa la ruta del montage para detener otra reproducción. Muerte, EndPlay y OnUnregister usan la misma ruta. No se depende solamente de NotifyEnd.
15. **Delegates:** BlendOut interrumpido limpia inmediatamente; BlendOut normal fuerza Recovery defensivamente y espera End. MontageEnded limpia el final normal. Cada callback lleva el GUID que lo originó. El test J ejecuta copias tardías de ambos delegates del ataque anterior contra una nueva ventana, además de un NotifyEnd viejo: no alteran la ejecución nueva.
16. **Timing:** montage1s, BlendIn0.08s, BlendOut0.10s. Windup0–0.24s; Active0.24–0.44s (0.20s); Recovery0.44s hasta End, aproximadamente1s. Cooldown0.55s desde el inicio; admisión exige también Idle. No hay ataques superpuestos ni solicitudes guardadas.
17. **Boss625 ->525:** test C, test melee anterior H y la integración A del mapa real. El callback DamageReceived de la integración verifica Active, Attack.Light, GUID y pérdida real100. Input/inicio y Windup conservan625. Render PIE registra625 en Windup y525 en Active.
18. **Kill completo:** integración B y PIE completan siete montages:625 ->525 ->425 ->325 ->225 ->125 ->25 ->0. El último impacto aplica25 reales. No se usa KillPrototypeBoss como evidencia de combate.
19. **Reward/Drop:** una recompensa y un pickup, misma definición DA_Item_Vitality; muerte única. PIE camina hasta el overlap: Reward Claimed, Drop Completed, pickup consumido, jugador Base100/Max125/Current125 y un grant. El core universal no cambia.
20. **154 anteriores:** todos conservan sus nombres y objetivos; comparación contra `Saved/Automation/CombatPhase-Final/index.json`: cero nombres ausentes. Los 15 tests melee simulan explícitamente Begin/Tick/End de la ventana en su fixture, y las tres integraciones esperan los notifies reales. No hay fallback de daño inmediato en producción. DamageContext5, input4 y las demás suites conservan sus garantías.
21. **22 nuevos:** A–M cubren inicio sin daño, ventana,625->525, cierre, dedup, ataque posterior, interrupción en Windup/Active/Recovery, limpieza/callbacks tardíos y falta de montage/AnimInstance/fallo de play. N–R cubren entrada tardía, procedencia inválida, muerte, EndPlay y mesh ambigua. V–Y comprueban carga/ruta/segmento/slot, skeleton/componentes reales, exactamente un notify nativo y timing/mezclas.
22. **Total global:** 176 Success,0 succeededWithWarnings,0 failed,0 notRun. Evidencia definitiva: `Saved/Automation/AnimatedAttackPhase-Complete/index.json` y `Saved/Logs/AnimatedAttackPhase-Automation-Complete.log`; proceso salida0. El estado JSON, además de la salida del proceso, confirma el resultado.
23. **Compilación:** SoulsNewEditor Win64 Development con UE5.8.3 mediante UBT/UHT, sin Live Coding: Succeeded/salida0, sin warnings de compilador. Evidencia final: `Saved/Logs/AnimatedAttackPhase-EditorBuild-Complete.log`,35.25s. La primera build de producción procesó los headers reflejados con UHT. No se genera una build Game/Shipping ni un paquete nuevo en esta fase.
24. **Cook Windows:** cook del mapa oficial y Lvl_ThirdPerson,593 paquetes, Success/salida0,0 errores/0 warnings en `Saved/Logs/AnimatedAttackPhase-Cook.log`. Se confirma nuevamente con la build final en `Saved/Logs/AnimatedAttackPhase-Cook-Complete.log`. ReferencedSet.txt y AssetRegistry.bin cocinados contienen AM_Souls_LightAttack, IA_LightAttack, IMC_SoulsCombat y Lvl_SoulsPrototype. El montage referencia SoulsAnimNotifyState_MeleeWindow y el cook carga su clase nativa desde el módulo runtime; una clase C++ no es un .uasset independiente. No se cambia AssetManager/configuración ni se requiere CookDir. Los paquetes archivados anteriores no validan Fase10.
25. **PIE:** mundo real UEDPIE_0_Lvl_SoulsPrototype, player Blueprint poseído. APIs existentes Move/Look/Jump, miss, rechazo de spam, siete ataques completos, drop y recogida caminando. Marcador SOULS_ANIMATED_PIE_VALIDATED en `Saved/Logs/AnimatedAttackPhase-RenderPIE-Complete.log`, salida0 con la build final. Capturas usan D3D12/RenderOffscreen y tiempo fijo60Hz solo en esa invocación, sin cambiar configuración. Las integraciones C++ anteriores ejecutan el montage en tiempo normal con NullRHI; C simula LeftMouseButton a través de Enhanced Input. No se prueban mouse/teclado físicos ni un ejecutable empaquetado de esta fase.
26. **Revisión visual:** se inspeccionaron tres PNG reales de1280×720: `Saved/Diagnostics/AnimatedAttack-Validated-Windup.png`, `AnimatedAttack-Validated-Active.png` y `AnimatedAttack-Validated-Recovery.png`. Quinn cambia de pose al reproducir DefaultSlot frente al boss, con poses de preparación, swing y continuación. Los logs de solicitudes de captura registran posiciones del montage0.0167,0.25 y0.45s; HP625/525/525. Las capturas son asíncronas y no certifican su frame exacto. Esto acredita reproducción visible y correlación de fases/daño, no una evaluación completa de fluidez, balance, hitbox anatómica o presentación final.
27. **Warnings/fallos nuevos:** ninguno en build final,176 suites, cook o PIE renderizado final. Se conservan diagnósticos iniciales: build del harness usó una API privada y carecía de include UPackage; se corrigieron con DispatchQueuedAnimEvents e include. Primera regresión172/176: una ruta esperada omitía Attack y tres tests intentaban avanzar0.56s en un Tick que UWorld limita; se corrigieron ruta y subpasos. La siguiente173/176 precedió esa corrección de reloj; las regresiones finales176/176 incluyen el refuerzo de delegates. La primera captura en tiempo normal produjo un warning de iteraciones de movimiento durante startup y un hitch de captura que saltó la ventana, dejando625; esa ejecución se marca fallida y no es evidencia de impacto. Se repitió con tiempo fijo sin cambiar gameplay ni ocultar logs.
28. **Históricos:** permanecen los14 LogAutomationTest: Error: Condition failed de arranque fuera de las suites, diagnósticos de profiling DLLs y SDKs de otras plataformas; Win64 válido. No son fallos del JSON de SoulsNew. Git advierte futura conversión LF/CRLF; diff --check no encuentra errores. Los mensajes de PSO/shaders al iniciar render son diagnósticos del renderer, no cambios de configuración.
29. **Diff/preservación:**4 C++ nuevos,6 C++ modificados,1 montage nuevo, sección24 de ARCHITECTURE.md y este informe. SHA256 de953 archivos preexistentes:0 ausentes; solo cambian los6 C++ y ARCHITECTURE.md. Todos los paquetes preexistentes y Config permanecen iguales; no hay módulos, plugins, dependencias, mapas, frameworks ni comandos debug nuevos. Logs/scripts/reportes/cook/binarios normales quedan en Saved/Intermediate/Binaries. No se hace commit ni staging.
30. **Fase11 recomendada:** decidir un incremento separado para coste y recuperación básicos de Stamina, manteniendo la admisión de ataque en C++ y probando rechazo por recurso insuficiente. Antes de ampliar combate, revisar manualmente timing del swing y rendimiento; la ventana actual depende de NotifyTick y un frame que salte toda su duración puede perder un impacto. No se añade compensación temporal o barrido de huesos en esta fase. No se implementa Fase11.

## Suites finales

| Suite | Success |
| --- | ---: |
| Health |15 |
| Items |20 |
| Rewards |20 |
| Pickups |21 |
| DropSpawner |23 |
| Prototype |15 |
| DataAssets |9 |
| PrototypeMap |4 |
| DamageContext |5 |
| Melee |15 |
| CombatInput |4 |
| ZCombatMap |3 |
| AnimatedAttack |22 |
| Total |176 |

## Validación manual

1. Abrir SoulsNew.uproject con UE5.8.3 y `/Game/SoulsNew/Maps/Lvl_SoulsPrototype`, pulsar Play y dar foco al viewport. WASD/mouse/Space conservan movimiento, cámara y salto.
2. Status inicial: Player100/base100/grants0 y boss625/base500/grants1. Desde lejos, LeftMouseButton reproduce el swing sin dañar al boss.
3. Acercarse y orientar el cuerpo hacia el cubo. Al pulsar no cambia HP durante Windup; durante0.24–0.44s recibe100 y permanece525 hasta otro ataque.
4. Pulsar rápidamente o mantener el botón: no superpone ataques ni los guarda. Orientarse de espaldas y atacar: no daña al boss. La cámara por sí sola no fija Forward del cuerpo.
5. Completar siete ataques hasta0, caminar sobre la esfera y comprobar jugador125, un grant y Reward/Drop completados. No usar KillPrototypeBoss.
6. Para interrupción sin añadir comandos runtime, pausar PIE durante el swing; usar Step para situarse en Windup, Active o Recovery. En la consola de Output Log ejecutar la línea Python siguiente y reanudar PIE. Tras procesar la interrupción debe quedar Idle/ventana cerrada; una solicitud posterior al cooldown puede iniciar otro ataque. Las pruebas G/H/I ya ejercitan Montage_Stop real; J comprueba doble limpieza y callbacks antiguos.

```text
py import unreal; w=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world(); p=unreal.GameplayStatics.get_player_character(w,0); p.get_component_by_class(unreal.SkeletalMeshComponent).get_anim_instance().montage_stop(0.0,unreal.load_asset('/Game/SoulsNew/Animations/AM_Souls_LightAttack'))
```

**Stamina, input buffering, combos and lock-on are not implemented yet.** Tampoco se implementan dodge/i-frames, parry, heavy/charged, armas/equipment, ataques del boss/AI, poise/stagger, GAS, hitstop/camera shake, trails/VFX/SFX/damage numbers. El sweep sigue siendo frontal, sin sockets, equipos u oclusión adicional. Exige una única mesh válida y skeleton idéntico; no introduce retargeting ni selección explícita entre varias meshes.

## Comandos reproducibles

```powershell
& 'G:/UE_5.8/Engine/Build/BatchFiles/Build.bat' SoulsNewEditor Win64 Development '-Project=G:/UnrealProjects/ProjectSoulsRogue/SoulsNew/SoulsNew.uproject' -WaitMutex -NoHotReloadFromIDE -NoPCH -NoUBA -MaxParallelActions=1
```

```text
UnrealEditor-Cmd.exe SoulsNew.uproject /Engine/Maps/Entry -ExecCmds="Automation RunTests SoulsNew." -TestExit="Automation Test Queue Empty" -ReportExportPath=G:/UnrealProjects/ProjectSoulsRogue/SoulsNew/Saved/Automation/AnimatedAttackPhase-Complete -nullrhi -nosound -unattended -nop4 -NoLiveCoding -NoAutoSave
UnrealEditor-Cmd.exe SoulsNew.uproject -run=Cook -TargetPlatform=Windows -Map=/Game/SoulsNew/Maps/Lvl_SoulsPrototype+/Game/ThirdPerson/Lvl_ThirdPerson -forcerecook=false -nullrhi -nosound -unattended -nop4 -NoLiveCoding
UnrealEditor.exe SoulsNew.uproject /Engine/Maps/Entry -ExecCmds="py G:/UnrealProjects/ProjectSoulsRogue/SoulsNew/Saved/Diagnostics/ValidateAnimatedCombatRender-Validated.py" -RenderOffscreen -UseFixedTimeStep -FPS=60 -nosound -unattended -nop4 -NoLiveCoding -NoAutoSave
```

Se ejecutan desde `G:/UE_5.8/Engine/Binaries/Win64` con rutas absolutas al proyecto/logs y espera hasta la salida del proceso. Los scripts de diagnóstico restauran la preferencia temporal de throttle, cierran PIE y salen del editor sin guardar mapas/configuración.
