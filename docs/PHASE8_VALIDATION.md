# Fase 8 — mapa de prototipo SoulsNew

Fecha: 10 de octubre de 2026 (America/Mexico_City).

Mapa oficial: `/Game/SoulsNew/Maps/Lvl_SoulsPrototype`.

La escena usa una plataforma de 30 × 20 m, con superficie a Z=0; PlayerStart en (-500,0,100), orientado hacia +X; un ASoulsPrototypeBoss en (500,350,100), DirectionalLight y SkyLight. El desplazamiento lateral permite verlo a un lado del personaje desde la cámara inicial. El boss conserva `/Game/SoulsNew/Items/DA_Item_Vitality`. El pickup existente aparece en (660,350,45) tras su muerte. No hay nuevos Blueprints, definiciones, UI ni combate.

World Settings establece `BP_ThirdPersonGameMode`, cuyo DefaultPawnClass es `BP_ThirdPersonCharacter` (ASoulsNewCharacter) y PlayerControllerClass es `BP_ThirdPersonPlayerController`. Se mantienen las rutas y configuración globales anteriores.

## Validación manual en Editor

1. Abrir SoulsNew.uproject con UE 5.8.3.
2. En Content Browser, abrir `Content/SoulsNew/Maps/Lvl_SoulsPrototype` y pulsar Play.
3. Comprobar caminar, cámara y salto con los controles ThirdPerson existentes. El cubo está delante del jugador, a unos 10 m del PlayerStart.
4. Ejecutar `Souls.Debug.Status` en la consola o en Output Log durante PIE: Player 100/100, Base=100, Grants=0; Boss 625/625, Base=500, Grants=1.
5. Ejecutar `Souls.Debug.KillPrototypeBoss`: el cubo se oculta y aparece una esfera a su lado. Status debe indicar Boss Dead, Reward Available y DropState PickupActive.
6. Caminar sobre la esfera. Desaparece; Status debe mostrar Player 125/125, Base=100, Grants=1; Reward Claimed; DropState Completed.
7. Repetir Kill: no debe aparecer otra recompensa. Detener PIE.

## Validación manual del paquete Development

Desde PowerShell en la raíz del proyecto:

```powershell
& '.\Saved\Packages\PrototypeMapPhase\Windows\SoulsNew.exe' '/Game/SoulsNew/Maps/Lvl_SoulsPrototype' -windowed -ResX=1280 -ResY=720 -log
```

El argumento abre directamente el mapa cocinado, sin cambiar GameDefaultMap. El paquete incluye también Lvl_ThirdPerson para que su arranque global existente siga siendo resoluble. Si se abre el ejecutable sin argumento, ejecutar `open /Game/SoulsNew/Maps/Lvl_SoulsPrototype` en su consola.

Repetir los pasos 3–7 de la validación de Editor. Si la consola no abre con la tecla del template, iniciar con `-ExecCmds="Souls.Debug.Status"` para comprobar el estado inicial en el log; esto no sustituye la validación manual completa de kill y recogida. Los comandos Souls.Debug están disponibles en Development y excluidos de Shipping.

## Evidencias de ejecución

Los diagnósticos, informes, capturas y paquetes permanecen bajo Saved, ignorado por Git.

