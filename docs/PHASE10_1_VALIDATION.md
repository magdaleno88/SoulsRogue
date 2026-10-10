# Fase 10.1 — robustez temporal de Attack Window

Validación local: 2026-10-10, Unreal Engine 5.8.3. Incremento autorizado exclusivamente para el ataque de Fase 10. **190/190 Success**, build Editor correcta y dos sesiones PIE completas. No se implementa Fase 11.

## Entrega de los veinte puntos solicitados

1. **Causa:** Advance procesa Begin/End por substeps, pero Tick de los Notify States activos ocurre al final de toda la actualización. Al cruzar 0.24–0.44 en un único avance, End elimina el estado antes del Tick. Antes de corregir, el test B avanzó realmente 0.60s: Recovery, boss625, cero eventos; falló exactamente su expectativa de100 de daño.
2. **Solución:** una evaluación pendiente en el End natural del notify, dentro de Active y antes de Recovery. Se verifica instancia, dirección del avance, posiciones nativas y ausencia de evaluación previa.
3. **Alternativas:** cambiar solo Montage Tick Type no añade un Tick entre Begin/End: el notify ya era branching point nativo. Procesar en cada Begin/cierre sin condiciones atribuiría presencia histórica a targets actuales. Un timer separado o subdivisor temporal propio duplicaría la autoridad del montage. Se conserva el mecanismo nativo y se restringe la evaluación adicional.
4. **APIs nativas:** bIsNativeBranchingPoint, payload MontageInstanceID/bReachedEnd, FAnimMontageInstance.GetPreviousPosition/GetPosition/GetPlayRate/IsStopped y UAnimNotifyLibrary.NotifyStateReachedEnd. El tipo serializado Queued no invalida el branching point nativo efectivo; se conserva el asset.
5. **Catch-up:** sí, como máximo una evaluación de la ventana sin Tick, solamente en End natural. No hay reloj, timer ni Tick nuevo del componente.
6. **Doble hit:** un único HitActors del ataque cubre ticks, recuperación y cierre. Reserva antes de ApplyDamage/notificaciones. Un Tick que falló también cuenta como evaluación y no se repite al cerrar.
7. **Salto completo:** se vuelve a consultar el sweep actual; solo cuerpos candidatos al inicio, con el mismo Health/transform/bounds y sin movimiento detectado, son elegibles. La muestra inicial no aplica daño ni reserva hits. Late target, salida antes de Active, salida y regreso, o propietario móvil se excluyen de catch-up. Los ticks normales conservan su evaluación actual.
8. **Interrupciones:** antes de Begin no hay catch-up; durante Active solo permanecen daños confirmados. End forzado/instancia detenida no recuperan daño. Finish/Cancel limpian flags, posición, muestras, suscripciones de transformación y HitActors junto al GUID/instancia anteriores.
9. **PlayRate:** 1 y 3 comprobados. Delta0.20 × Rate3 recorre0.60 del montage, produce625->525 y termina Idle. El cooldown de World0.55s permanece; acabar más rápido no permite omitirlo.
10. **Archivos:** modificados Combat/SoulsMeleeCombatComponent.{h,cpp}, Combat/Animation/SoulsAnimNotifyState_MeleeWindow.cpp y docs/ARCHITECTURE.md. Nuevos Tests/SoulsTemporalWindowTests.cpp y esta nota. Los paths de código parten de Source/SoulsNew/. Scripts, logs, JSON y capturas diagnósticos se conservan en Saved, ignorado por Git.
11. **Assets:** ninguno. SHA256 conserva los826 archivos de Content, montage, fuente de animación, mapas, Blueprints, input y Variant_Combat. Config, descriptor, AGENTS y los archivos previos de tests también permanecen iguales; cero archivos ausentes de la muestra inicial de961.
12. **Tests nuevos:** catorce SoulsNew.TemporalWindow.A–N, todos Success; detalle abajo.
13. **Tests anteriores:**176 Success. Comparación por fullTestPath con AnimatedAttackPhase-Complete: cero nombres ausentes; no se modifican los tests/fixture anteriores.
14. **Total:**190 Success,0 succeededWithWarnings,0 failed,0 notRun. Duración final de tests11.055s; proceso salida0. Se inspeccionó el JSON, sin usar la salida del proceso como sustituto de resultados.
15. **Build:** SoulsNewEditor Win64 Development mediante Build.bat/UBT/UHT de UE5.8.3, -WaitMutex -NoHotReloadFromIDE -NoPCH -NoUBA -MaxParallelActions=1. Build final96.39s Succeeded, salida0; sin Live Coding ni warnings de compilación. Las últimas ediciones sitúan las variables de dibujo exclusivamente dentro del bloque debug, evitando variables sin uso en Shipping, y aplican el prefijo Souls al tipo privado de muestra; no cambian el comportamiento probado en PIE.
16. **Cook:** no aplica la condición solicitada: AM_Souls_LightAttack no fue modificado. No se ejecutó cook, package ni build Game/Shipping en esta fase; no se presentan como evidencia nueva los paquetes anteriores.
17. **PIE:** mapa oficial /Game/SoulsNew/Maps/Lvl_SoulsPrototype, Player poseído real. La sesión D3D12 RenderOffscreen a paso60Hz verificó montage y capturas Windup/Active/Recovery, primer daño a posición0.25,625->525; siete ataques hasta0, una Reward con la definición original, un pickup y overlap caminando, Player125/base100/grants1, Claimed/Completed. La sesión NullRHI diagnóstica puso temporalmente t.OverrideFPS5 y PlayRate3: un frame avanzó0->0.600000 y produjo525 en Recovery. Restauró OverrideFPS0 inmediatamente y completó los seis ataques restantes y la recogida. Solo scripts de Saved; sin comandos de gameplay nuevos ni cambios guardados de preferencias/configuración.
18. **Warnings:** cero warnings nuevos de build/suites/PIE finales. Continúan los14 Condition failed de arranque y UnifiedErrorTest previos fuera de las suites, y SDKs de otras plataformas sin configurar. Automation también informó presión estimada de memoria de SkinnedAsset como Display, sin fallo. Git advierte futura conversión LF/CRLF. Durante preparación hubo un linker fallido al consultar un contexto de End cuyo TypeName no está exportado: se sustituyó por NotifyStateReachedEnd pública. El primer harness PIE acelerado asumía que Idle implicaba cooldown vencido; se corrigió la espera, sin cambiar gameplay. Ambos intentos y la reproducción fallida anterior a la corrección permanecen en Saved.
19. **Limitaciones:** catch-up conservador puede descartar impactos visuales con movimiento. Las muestras/eventos no reconstruyen colisión histórica ni detectan todo cambio intermedio de geometría que no emita transformación y vuelva a los mismos bounds. Presupone cuerpos estables; sin rewind. No se prueban saltos deliberados de sección, reproducción inversa, networking, teclado físico, rendimiento extremo ni una build empaquetada nueva. Las capturas validan poses/fase aproximada del prototipo, no contacto de un arma/socket ni balance final.
20. **Fase 11:** revisar manualmente el timing visual y aceptar la semántica conservadora antes de ampliar el combate. Tras autorización separada, abordar stamina mínima en C++; no implementada aquí.