| Comprobación | Resultado y evidencia |
| --- | --- |
| Editor Win64 Development | UE 5.8.3, UBT, sin Live Coding; salida 0, sin warnings de compilación. `Saved/Logs/PrototypeMapPhase-EditorBuild-Complete.log` |
| Regresión anterior | Health 15/15; Items 20/20; Rewards 20/20; Pickups 21/21; DropSpawner 23/23; Prototype 15/15; DataAssets 9/9: 123/123 conservados |
| Nuevas pruebas | `SoulsNew.PrototypeMap`: PrototypeMapLoads, ContainsSinglePrototypeBoss, BossUsesRealVitalityAsset, MapPlayableConfiguration: 4/4 |
| Total Editor | 127/127 Success sobre el código final; 0 fallos, 0 warnings/errores en tests, 0 omitidos; salida 0. `Saved/Automation/PrototypeMapPhase-Complete/index.json` y `Saved/Logs/PrototypeMapPhase-Automation-Complete.log` |
| PIE final | Mapa guardado real y código final; spawn mediante PlayerStart/GameMode, movimiento/look/salto mediante APIs existentes; boss 625/base500, kill por consola, recompensa/pickup de la misma definición; caminar con DoMove hasta el overlap físico, Player125/base100/grants1, Reward Claimed y Drop Completed; segundo kill sin duplicación. Salida 0. `Saved/Logs/PrototypeMapPhase-PIE-FinalValidated.log`, marcador `SOULS_MAP_PIE_VALIDATED` |
| Game Win64 Development | Compilación final de la corrección mediante UBT, salida 0, sin warnings de compilación; registro de build en `Saved/Logs/PrototypeMapPhase-Packaging-Final.log` |
| Cook Windows | Cook completo inicial: salida 0, 0 errores/0 warnings, 350.78 s. Cook incremental del mapa con posición final: salida 0, 0 errores/0 warnings, 88.63 s. `Saved/Logs/PrototypeMapPhase-Packaging.log` y `PrototypeMapPhase-Packaging-Complete.log` |
| Packaging final | BuildCookRun, stage, pak/IoStore, package y archive: BUILD SUCCESSFUL, salida 0, 136.79 s. `Saved/Logs/PrototypeMapPhase-Packaging-Complete.log`; paquete `Saved/Packages/PrototypeMapPhase/Windows` |
| Binario distribuido | SHA256 del ejecutable archivado y Binaries/Win64/SoulsNew.exe idénticos: `D8160B049967B799F720B2A019C67430449883842912ACFCC4F9B906445FBFD5`. La carga asíncrona normal permanece habilitada |
| Tests en paquete | Las cuatro pruebas de mapa pasan sobre el contenido cocinado; salida 0. `Saved/Logs/PrototypeMapPhase-PackagedFlow-Complete.log` y `PrototypeMapPhase-PackagedDropRender.log` |
| Loop empaquetado | Estado inicial Player100/Boss625; Kill -> mismo asset, Reward Available y PickupActive; BugItGo al pickup seguido de Walk restaura colisión y produce overlap real -> Player125, un grant, Claimed, Completed. Segundo Kill sin otro drop. Salida 0. `Saved/Logs/PrototypeMapPhase-PackagedFlow-Complete.log` |
| Render empaquetado | D3D12 real, render fuera de pantalla: jugador, suelo y boss visibles en `Saved/Diagnostics/PrototypeMapPhase-Final.png`; boss oculto y pickup visible tras Kill en `PrototypeMapPhase-Drop.png`. Capturas inspeccionadas; salida 0. `Saved/Logs/PrototypeMapPhase-PackagedRender-Complete.log` y `PrototypeMapPhase-PackagedDropRender.log` |
| Entrada física / Shipping | No se verifican teclado/ratón físicos ni recorrido manual en el paquete. No se vuelve a compilar Shipping en esta fase. Las instrucciones anteriores permiten completar la validación manual |

PIE utilizó NullRHI. Acredita locomoción, control de cámara y salto mediante las APIs existentes y un controller poseído; no acredita render ni teclado/ratón físicos. La prueba camina hacia el pickup, sin teletransportar el personaje ni invocar Claim manualmente.

La prueba del paquete utiliza teletransporte diagnóstico y un overlap real; no acredita caminar mediante teclado. El render se valida por separado con D3D12 sobre ese mismo paquete. No se invoca Claim directamente en ninguno de esos recorridos.

## Conservación y diagnósticos

Al comenzar ya existían, sin seguimiento, Lvl_SoulsPrototype como copia de la arena, NewWorld.umap y 67 paquetes externos de SoulsNew. Lvl_ThirdPerson.umap también estaba modificado previamente. Se preserva ese estado del template: SHA256 `D1C2AD3B4D341BEA4849C171C0E85DF8940E3E4464E4E44789BE859AED8C145B` idéntico antes/después de la autoría del prototipo.

El usuario autorizó sustituir el contenido del mapa de prototipo por una plataforma mínima. `save_map` sobre el nombre de la copia World Partition retiró automáticamente sus 67 paquetes externos, generando 134 warnings de AssetRegistry en esa invocación. Se restauraron inmediatamente desde los objetos locales LFS, con SHA256 idéntico para cada archivo; se conservaron también mapa previo y paquetes completos en `Saved/Diagnostics/PrototypeMapPhase-Original`. Los paquetes externos antiguos ya no son dependencias del mapa convencional nuevo. No se borraron archivos del template, NewWorld ni contenido ajeno.

La primera regresión dio 126 Success y un fallo previo de DefinitionGCLifetime: el fixture nuevo cargaba un mundo inactivo cuyos subsistemas de editor quedaban inicializados al liberarlo. Se corrigió exclusivamente el fixture para llamar CleanupWorld antes de liberar un mundo inactivo inicializado; no afecta mundos de Editor/Game/PIE activos. Recompilación y regresión completas posteriores dieron 127/127. Los informes anteriores permanecen disponibles para diagnóstico.

Fuera de los tests siguen los 14 Condition failed de arranque y el warning histórico de layout. PIE registró además un timeout HTTP de la comprobación de conectividad `generate_204`; no interrumpió el flujo ni se modificó gameplay para ocultarlo. No hay warnings en la compilación final ni en los tests finales.

La primera ejecución del paquete detectó un fallo real al cargar el boss serializado: su constructor llamaba SetBaseMaxHealth desde FAsyncLoadingThread. La revisión encontró también SetSpawnOffset con la misma restricción. Ambas configuraciones pasan a OnConstruction para instancias creadas/editor y a BeginPlay para actores cargados, antes de Ready y de conceder Vitalidad. Los constructores no llaman esas APIs runtime; Health y DropSpawner conservan sus validaciones, lógica, eventos y autoridad. Se volvió a compilar y empaquetar sin desactivar la carga asíncrona ni cambiar configuración global. La corrección reutilizó el cook porque no modificaba assets ni APIs reflejadas; el ajuste lateral posterior se cocina de nuevo de forma incremental.

El primer packaging tuvo tres reintentos SafeCopyFile al coincidir con la actualización del binario Game; terminó BUILD SUCCESSFUL y el SHA256 archivado coincidió con el binario final. El packaging posterior serializa build/stage y no contiene esas excepciones. Un intento de diagnóstico con BugItGo dejó al personaje en Ghost, por lo que no concedió el objeto; no se cuenta como recogida validada. Se conserva el log y se restaura Walk en el diagnóstico posterior.

El primer recorrido lateral de PIE agotó el timeout del harness. El diagnóstico final dirige los ejes Right/Forward hacia el pickup y evita la limitación de ejecución del editor en segundo plano solo durante la sesión; restaura el valor original y no guarda ajustes. Dos intentos previos de acceso al ajuste del editor fallaron por ausencia de wrapper Python/nombre transformado; el nombre C++ reflejado funcionó. El recorrido final camina, valida todos los estados y sale con 0. Estos intentos afectan al harness en Saved, no al gameplay.

Los renders finales no registran warnings/errores. Un render previo, antes del ajuste de visibilidad, emitió tres avisos de espera de precarga de shaders. El diagnóstico empaquetado de nueve comandos registra una advertencia de TickDeferredCommands por 46.96 ms, sin error de juego ni fallo de los cuatro tests. Cook, packaging final y compilaciones finales no tienen warnings nuevos. Git conserva el aviso de conversión futura LF/CRLF.

La auditoría final contra los hashes iniciales no encuentra archivos ausentes. Solo cambian PrototypeBoss.cpp, Lvl_SoulsPrototype.umap y la sección añadida a ARCHITECTURE.md; se añaden el archivo de cuatro tests y este informe. Se conservan todos los paquetes externos iniciales, los 753 paquetes del template, DA_Item_Vitality, Config, AGENTS.md, descriptor y el código restante. `git diff --check` pasó. Evidencia: `Saved/Diagnostics/PrototypeMapPhase-FinalAudit.json`.

`.gitignore` ya contiene `Build/*/FileOpenOrder/`; se conserva sin cambios. Saved, Binaries, Intermediate, DerivedDataCache, logs de cook, paquetes y FileOpenOrder quedan ignorados. No se añadió ningún archivo al índice de Git.

## Alcance de la entrega

| Elemento solicitado | Entrega |
| --- | --- |
| Assets creados/modificados | Se reemplazó el contenido del Lvl_SoulsPrototype preexistente sin seguimiento por la escena mínima mediante Unreal Editor. No hay nuevas rutas de Data Assets, materiales o Blueprints. DA_Item_Vitality y template quedan intactos respecto al inicio de la sesión |
| Código | Nuevo `Source/SoulsNew/Tests/SoulsPrototypeMapTests.cpp`; cambio puntual en `Characters/SoulsPrototypeBoss.cpp` para inicialización segura durante carga asíncrona; no cambios en Health, Items, Rewards, Pickup, DropSpawner o Character |
| Ruta del mapa | `/Game/SoulsNew/Maps/Lvl_SoulsPrototype` |
| GameMode / Player / Controller | BP_ThirdPersonGameMode / BP_ThirdPersonCharacter (ASoulsNewCharacter) / BP_ThirdPersonPlayerController; referencias originales |
| Boss / ItemDefinition | Exactamente un ASoulsPrototypeBoss; la definición persistente existente DA_Item_Vitality |
| Inicio del Player | 100/100, Base=100, Grants=0 |
| Inicio del boss | 625/625, Base=500, Grants=1 |
| Después de Kill | Boss 0/625, Dead; Reward Available; Drop PickupActive; esfera materializada automáticamente |
| Después del pickup | Player125/125, Base=100, Grants=1; Reward Claimed; Drop Completed; pickup ausente |
| Tests | 123 anteriores + 4 nuevos = 127; la ejecución final se acredita en la tabla de evidencias |
| Documentación | Se añade únicamente la sección 22 de ARCHITECTURE.md y este informe |
| Git relevante | Cambios propios: mapa de prototipo, inicialización de PrototypeBoss, cuatro tests y documentación. Siguen aparte los cambios iniciales de Lvl_ThirdPerson, NewWorld.umap y paquetes externos de SoulsNew. No commit, staging ni cambios de ignores |
| Generados ignorados | Scripts de autoría/PIE, backups, SHA256, logs, informes, cook/stage/paquete y capturas en Saved; binarios/intermedios/cachés y Build/*/FileOpenOrder |
| Siguiente bloque sugerido | Autorizar primero un incremento pequeño de daño con contexto e impactos confirmados para una Light Attack; fijar detección, deduplicación por ataque y limpieza de interrupciones. Mantener objetos universales y Health como autoridad; decidir GAS explícitamente antes de ampliar habilidades/status effects. No se implementa ese bloque aquí |