## Pruebas nuevas

Los avances usan SkeletalMeshComponent.TickAnimation y AnimInstance.DispatchQueuedAnimEvents públicos, con el montage real; no sleeps ni llamadas falsas de Notify para provocar el salto.

| Test | Resultado comprobado |
| --- | --- |
| A.NormalWindowStillWorks | Windup sin daño; Tick nativo625->525; Idle |
| B.FullWindowSkipped | Un avance0.60 cruza toda la ventana; un evento100 con GUID actual y estado Active |
| C.SkipDoesNotDoubleHit | Tick, evaluación repetida y cierre conservan525 |
| D.SkipWithoutTarget | Boss fuera de alcance conserva625; Idle |
| E.InterruptedBeforeWindowNoCatchUp | Interrupción real antes de Begin conserva625 |
| F.HighPlayRate | Rate3/Delta0.20,525, Recovery e Idle |
| G.Cleanup | Idle, ventana cerrada, GUID inválido; siguiente catch-up llega425 |
| H.NextAttackWorks | Nuevo GUID; siguiente Tick normal llega425 |
| I.ExistingSingleHitContract | Múltiples evaluaciones normales suman solo100 |
| J.WindupOnlyTargetExcluded | Target que salió antes de Active conserva625 |
| K.LateTargetConservativelyExcluded | Target ausente al iniciar conserva625 al entrar tarde |
| L.InterruptedInsideWindowNoExtraHit | Solo daño previo confirmado;525 e Idle |
| M.MovingOwnerConservativelyExcluded | No se reconstruye volumen del atacante desplazado |
| N.OutAndBackTargetExcluded | Salir y volver invalida elegibilidad; próximo ataque fresco sí golpea |

## Evidencias conservadas

- [Causa reproducida antes del cambio](../Saved/Automation/TemporalWindowPhase-Before/index.json) y [log](../Saved/Logs/TemporalWindowPhase-Before.log).
- [Build final](../Saved/Logs/TemporalWindowPhase-EditorBuild-Final.log).
- [Resultados finales190/190](../Saved/Automation/TemporalWindowPhase-Final/index.json) y [log completo final](../Saved/Logs/TemporalWindowPhase-Automation-Final.log).
- [PIE normal/render](../Saved/Logs/TemporalWindowPhase-PIE-Normal.log): SOULS_TEMPORAL_NORMAL_PIE_VALIDATED.
- [Windup](../Saved/Diagnostics/TemporalWindow-Normal-Windup.png), [Active](../Saved/Diagnostics/TemporalWindow-Normal-Active.png), [Recovery](../Saved/Diagnostics/TemporalWindow-Normal-Recovery.png), inspeccionadas visualmente.
- [PIE salto controlado final](../Saved/Logs/TemporalWindowPhase-PIE-Skip-Validated.log): SOULS_TEMPORAL_SKIP_RECOVERED y SOULS_TEMPORAL_SKIP_PIE_VALIDATED.
- [Auditoría SHA256](../Saved/Diagnostics/TemporalWindowPhase-FinalAudit.json).

Fuente local inspeccionada, solo lectura: G:/UE_5.8/Engine/Source/Runtime/Engine/Private/Animation/AnimMontage.cpp, Advance/BranchingPointEventHandler/Terminate; Public/Animation/AnimNotifyLibrary.h y Classes/Animation/AnimNotifies/AnimNotify.h. La decisión y semántica se incorporan en [ARCHITECTURE, sección25](ARCHITECTURE.md#25-fase-101-robustez-temporal-de-la-ventana-existente), con enlaces a la documentación primaria de Epic.

